#include "can_recorder.h"

#include <SPIFFS.h>

#include "ct_can_record.h"
#include "ct_time.h"

static const char CAN_RECORD_HEADER[] =
    "timestamp_ms,bus,id,extended,remote,dlc,data_hex\n";

CanRecorder::CanRecorder(CANService& canService)
    : _canService(canService), _storageAvailable(false), _recording(false),
      _busMask(0), _frameCount(0), _droppedFrameCount(0),
      _lastDropCount{0, 0}, _lastFlushMs(0), _lastSpaceCheckMs(0) {}

void CanRecorder::setStorageAvailable(bool available) {
    _storageAvailable = available;
    if (!available && _recording) _fail("Storage became unavailable");
}

bool CanRecorder::start(uint8_t busMask) {
    if (_recording) {
        _lastError = "A recording is already active";
        return false;
    }
    if (!_storageAvailable || SPIFFS.totalBytes() == 0) {
        _lastError = "SPIFFS is unavailable";
        return false;
    }
    if (busMask == 0 || (busMask & (uint8_t)~BUS_MASK_BOTH) != 0) {
        _lastError = "Invalid CAN bus selection";
        return false;
    }
    for (uint8_t bus = 0; bus < 2; ++bus) {
        const uint8_t bit = (uint8_t)(1u << bus);
        if ((busMask & bit) && !_canService.isActive((CanBusId)bus)) {
            _lastError = bus == 0 ? "CAN1 is unavailable" : "CAN2 is unavailable";
            return false;
        }
    }

    const uint32_t total = SPIFFS.totalBytes();
    const uint32_t used = SPIFFS.usedBytes();
    if (used >= total || total - used < MIN_FREE_BYTES + sizeof(CAN_RECORD_HEADER)) {
        _lastError = "Insufficient free SPIFFS space";
        return false;
    }

    char fileName[16];
    bool foundName = false;
    for (uint16_t index = 0; index < MAX_RECORDING_FILES; ++index) {
        snprintf(fileName, sizeof(fileName), "/can%04u.csv", (unsigned)index);
        if (!SPIFFS.exists(fileName)) {
            foundName = true;
            break;
        }
    }
    if (!foundName) {
        _lastError = "No free recording filename";
        return false;
    }

    _file = SPIFFS.open(fileName, FILE_WRITE);
    if (!_file) {
        _lastError = "Could not create recording file";
        return false;
    }
    if (_file.write(reinterpret_cast<const uint8_t*>(CAN_RECORD_HEADER),
                    sizeof(CAN_RECORD_HEADER) - 1u) != sizeof(CAN_RECORD_HEADER) - 1u) {
        _file.close();
        SPIFFS.remove(fileName);
        _lastError = "Could not write recording header";
        return false;
    }

    _busMask = busMask;
    _frameCount = 0;
    _droppedFrameCount = 0;
    _lastError = "";
    _lastFileName = fileName;
    for (uint8_t bus = 0; bus < 2; ++bus) {
        const uint8_t bit = (uint8_t)(1u << bus);
        _lastDropCount[bus] = 0;
        if ((busMask & bit) && !_canService.subscribeRx((CanBusId)bus, CAN_RX_RECORDER)) {
            _canService.unsubscribeRx(CAN_BUS_1, CAN_RX_RECORDER);
            _canService.unsubscribeRx(CAN_BUS_2, CAN_RX_RECORDER);
            _file.close();
            SPIFFS.remove(fileName);
            _busMask = 0;
            _lastError = "Could not subscribe to CAN receive queue";
            return false;
        }
    }

    _recording = true;
    _lastFlushMs = millis();
    _lastSpaceCheckMs = _lastFlushMs;
    _file.flush();
    return true;
}

bool CanRecorder::stop() {
    if (!_recording) return true;
    _closeFile();
    return true;
}

bool CanRecorder::deleteRecording(const char* fileName) {
    if (!_storageAvailable || !SPIFFS.totalBytes()) {
        _lastError = "SPIFFS is unavailable";
        return false;
    }
    if (!ctCanRecordFilenameValid(fileName)) {
        _lastError = "Invalid recording filename";
        return false;
    }
    const String path = String("/") + fileName;
    if (_recording && _lastFileName == path) {
        _lastError = "Stop the active recording before deleting it";
        return false;
    }
    if (!SPIFFS.exists(path) || !SPIFFS.remove(path)) {
        _lastError = "Could not delete recording";
        return false;
    }
    if (_lastFileName == path) _lastFileName = "";
    _lastError = "";
    return true;
}

void CanRecorder::update() {
    if (!_recording) return;

    const uint32_t now = millis();
    if (ctElapsedAtLeast(now, _lastSpaceCheckMs, FREE_SPACE_CHECK_MS)) {
        _lastSpaceCheckMs = now;
        const uint32_t total = SPIFFS.totalBytes();
        const uint32_t used = SPIFFS.usedBytes();
        if (used >= total || total - used < MIN_FREE_BYTES) {
            _fail("Recording stopped to preserve remaining SPIFFS space");
            return;
        }
    }

    for (uint8_t bus = 0; bus < 2; ++bus) {
        const uint8_t bit = (uint8_t)(1u << bus);
        if (!(_busMask & bit)) continue;

        const CanBusId busId = (CanBusId)bus;
        const uint32_t dropped = _canService.getRxDrops(busId, CAN_RX_RECORDER);
        _droppedFrameCount += dropped - _lastDropCount[bus];
        _lastDropCount[bus] = dropped;

        for (uint8_t count = 0; count < 16; ++count) {
            CanRxFrame frame = {};
            if (!_canService.receiveRx(busId, CAN_RX_RECORDER, frame)) break;
            if (!_writeFrame(frame)) return;
        }
    }

    if (ctElapsedAtLeast(now, _lastFlushMs, 1000)) {
        _file.flush();
        _lastFlushMs = now;
    }
}

void CanRecorder::_closeFile() {
    _canService.unsubscribeRx(CAN_BUS_1, CAN_RX_RECORDER);
    _canService.unsubscribeRx(CAN_BUS_2, CAN_RX_RECORDER);
    if (_file) {
        _file.flush();
        _file.close();
    }
    _recording = false;
    _busMask = 0;
}

void CanRecorder::_fail(const char* error) {
    _lastError = error ? error : "Recording failed";
    _closeFile();
}

bool CanRecorder::_writeFrame(const CanRxFrame& frame) {
    char line[64];
    size_t length = 0;
    if (!ctFormatCanRecordLine(frame, line, sizeof(line), length)) {
        _fail("Invalid CAN frame rejected by recorder");
        return false;
    }
    if (!_file || _file.size() + length > MAX_RECORDING_BYTES) {
        _fail("Recording reached the 256 KiB file limit");
        return false;
    }
    if (_file.write(reinterpret_cast<const uint8_t*>(line), length) != length) {
        _fail("Recording write failed");
        return false;
    }
    ++_frameCount;
    return true;
}