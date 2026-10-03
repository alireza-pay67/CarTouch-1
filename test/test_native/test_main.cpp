#include <unity.h>
#include <stdint.h>
#include <string.h>
#include "ct_tx_guard.h"
#include "ct_index_parser.h"
#include "ct_time.h"
#include "ct_hex_parser.h"
#include "ct_obd_parser.h"
#include "ct_dbc_validation.h"
#include "ct_verify.h"
#include "ct_json_validation.h"
#include "ct_battery.h"
#include "ct_can_config.h"
#include "ct_storage_guard.h"
#include "ct_can_record.h"
#include "can_service.h"

void setUp(void) {}
void tearDown(void) {}

class MockCanInterface : public CanInterface {
public:
    bool beginResult = true;
    bool active = false;
    bool listenOnly = true;
    bool lastReconfigure = false;
    uint8_t beginCalls = 0;
    uint8_t sendCalls = 0;
    uint8_t receiveCalls = 0;
    uint8_t reconfigureCalls = 0;
    uint32_t lastTxId = 0;
    bool scriptedRx = false;
    CanMessage rxFrames[64] = {};
    uint8_t rxFrameCount = 0;
    uint8_t rxFrameIndex = 0;

    bool begin() override {
        ++beginCalls;
        active = beginResult;
        return beginResult;
    }
    void end() override { active = false; }
    bool sendMessage(const CanMessage& msg, uint32_t) override {
        ++sendCalls;
        lastTxId = msg.id;
        return active && !listenOnly;
    }
    bool receiveMessage(CanMessage& msg, uint32_t) override {
        ++receiveCalls;
        msg.id = 0x123;
        return active;
    }
    bool receiveMessageNonBlocking(CanMessage& msg) override {
        ++receiveCalls;
        if (scriptedRx) {
            if (!active || rxFrameIndex >= rxFrameCount) return false;
            msg = rxFrames[rxFrameIndex++];
            return true;
        }
        msg.id = 0x123;
        return active;
    }
    void flushRxQueue() override {}
    bool isActive() override { return active; }
    CanError getLastError() override { return CAN_OK; }
    bool recoverFromBusOff() override { return active; }
    void getStats(uint32_t& tx, uint32_t& rx, uint32_t& errors) override {
        tx = rx = errors = 0;
    }
    bool getDiagnostics(CanDiagnostics& out) override {
        memset(&out, 0, sizeof(out));
        out.driverReady = active;
        return active;
    }
    uint32_t getLastRxTime() const override { return 0; }
    bool reconfigureMode(bool mode) override {
        ++reconfigureCalls;
        lastReconfigure = mode;
        listenOnly = mode;
        return active;
    }
    bool isListenOnlyActive() override { return active && listenOnly; }
};

void test_tx_guard_listen_only(void) {
    TEST_ASSERT_EQUAL(CT_TX_ERR_LISTEN_ONLY, ctTxGuard(true, true, 8));
    TEST_ASSERT_EQUAL(CT_TX_ERR_LISTEN_ONLY, ctTxGuard(true, true, 0));
}
void test_tx_guard_initialization_and_dlc(void) {
    TEST_ASSERT_EQUAL(CT_TX_ERR_NOT_INITIALIZED, ctTxGuard(false, false, 8));
    TEST_ASSERT_EQUAL(CT_TX_ERR_LENGTH, ctTxGuard(true, false, 9));
    TEST_ASSERT_EQUAL(CT_TX_OK, ctTxGuard(true, false, 8));
}
void test_tx_id_validity(void) {
    TEST_ASSERT_TRUE(ctTxIdValid(0x7FF, false));
    TEST_ASSERT_FALSE(ctTxIdValid(0x800, false));
    TEST_ASSERT_FALSE(ctTxIdValid(0x1234, false));
    TEST_ASSERT_TRUE(ctTxIdValid(0x1FFFFFFF, true));
    TEST_ASSERT_FALSE(ctTxIdValid(0x20000000, true));
}
void test_battery_voltage_availability(void) {
    TEST_ASSERT_FALSE(ctBatteryVoltageAvailable(0.0f));
    TEST_ASSERT_FALSE(ctBatteryVoltageAvailable(-1.0f));
    TEST_ASSERT_FALSE(ctBatteryVoltageAvailable(5.9f));
    TEST_ASSERT_TRUE(ctBatteryVoltageAvailable(6.0f));
    TEST_ASSERT_TRUE(ctBatteryVoltageAvailable(12.6f));
    TEST_ASSERT_TRUE(ctBatteryVoltageAvailable(36.0f));
    TEST_ASSERT_FALSE(ctBatteryVoltageAvailable(36.1f));
}
void test_can_frame_identity_includes_format(void) {
    TEST_ASSERT_TRUE(ctSameCanFrameId(0x123, false, 0x123, false));
    TEST_ASSERT_TRUE(ctSameCanFrameId(0x123, true, 0x123, true));
    TEST_ASSERT_FALSE(ctSameCanFrameId(0x123, false, 0x123, true));
    TEST_ASSERT_FALSE(ctSameCanFrameId(0x123, true, 0x124, true));
}
void test_can_bus_pin_conflicts_are_rejected(void) {
    TEST_ASSERT_TRUE(ctCanPinsConflictFree(9, 6, 15, 16));
    TEST_ASSERT_FALSE(ctCanPinsConflictFree(15, 6, 15, 16));
    TEST_ASSERT_FALSE(ctCanPinsConflictFree(9, 16, 15, 16));
    TEST_ASSERT_FALSE(ctCanPinsConflictFree(9, 6, 16, 16));
}
void test_mcp2515_supported_bitrates(void) {
    TEST_ASSERT_TRUE(ctMcp2515BitrateValid(100000));
    TEST_ASSERT_TRUE(ctMcp2515BitrateValid(125000));
    TEST_ASSERT_TRUE(ctMcp2515BitrateValid(250000));
    TEST_ASSERT_TRUE(ctMcp2515BitrateValid(500000));
    TEST_ASSERT_TRUE(ctMcp2515BitrateValid(1000000));
    TEST_ASSERT_FALSE(ctMcp2515BitrateValid(800000));
    TEST_ASSERT_FALSE(ctMcp2515BitrateValid(0));
}
void test_partition_layout_must_fit_detected_flash(void) {
    TEST_ASSERT_TRUE(ctPartitionFitsFlash(16u * 1024u * 1024u, 16u * 1024u * 1024u));
    TEST_ASSERT_FALSE(ctPartitionFitsFlash(8u * 1024u * 1024u, 16u * 1024u * 1024u));
    TEST_ASSERT_FALSE(ctPartitionFitsFlash(4u * 1024u * 1024u, 16u * 1024u * 1024u));
    TEST_ASSERT_FALSE(ctPartitionFitsFlash(16u * 1024u * 1024u, 0u));
    TEST_ASSERT_FALSE(ctPartitionFitsFlash(0u, 16u * 1024u * 1024u));
}
void test_filesystem_ota_requires_mounted_empty_user_storage(void) {
    TEST_ASSERT_TRUE(ctFilesystemOtaAllowed(true, false));
    TEST_ASSERT_FALSE(ctFilesystemOtaAllowed(false, false));
    TEST_ASSERT_FALSE(ctFilesystemOtaAllowed(true, true));
    TEST_ASSERT_FALSE(ctFilesystemOtaAllowed(false, true));
}
void test_can_service_initializes_each_bus_independently(void) {
    MockCanInterface can0;
    MockCanInterface can1;
    can0.beginResult = false;
    can1.beginResult = true; // Align with the new contract
    CANService service(can0, can1);
    TEST_ASSERT_TRUE(service.begin());
    TEST_ASSERT_EQUAL(1, can0.beginCalls);
    TEST_ASSERT_EQUAL(1, can1.beginCalls);
    TEST_ASSERT_FALSE(service.isActive(CAN_BUS_1));
    TEST_ASSERT_TRUE(service.isActive(CAN_BUS_2));
}
void test_can_service_starts_with_can1_only(void) {
    MockCanInterface can1;
    MockCanInterface can2;
    can1.beginResult = true;
    can2.beginResult = false;
    CANService service(can1, can2);
    TEST_ASSERT_TRUE(service.begin());
    TEST_ASSERT_TRUE(service.isActive(CAN_BUS_1));
    TEST_ASSERT_FALSE(service.isActive(CAN_BUS_2));
}
void test_can_service_reports_unavailable_when_both_fail(void) {
    MockCanInterface can1;
    MockCanInterface can2;
    can1.beginResult = false;
    can2.beginResult = false;
    CANService service(can1, can2);
    TEST_ASSERT_FALSE(service.begin());
    TEST_ASSERT_EQUAL(1, can1.beginCalls);
    TEST_ASSERT_EQUAL(1, can2.beginCalls);
}
void test_can_service_routes_legacy_calls_to_can1(void) {
    MockCanInterface can0;
    MockCanInterface can1;
    can0.active = can1.active = true;
    can0.listenOnly = can1.listenOnly = false;
    CANService service(can0, can1);
    CanMessage message = {};
    message.id = 0x321;
    TEST_ASSERT_TRUE(service.sendMessage(message));
    TEST_ASSERT_EQUAL(1, can0.sendCalls);
    TEST_ASSERT_EQUAL(0, can1.sendCalls);
    TEST_ASSERT_TRUE(service.sendMessage(CAN_BUS_1, message));
    TEST_ASSERT_EQUAL(2, can0.sendCalls);
    TEST_ASSERT_EQUAL(0, can1.sendCalls);
    TEST_ASSERT_TRUE(service.sendMessage(CAN_BUS_2, message));
    TEST_ASSERT_EQUAL(2, can0.sendCalls);
    TEST_ASSERT_EQUAL(1, can1.sendCalls);
}
void test_can_service_does_not_fallback_from_selected_inactive_bus(void) {
    MockCanInterface can1;
    MockCanInterface can2;
    can1.active = true;
    can1.listenOnly = false;
    can2.active = false;
    CANService service(can1, can2);
    CanMessage message = {};
    message.id = 0x321;

    TEST_ASSERT_FALSE(service.sendMessage(CAN_BUS_2, message));
    TEST_ASSERT_EQUAL(0, can1.sendCalls);
    TEST_ASSERT_EQUAL(1, can2.sendCalls);
}

void test_can_service_fans_out_frames_to_independent_subscribers(void) {
    MockCanInterface can1;
    MockCanInterface can2;
    can1.active = true;
    can1.scriptedRx = true;
    can1.rxFrameCount = 1;
    can1.rxFrames[0] = {};
    can1.rxFrames[0].id = 0x456;
    can1.rxFrames[0].length = 2;
    can1.rxFrames[0].data[0] = 0x12;
    can1.rxFrames[0].data[1] = 0x34;
    CANService service(can1, can2);
    TEST_ASSERT_TRUE(service.subscribeRx(CAN_BUS_1, CAN_RX_OBD));
    TEST_ASSERT_TRUE(service.subscribeRx(CAN_BUS_1, CAN_RX_LEARN));
    TEST_ASSERT_EQUAL(1, service.pumpRx());

    CanRxFrame obdFrame = {};
    CanRxFrame learnFrame = {};
    TEST_ASSERT_TRUE(service.receiveRx(CAN_BUS_1, CAN_RX_OBD, obdFrame));
    TEST_ASSERT_TRUE(service.receiveRx(CAN_BUS_1, CAN_RX_LEARN, learnFrame));
    TEST_ASSERT_EQUAL(0x456, obdFrame.message.id);
    TEST_ASSERT_EQUAL(0x456, learnFrame.message.id);
    TEST_ASSERT_EQUAL(0x12, obdFrame.message.data[0]);
    TEST_ASSERT_EQUAL(0x34, learnFrame.message.data[1]);
    TEST_ASSERT_FALSE(service.receiveRx(CAN_BUS_1, CAN_RX_OBD, obdFrame));
    TEST_ASSERT_FALSE(service.receiveRx(CAN_BUS_1, CAN_RX_LEARN, learnFrame));
}

void test_can_service_pumps_both_buses_independently(void) {
    MockCanInterface can1;
    MockCanInterface can2;
    can1.active = can2.active = true;
    can1.scriptedRx = can2.scriptedRx = true;
    can1.rxFrameCount = can2.rxFrameCount = 2;
    can1.rxFrames[0].id = 0x111;
    can1.rxFrames[1].id = 0x112;
    can2.rxFrames[0].id = 0x221;
    can2.rxFrames[1].id = 0x222;

    CANService service(can1, can2);
    TEST_ASSERT_TRUE(service.subscribeRx(CAN_BUS_1, CAN_RX_MONITOR));
    TEST_ASSERT_TRUE(service.subscribeRx(CAN_BUS_2, CAN_RX_MONITOR));
    TEST_ASSERT_EQUAL(4, service.pumpRx(2));

    CanRxFrame frame = {};
    TEST_ASSERT_TRUE(service.receiveRx(CAN_BUS_1, CAN_RX_MONITOR, frame));
    TEST_ASSERT_EQUAL(CAN_BUS_1, frame.bus);
    TEST_ASSERT_EQUAL_HEX32(0x111, frame.message.id);
    TEST_ASSERT_TRUE(service.receiveRx(CAN_BUS_1, CAN_RX_MONITOR, frame));
    TEST_ASSERT_EQUAL_HEX32(0x112, frame.message.id);
    TEST_ASSERT_FALSE(service.receiveRx(CAN_BUS_1, CAN_RX_MONITOR, frame));

    TEST_ASSERT_TRUE(service.receiveRx(CAN_BUS_2, CAN_RX_MONITOR, frame));
    TEST_ASSERT_EQUAL(CAN_BUS_2, frame.bus);
    TEST_ASSERT_EQUAL_HEX32(0x221, frame.message.id);
    TEST_ASSERT_TRUE(service.receiveRx(CAN_BUS_2, CAN_RX_MONITOR, frame));
    TEST_ASSERT_EQUAL_HEX32(0x222, frame.message.id);
    TEST_ASSERT_FALSE(service.receiveRx(CAN_BUS_2, CAN_RX_MONITOR, frame));
}

void test_can_service_fans_out_to_monitor_and_recorder(void) {
    MockCanInterface can1;
    MockCanInterface can2;
    can1.active = true;
    can1.scriptedRx = true;
    can1.rxFrameCount = 1;
    can1.rxFrames[0].id = 0x456;
    can1.rxFrames[0].length = 2;
    can1.rxFrames[0].data[0] = 0xAB;
    can1.rxFrames[0].data[1] = 0xCD;

    CANService service(can1, can2);
    TEST_ASSERT_TRUE(service.subscribeRx(CAN_BUS_1, CAN_RX_MONITOR));
    TEST_ASSERT_TRUE(service.subscribeRx(CAN_BUS_1, CAN_RX_RECORDER));
    TEST_ASSERT_EQUAL(1, service.pumpRx());

    CanRxFrame monitorFrame = {};
    CanRxFrame recordFrame = {};
    TEST_ASSERT_TRUE(service.receiveRx(CAN_BUS_1, CAN_RX_MONITOR, monitorFrame));
    TEST_ASSERT_TRUE(service.receiveRx(CAN_BUS_1, CAN_RX_RECORDER, recordFrame));
    TEST_ASSERT_EQUAL_HEX32(0x456, monitorFrame.message.id);
    TEST_ASSERT_EQUAL_HEX32(monitorFrame.message.id, recordFrame.message.id);
    TEST_ASSERT_EQUAL_HEX8(0xAB, recordFrame.message.data[0]);
    TEST_ASSERT_EQUAL_HEX8(0xCD, recordFrame.message.data[1]);
}

void test_can_record_format_preserves_channel_and_payload(void) {
    CanRxFrame frame = {};
    frame.bus = CAN_BUS_2;
    frame.receivedAtMs = 123;
    frame.message.id = 0x123;
    frame.message.isExtended = true;
    frame.message.length = 2;
    frame.message.data[0] = 0xA1;
    frame.message.data[1] = 0xB2;

    char line[64] = {};
    size_t written = 0;
    TEST_ASSERT_TRUE(ctFormatCanRecordLine(frame, line, sizeof(line), written));
    TEST_ASSERT_EQUAL_STRING("123,CAN2,00000123,1,0,2,A1B2\n", line);
    TEST_ASSERT_EQUAL(strlen(line), written);
}

void test_can_record_format_rejects_invalid_or_truncated_frames(void) {
    CanRxFrame frame = {};
    frame.bus = CAN_BUS_1;
    frame.message.length = 9;
    char line[64] = {};
    size_t written = 0;
    TEST_ASSERT_FALSE(ctFormatCanRecordLine(frame, line, sizeof(line), written));

    frame.message.length = 0;
    char tiny[4] = {};
    TEST_ASSERT_FALSE(ctFormatCanRecordLine(frame, tiny, sizeof(tiny), written));
}

void test_can_record_filename_allowlist(void) {
    TEST_ASSERT_TRUE(ctCanRecordFilenameValid("can0000.csv"));
    TEST_ASSERT_TRUE(ctCanRecordFilenameValid("can9999.csv"));
    TEST_ASSERT_FALSE(ctCanRecordFilenameValid("/can0000.csv"));
    TEST_ASSERT_FALSE(ctCanRecordFilenameValid("can00x0.csv"));
    TEST_ASSERT_FALSE(ctCanRecordFilenameValid("can0000.csv/../config"));
    TEST_ASSERT_FALSE(ctCanRecordFilenameValid("custom.csv"));
}

void test_can_service_bounds_consumer_queues_and_counts_drops(void) {
    MockCanInterface can1;
    MockCanInterface can2;
    can1.active = true;
    can1.scriptedRx = true;
    can1.rxFrameCount = CANService::RX_QUEUE_DEPTH + 3;
    for (uint8_t i = 0; i < can1.rxFrameCount; ++i) {
        can1.rxFrames[i].id = (uint32_t)(0x100 + i);
    }
    CANService service(can1, can2);
    TEST_ASSERT_TRUE(service.subscribeRx(CAN_BUS_1, CAN_RX_MONITOR));
    TEST_ASSERT_EQUAL(CANService::RX_QUEUE_DEPTH + 3, service.pumpRx(40));
    TEST_ASSERT_EQUAL(3, service.getRxDrops(CAN_BUS_1, CAN_RX_MONITOR));

    CanRxFrame frame = {};
    for (uint8_t i = 0; i < CANService::RX_QUEUE_DEPTH; ++i) {
        TEST_ASSERT_TRUE(service.receiveRx(CAN_BUS_1, CAN_RX_MONITOR, frame));
        TEST_ASSERT_EQUAL((uint32_t)(0x100 + i), frame.message.id);
    }
    TEST_ASSERT_FALSE(service.receiveRx(CAN_BUS_1, CAN_RX_MONITOR, frame));
}
void test_bounded_index_parser_rejects_wraparound(void) {
    uint8_t index = 0;
    TEST_ASSERT_TRUE(ctParseBoundedIndex("7", 8, index));
    TEST_ASSERT_EQUAL(7, index);
    TEST_ASSERT_FALSE(ctParseBoundedIndex("8", 8, index));
    TEST_ASSERT_FALSE(ctParseBoundedIndex("256", 8, index));
    TEST_ASSERT_FALSE(ctParseBoundedIndex("65543", 8, index));
    TEST_ASSERT_FALSE(ctParseBoundedIndex("-1", 8, index));
    TEST_ASSERT_FALSE(ctParseBoundedIndex("1x", 8, index));
    TEST_ASSERT_FALSE(ctParseBoundedIndex("", 8, index));
    TEST_ASSERT_TRUE(ctParseBoundedIndex("9", 10, index));
    TEST_ASSERT_EQUAL(9, index);
    TEST_ASSERT_FALSE(ctParseBoundedIndex("10", 10, index));
}
void test_strict_decimal_and_boolean_parsing(void) {
    uint32_t value = 0;
    bool flag = false;
    TEST_ASSERT_TRUE(ctParseUnsignedDecimal("500000", 1000000, value));
    TEST_ASSERT_EQUAL_UINT32(500000, value);
    TEST_ASSERT_FALSE(ctParseUnsignedDecimal("500000x", 1000000, value));
    TEST_ASSERT_FALSE(ctParseUnsignedDecimal("4294967296", UINT32_MAX, value));
    TEST_ASSERT_TRUE(ctParseBoolean("true", flag));
    TEST_ASSERT_TRUE(flag);
    TEST_ASSERT_TRUE(ctParseBoolean("false", flag));
    TEST_ASSERT_FALSE(flag);
    TEST_ASSERT_FALSE(ctParseBoolean("garbage", flag));
}
void test_wrap_safe_timer(void) {
    const uint32_t start = 0xFFFFFF00u;
    const uint32_t now = 0x00000100u;
    TEST_ASSERT_TRUE(ctElapsedAtLeast(now, start, 512));
    TEST_ASSERT_FALSE(ctElapsedAtLeast(now, start, 513));
    TEST_ASSERT_TRUE(ctElapsedMoreThan(now, start, 511));
    TEST_ASSERT_FALSE(ctElapsedMoreThan(now, start, 512));
}

void test_hex_standard_and_extended(void) {
    uint32_t v = 0;
    TEST_ASSERT_TRUE(ctParseHexUint32("7FF", v, 8)); TEST_ASSERT_EQUAL_HEX32(0x7FF, v);
    TEST_ASSERT_TRUE(ctParseHexUint32("0x1ABCDE", v, 8)); TEST_ASSERT_EQUAL_HEX32(0x1ABCDE, v);
    TEST_ASSERT_TRUE(ctParseHexUint32("ABCDEF01", v, 8)); TEST_ASSERT_EQUAL_HEX32(0xABCDEF01, v);
}
void test_hex_rejects_bad_input(void) {
    uint32_t v = 0; uint8_t b = 0;
    TEST_ASSERT_FALSE(ctParseHexUint32("", v, 8));
    TEST_ASSERT_FALSE(ctParseHexUint32("0x", v, 8));
    TEST_ASSERT_FALSE(ctParseHexUint32("12G4", v, 8));
    TEST_ASSERT_FALSE(ctParseHexUint32("123456789", v, 8));
    TEST_ASSERT_FALSE(ctParseHexUint32("7FFjunk", v, 8));
    TEST_ASSERT_TRUE(ctParseHexByteToken("FF", b)); TEST_ASSERT_EQUAL_HEX8(0xFF, b);
    TEST_ASSERT_FALSE(ctParseHexByteToken("100", b));
    TEST_ASSERT_FALSE(ctParseHexByteToken("GG", b));
}

void test_obd_valid_pid_response_uses_pci_length(void) {
    const uint8_t frame[] = {0x06, 0x41, 0x0C, 0x1A, 0xF8, 0xAA, 0xBB, 0xCC};
    CtObdSingleFrame p;
    TEST_ASSERT_TRUE(ctParseObdSingleFrame(frame, 8, 0x41, 0x0C, p));
    TEST_ASSERT_EQUAL(6, p.payloadLength);
    TEST_ASSERT_EQUAL(3, p.dataOffset);
}
void test_obd_reply_requires_standard_data_frame(void) {
    TEST_ASSERT_TRUE(ctIsObdReplyFrame(0x7E8, false, false));
    TEST_ASSERT_TRUE(ctIsObdReplyFrame(0x7E9, false, false));
    TEST_ASSERT_TRUE(ctIsObdReplyFrame(0x7EA, false, false));
    TEST_ASSERT_TRUE(ctIsObdReplyFrame(0x7EF, false, false));
    TEST_ASSERT_FALSE(ctIsObdReplyFrame(0x7E8, true, false));
    TEST_ASSERT_FALSE(ctIsObdReplyFrame(0x7E8, false, true));
    TEST_ASSERT_FALSE(ctIsObdReplyFrame(0x7E7, false, false));
    TEST_ASSERT_FALSE(ctIsObdReplyFrame(0x7F0, false, false));
}
void test_obd_rejects_inconsistent_dlc_and_pci(void) {
    CtObdSingleFrame p;
    const uint8_t shortFrame[] = {0x06, 0x41, 0x0C, 0x1A};
    const uint8_t multiFrame[] = {0x10, 0x06, 0x41, 0x0C, 0x1A, 0xF8, 0x00, 0x00};
    TEST_ASSERT_FALSE(ctParseObdSingleFrame(shortFrame, sizeof(shortFrame), 0x41, 0x0C, p));
    TEST_ASSERT_FALSE(ctParseObdSingleFrame(multiFrame, sizeof(multiFrame), 0x41, 0x0C, p));
}
void test_obd_rejects_wrong_service_or_pid(void) {
    CtObdSingleFrame p;
    const uint8_t frame[] = {0x06, 0x41, 0x0D, 0x40, 0, 0, 0, 0};
    TEST_ASSERT_FALSE(ctParseObdSingleFrame(frame, 8, 0x41, 0x0C, p));
    TEST_ASSERT_FALSE(ctParseObdSingleFrame(frame, 8, 0x43, 0xFF, p));
}
void test_obd_clear_dtc_requires_positive_ecu_ack(void) {
    const uint8_t positive[] = {0x01, 0x44, 0, 0, 0, 0, 0, 0};
    const uint8_t negative[] = {0x03, 0x7F, 0x04, 0x22, 0, 0, 0, 0};
    const uint8_t wrongService[] = {0x03, 0x7F, 0x03, 0x22, 0, 0, 0, 0};
    const uint8_t malformed[] = {0x02, 0x7F, 0x04, 0, 0, 0, 0, 0};
    uint8_t responseCode = 0;

    TEST_ASSERT_TRUE(ctParseObdPositiveServiceAck(positive, sizeof(positive), 0x44));
    TEST_ASSERT_FALSE(ctParseObdPositiveServiceAck(positive, sizeof(positive), 0x43));
    TEST_ASSERT_TRUE(ctParseObdNegativeResponse(negative, sizeof(negative), 0x04,
                                               responseCode));
    TEST_ASSERT_EQUAL_HEX8(0x22, responseCode);
    TEST_ASSERT_FALSE(ctParseObdNegativeResponse(wrongService, sizeof(wrongService),
                                                0x04, responseCode));
    TEST_ASSERT_FALSE(ctParseObdNegativeResponse(malformed, sizeof(malformed),
                                                0x04, responseCode));
}
void test_dtc_response_and_padding(void) {
    const uint8_t frame[] = {0x07, 0x43, 0x01, 0x23, 0x00, 0x00, 0x00, 0x00};
    CtObdSingleFrame p;
    TEST_ASSERT_TRUE(ctParseObdSingleFrame(frame, 8, 0x43, 0xFF, p));
    TEST_ASSERT_EQUAL(2, p.dataOffset);
    TEST_ASSERT_EQUAL_HEX16(0x0123, (uint16_t)((frame[2] << 8) | frame[3]));
    TEST_ASSERT_EQUAL_HEX16(0x0000, (uint16_t)((frame[4] << 8) | frame[5]));
    TEST_ASSERT_TRUE(ctDtcPayloadHasValidPairLength(1));
    TEST_ASSERT_TRUE(ctDtcPayloadHasValidPairLength(3));
    TEST_ASSERT_FALSE(ctDtcPayloadHasValidPairLength(2));

    const uint8_t noDtc[] = {0x01, 0x43};
    TEST_ASSERT_TRUE(ctParseObdSingleFrame(noDtc, sizeof(noDtc), 0x43, 0xFF, p));
    TEST_ASSERT_EQUAL(0, p.pid);
    TEST_ASSERT_EQUAL(2, p.dataOffset);
}
void test_obd_dtc_payload_parsing(void) {
    const uint8_t payload[] = {0x43, 0x01, 0x23, 0x00, 0x00, 0xA4, 0x56};
    uint16_t codes[2] = {};
    uint8_t count = 0;
    TEST_ASSERT_TRUE(ctParseObdDtcPayload(payload, sizeof(payload), codes, 2, count));
    TEST_ASSERT_EQUAL(2, count);
    TEST_ASSERT_EQUAL_HEX16(0x0123, codes[0]);
    TEST_ASSERT_EQUAL_HEX16(0xA456, codes[1]);

    const uint8_t malformed[] = {0x43, 0x01, 0x23, 0x45};
    TEST_ASSERT_FALSE(ctParseObdDtcPayload(malformed, sizeof(malformed), codes, 2, count));
    TEST_ASSERT_FALSE(ctParseObdDtcPayload(payload, sizeof(payload), codes, 0, count));
}
void test_isotp_flow_control_frame_validation(void) {
    uint8_t frame[8] = {};
    TEST_ASSERT_TRUE(ctBuildIsoTpFlowControl(frame, sizeof(frame), 0, 0, 0));
    TEST_ASSERT_EQUAL_HEX8(0x30, frame[0]);
    TEST_ASSERT_EQUAL(0, frame[1]);
    TEST_ASSERT_EQUAL(0, frame[2]);
    TEST_ASSERT_TRUE(ctBuildIsoTpFlowControl(frame, sizeof(frame), 1, 4, 0xF3));
    TEST_ASSERT_EQUAL_HEX8(0x31, frame[0]);
    TEST_ASSERT_EQUAL(4, frame[1]);
    TEST_ASSERT_EQUAL_HEX8(0xF3, frame[2]);
    TEST_ASSERT_FALSE(ctBuildIsoTpFlowControl(frame, 7, 0, 0, 0));
    TEST_ASSERT_FALSE(ctBuildIsoTpFlowControl(frame, sizeof(frame), 3, 0, 0));
    TEST_ASSERT_FALSE(ctBuildIsoTpFlowControl(frame, sizeof(frame), 0, 0, 0x80));
}
void test_isotp_multiframe_reassembly_and_sequence_validation(void) {
    const uint8_t first[] = {0x10, 0x07, 0x43, 0x01, 0x23, 0x04, 0x56, 0x07};
    const uint8_t last[] = {0x21, 0x89, 0, 0, 0, 0, 0, 0};
    uint8_t payload[8] = {};
    CtIsoTpReassembly state;
    TEST_ASSERT_TRUE(ctIsoTpBegin(first, sizeof(first), 0x7E8, false,
                                  payload, sizeof(payload), state));
    TEST_ASSERT_FALSE(ctIsoTpComplete(state));
    TEST_ASSERT_FALSE(ctIsoTpAppend(last, sizeof(last), 0x7E9, false,
                                   payload, state));
    const uint8_t wrongSequence[] = {0x22, 0x89, 0, 0, 0, 0, 0, 0};
    TEST_ASSERT_FALSE(ctIsoTpAppend(wrongSequence, sizeof(wrongSequence), 0x7E8,
                                   false, payload, state));
    TEST_ASSERT_TRUE(ctIsoTpAppend(last, sizeof(last), 0x7E8, false,
                                  payload, state));
    TEST_ASSERT_TRUE(ctIsoTpComplete(state));
    TEST_ASSERT_EQUAL(0x43, payload[0]);
    TEST_ASSERT_EQUAL(0x89, payload[6]);
    TEST_ASSERT_FALSE(ctIsoTpAppend(last, sizeof(last), 0x7E8, false,
                                   payload, state));
}

void test_isotp_rejects_invalid_first_frame(void) {
    const uint8_t singleFrame[] = {0x02, 0x43, 0, 0, 0, 0, 0, 0};
    const uint8_t oversized[] = {0x10, 0x20, 0x43, 0, 0, 0, 0, 0};
    uint8_t payload[8] = {};
    CtIsoTpReassembly state;
    TEST_ASSERT_FALSE(ctIsoTpBegin(singleFrame, sizeof(singleFrame), 0x7E8,
                                   false, payload, sizeof(payload), state));
    TEST_ASSERT_FALSE(ctIsoTpBegin(oversized, sizeof(oversized), 0x7E8,
                                   false, payload, sizeof(payload), state));
}

void test_dbc_intel_boundaries(void) {
    TEST_ASSERT_TRUE(ctDbcSignalFitsDlc(0, 1, false, 1));
    TEST_ASSERT_TRUE(ctDbcSignalFitsDlc(0, 8, false, 1));
    TEST_ASSERT_TRUE(ctDbcSignalFitsDlc(7, 2, false, 2));
    TEST_ASSERT_FALSE(ctDbcSignalFitsDlc(7, 2, false, 1));
    TEST_ASSERT_TRUE(ctDbcSignalFitsDlc(0, 64, false, 8));
}
void test_dbc_motorola_boundaries(void) {
    TEST_ASSERT_TRUE(ctDbcSignalFitsDlc(7, 8, true, 1));
    TEST_ASSERT_TRUE(ctDbcSignalFitsDlc(7, 16, true, 2));
    TEST_ASSERT_FALSE(ctDbcSignalFitsDlc(7, 16, true, 1));
    TEST_ASSERT_TRUE(ctDbcSignalFitsDlc(63, 2, true, 8));
    TEST_ASSERT_FALSE(ctDbcSignalFitsDlc(0, 64, true, 8));
}
void test_dbc_invalid_lengths_and_start(void) {
    TEST_ASSERT_FALSE(ctDbcSignalFitsDlc(0, 0, false, 8));
    TEST_ASSERT_FALSE(ctDbcSignalFitsDlc(0, 65, false, 8));
    TEST_ASSERT_FALSE(ctDbcSignalFitsDlc(64, 1, false, 8));
}

void test_verification_fingerprint_changes_with_command(void) {
    const uint8_t a[] = {0x01, 0x02};
    const uint8_t b[] = {0x01, 0x03};
    uint32_t fa = ctVerifyFingerprint(1, 1, "lock", 0x123, false, 2, a);
    TEST_ASSERT_NOT_EQUAL(0, fa);
    TEST_ASSERT_NOT_EQUAL(fa, ctVerifyFingerprint(1, 1, "lock", 0x123, false, 2, b));
    TEST_ASSERT_NOT_EQUAL(fa, ctVerifyFingerprint(2, 1, "lock", 0x123, false, 2, a));
    TEST_ASSERT_NOT_EQUAL(fa, ctVerifyFingerprint(1, 1, "unlock", 0x123, false, 2, a));
}



void test_dbc_extended_id_decode(void) {
    uint32_t id = 0;
    bool extended = false;
    TEST_ASSERT_TRUE(ctDecodeDbcCanId(0x123, id, extended));
    TEST_ASSERT_EQUAL_HEX32(0x123, id);
    TEST_ASSERT_FALSE(extended);

    TEST_ASSERT_TRUE(ctDecodeDbcCanId(0x80000123UL, id, extended));
    TEST_ASSERT_EQUAL_HEX32(0x123, id);
    TEST_ASSERT_TRUE(extended);

    TEST_ASSERT_TRUE(ctDecodeDbcCanId(0x80000000UL, id, extended));
    TEST_ASSERT_EQUAL_HEX32(0x00000000UL, id);
    TEST_ASSERT_TRUE(extended);

    // Bundled GM vendor/OpenDBC files contain raw 29-bit IDs without the
    // DBC bit-31 marker; these must still become Extended CAN frames.
    TEST_ASSERT_TRUE(ctDecodeDbcCanId(0x10630000UL, id, extended));
    TEST_ASSERT_EQUAL_HEX32(0x10630000UL, id);
    TEST_ASSERT_TRUE(extended);
}

void test_json_command_validation(void) {
    JsonDocument doc;
    JsonObject item = doc.to<JsonObject>();
    item["label"] = "lock_all";
    item["displayName"] = "Lock";
    item["canId"] = 0x123;
    item["extended"] = false;
    item["length"] = 2;
    item["data"].to<JsonArray>().add(1);
    item["data"].as<JsonArray>().add(2);
    item["source"] = 2;
    item["status"] = "verified";
    CtJsonCommandFields f;
    TEST_ASSERT_TRUE(ctValidateImportedCommand(item, f));
    TEST_ASSERT_EQUAL_HEX32(0x123, f.canId);
    TEST_ASSERT_EQUAL(2, f.length);
}
void test_json_command_rejects_invalid_types_and_ranges(void) {
    JsonDocument doc;
    JsonObject item = doc.to<JsonObject>();
    item["label"] = "x"; item["displayName"] = "x"; item["canId"] = 0x800;
    item["extended"] = false; item["length"] = 1; item["data"].to<JsonArray>().add(1);
    item["source"] = 2; item["status"] = "unverified";
    CtJsonCommandFields f;
    TEST_ASSERT_FALSE(ctValidateImportedCommand(item, f));
    item["canId"] = 0x123; item["extended"] = "false";
    TEST_ASSERT_FALSE(ctValidateImportedCommand(item, f));
    item["extended"] = false; item["source"] = 99;
    TEST_ASSERT_FALSE(ctValidateImportedCommand(item, f));
    item["source"] = 2; char longName[49]; memset(longName, 'x', 48); longName[48] = '\0'; item["displayName"] = longName;
    TEST_ASSERT_FALSE(ctValidateImportedCommand(item, f));
}

void test_verification_transaction_rejections(void) {
    const char* label = "lock";
    TEST_ASSERT_TRUE(ctVerifyTransactionValid(true, 1, label, 77, 1000, 1, label, 77, 1500, 120000));
    TEST_ASSERT_FALSE(ctVerifyTransactionValid(false, 1, label, 77, 1000, 1, label, 77, 1500, 120000));
    TEST_ASSERT_FALSE(ctVerifyTransactionValid(true, 1, label, 77, 1000, 2, label, 77, 1500, 120000));
    TEST_ASSERT_FALSE(ctVerifyTransactionValid(true, 1, label, 77, 1000, 1, "unlock", 77, 1500, 120000));
    TEST_ASSERT_FALSE(ctVerifyTransactionValid(true, 1, label, 77, 1000, 1, label, 78, 1500, 120000));
    TEST_ASSERT_FALSE(ctVerifyTransactionValid(true, 1, label, 77, 1000, 1, label, 77, 121001, 120000));
}
void test_vehicle_tx_guard_blocks_when_config_or_driver_listen_only() {
    // config says Listen-Only: blocked even if the driver reports Normal
    TEST_ASSERT_EQUAL(CT_TX_ERR_LISTEN_ONLY, ctVehicleTxGuard(true, true, false, 8));
    // driver really in Listen-Only: blocked even if config says Normal
    TEST_ASSERT_EQUAL(CT_TX_ERR_LISTEN_ONLY, ctVehicleTxGuard(false, true, true, 8));
    // inactive bus is reported as not initialised, never as success
    TEST_ASSERT_EQUAL(CT_TX_ERR_NOT_INITIALIZED, ctVehicleTxGuard(false, false, false, 8));
    TEST_ASSERT_EQUAL(CT_TX_ERR_LENGTH, ctVehicleTxGuard(false, true, false, 9));
    TEST_ASSERT_EQUAL(CT_TX_OK, ctVehicleTxGuard(false, true, false, 8));
}

void test_bus_route_selector_is_sanitised() {
    TEST_ASSERT_EQUAL(0, ctSanitizeBusIndex(0));
    TEST_ASSERT_EQUAL(1, ctSanitizeBusIndex(1));
    TEST_ASSERT_EQUAL(0, ctSanitizeBusIndex(2));
    TEST_ASSERT_EQUAL(0, ctSanitizeBusIndex(255));
}

void test_mcp2515_8mhz_timing_table_decodes_to_requested_bitrates() {
    // CNF1/CNF2/CNF3 for an 8 MHz crystal. Values are taken from the usual
    // autowp-mcp2515 table; scripts/check_mcp_timing.py compares them with the
    // header that is really installed by PlatformIO.
    struct Row { uint32_t bps; uint8_t c1, c2, c3; uint32_t minSp, maxSp; };
    const Row rows[] = {
        {1000000, 0x00, 0x80, 0x80, 700, 800},
        { 500000, 0x00, 0x90, 0x82, 600, 800},
        { 250000, 0x00, 0xB1, 0x85, 600, 800},
        { 125000, 0x01, 0xB1, 0x85, 600, 800},
        { 100000, 0x01, 0xB4, 0x86, 600, 800},
    };
    for (unsigned i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i) {
        uint32_t bps = 0, sp = 0; uint8_t sjw = 0;
        TEST_ASSERT_TRUE(ctMcp2515DecodeTiming(8000000UL, rows[i].c1, rows[i].c2,
                                               rows[i].c3, bps, sp, sjw));
        TEST_ASSERT_EQUAL(rows[i].bps, bps);
        TEST_ASSERT_TRUE(sp >= rows[i].minSp && sp <= rows[i].maxSp);
        TEST_ASSERT_TRUE(sjw >= 1 && sjw <= 4);
        TEST_ASSERT_TRUE(ctMcp2515BitrateValid(rows[i].bps));
    }
    TEST_ASSERT_FALSE(ctMcp2515BitrateValid(800000));   // TWAI-only rate
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_tx_guard_listen_only);
    RUN_TEST(test_tx_guard_initialization_and_dlc);
    RUN_TEST(test_tx_id_validity);
    RUN_TEST(test_battery_voltage_availability);
    RUN_TEST(test_can_frame_identity_includes_format);
    RUN_TEST(test_can_bus_pin_conflicts_are_rejected);
    RUN_TEST(test_mcp2515_supported_bitrates);
    RUN_TEST(test_partition_layout_must_fit_detected_flash);
    RUN_TEST(test_filesystem_ota_requires_mounted_empty_user_storage);
    RUN_TEST(test_can_service_initializes_each_bus_independently);
    RUN_TEST(test_can_service_starts_with_can1_only);
    RUN_TEST(test_can_service_reports_unavailable_when_both_fail);
    RUN_TEST(test_can_service_routes_legacy_calls_to_can1);
    RUN_TEST(test_can_service_does_not_fallback_from_selected_inactive_bus);
    RUN_TEST(test_can_service_fans_out_frames_to_independent_subscribers);
    RUN_TEST(test_can_service_pumps_both_buses_independently);
    RUN_TEST(test_can_service_fans_out_to_monitor_and_recorder);
    RUN_TEST(test_can_record_format_preserves_channel_and_payload);
    RUN_TEST(test_can_record_format_rejects_invalid_or_truncated_frames);
    RUN_TEST(test_can_record_filename_allowlist);
    RUN_TEST(test_can_service_bounds_consumer_queues_and_counts_drops);
    RUN_TEST(test_bounded_index_parser_rejects_wraparound);
    RUN_TEST(test_strict_decimal_and_boolean_parsing);
    RUN_TEST(test_wrap_safe_timer);
    RUN_TEST(test_hex_standard_and_extended);
    RUN_TEST(test_hex_rejects_bad_input);
    RUN_TEST(test_obd_valid_pid_response_uses_pci_length);
    RUN_TEST(test_obd_reply_requires_standard_data_frame);
    RUN_TEST(test_obd_rejects_inconsistent_dlc_and_pci);
    RUN_TEST(test_obd_rejects_wrong_service_or_pid);
    RUN_TEST(test_obd_clear_dtc_requires_positive_ecu_ack);
    RUN_TEST(test_dtc_response_and_padding);
    RUN_TEST(test_obd_dtc_payload_parsing);
    RUN_TEST(test_isotp_flow_control_frame_validation);
    RUN_TEST(test_isotp_multiframe_reassembly_and_sequence_validation);
    RUN_TEST(test_isotp_rejects_invalid_first_frame);
    RUN_TEST(test_dbc_intel_boundaries);
    RUN_TEST(test_dbc_motorola_boundaries);
    RUN_TEST(test_dbc_invalid_lengths_and_start);
    RUN_TEST(test_verification_fingerprint_changes_with_command);
    RUN_TEST(test_verification_transaction_rejections);
    RUN_TEST(test_dbc_extended_id_decode);
    RUN_TEST(test_json_command_validation);
    RUN_TEST(test_json_command_rejects_invalid_types_and_ranges);
    RUN_TEST(test_vehicle_tx_guard_blocks_when_config_or_driver_listen_only);
    RUN_TEST(test_bus_route_selector_is_sanitised);
    RUN_TEST(test_mcp2515_8mhz_timing_table_decodes_to_requested_bitrates);
    return UNITY_END();
}
