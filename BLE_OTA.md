# CarTouch BLE and BLE OTA

CarTouch starts BLE independently of Wi-Fi and advertises as `CarTouch-XXXX`.

## Characteristics
- **Status** — read/notify status and OTA progress. Authenticated diagnostic notifications are sent only to the requesting connection.
- **Command** — status, authenticated DTC/recording commands, and OTA control commands.
- **Data** — firmware bytes during authenticated OTA.

## Authenticated device commands
1. Pair/connect over the encrypted BLE link.
2. Write `AUTH:<web-password>` to Command. The default Web password is not accepted; five failures lock command authentication for 60 seconds.
3. Send one of `DTC:READ`, `DTC:CLEAR`, `DTC:STATUS`, `RECORD:START:1`, `RECORD:START:2`, `RECORD:START:BOTH`, `RECORD:STOP`, `RECORD:STATUS`, or `RECORD:DELETE:canNNNN.csv`. Device operations are rate-limited to one request per 300 ms.
4. Subscribe to Status for the result. Diagnostic status notifications are targeted to the authenticated connection. `DTC:CLEAR` is explicit and reports success only after a positive ECU acknowledgement.
5. Send `LOGOUT` when finished. Command authentication and OTA ownership are bound to the BLE connection and cleared on disconnect.

This is a bounded subset, not Web UI parity. BLE does not accept raw CAN frames, replay, vehicle control, Learn changes, or DBC import. Accepted DTC and recording operations use the same main-loop services as Web and Serial.

## Authenticated device commands
1. Pair/connect over the encrypted BLE link.
2. Write `AUTH:<web-password>` to Command. The default Web password is not accepted; five failures lock command authentication for 60 seconds.
3. Send one of `DTC:READ`, `DTC:CLEAR`, `DTC:STATUS`, `RECORD:START:1`, `RECORD:START:2`, `RECORD:START:BOTH`, `RECORD:STOP`, `RECORD:STATUS`, or `RECORD:DELETE:canNNNN.csv`.
4. Read/subscribe to Status for the resulting state. `DTC:CLEAR` is explicit and requires a positive ECU acknowledgement before reporting success.
5. Send `LOGOUT` when finished. Authentication is connection-scoped and is cleared when that connection disconnects.

BLE device commands are a bounded subset; they do not provide raw CAN transmission, Learn profile management, DBC import, or general Web UI parity. All accepted operations are queued into the same main-loop services used by Web/Serial.

## OTA sequence
1. Connect to the device.
2. Send `START:<web-password>:<firmware-size>` (the size is always taken after the last `:`, so the password may contain `:`).
3. Wait for `OTA_STARTED`.
4. Write firmware bytes to Data.
5. Send `END`.
6. The device verifies size, finalizes the image and reboots.

`ABORT` cancels a transfer; disconnecting also aborts an active OTA.

## Security
BLE link security is enabled. BLE OTA requires the current Web password in `START`; device commands require a separate `AUTH` using that password. Do not expose the password over an untrusted BLE environment.

The authenticated Web UI remains the full configuration surface.

## Hardening notes
- Command/Data characteristics require an encrypted (paired) link.
- OTA is refused while the default web password is still in use (`OTA_CHANGE_DEFAULT_PASSWORD`).
- 5 wrong passwords lock BLE OTA for 60 seconds.
- Pairing uses Just Works (no passkey, no MITM protection). The link is encrypted, but an attacker present during first pairing could impersonate the device or the phone and capture the OTA password. Pair only in a trusted place, and change the Web password if pairing may have been observed.
