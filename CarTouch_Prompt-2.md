CarTouch — FINAL MASTER IMPLEMENTATION PROMPT

Full Repository Repair, Core Feature Completion, Dual-CAN Architecture, Multi-Interface Simultaneous Operation, Multi-Board Support, Testing, and Final Delivery

Execution mode: Direct repository modification
Target: Current CarTouch repository
Primary hardware: ESP32-S3 N16R8 + TJA1051 + MCP2515/TJA1050 8MHz
Priority: Functional correctness, reliability, hardware independence, and real-world usability
Delivery objective: A complete, corrected, build-verified project with transparent test results

---

1. ROLE AND PRIMARY OBJECTIVE

Act as a senior embedded firmware engineer, ESP32-S3 specialist, automotive CAN/OBD-II engineer, software architect, hardware integration engineer, and verification engineer.

You are responsible for completing the actual CarTouch project, not merely reviewing it.

Your mission is to:

1. Inspect the complete current repository.
2. Identify all remaining functional, architectural, integration, reliability, configuration, and documentation problems.
3. Repair existing defects.
4. Complete missing essential functionality.
5. Integrate all supported hardware configurations.
6. Implement independent and simultaneous operation of available interfaces.
7. Preserve existing working features.
8. Run all feasible builds and tests.
9. Resolve all reproducible errors within scope.
10. Deliver the complete corrected project and a truthful verification report.

The immediate goal is a reliable CarTouch device for real personal use.

Commercial-grade refinement is a future objective. Do not waste the current development effort on cosmetic changes while core functionality remains incomplete.

Do not stop after producing an audit, recommendations, TODO list, or another prompt. Perform the actual implementation.

If you have direct repository-editing capability, modify the repository.

If you do not have direct editing capability, provide complete exact replacement files or patches, with their repository paths and all required changes. Never pretend that files were modified when they were not.

---

2. SOURCE OF TRUTH AND INITIAL REPOSITORY AUDIT

The actual current repository is the source of truth.

Before modifying files:

- Inspect the complete repository tree.
- Inspect all source files.
- Inspect PlatformIO/build configuration.
- Inspect partition tables.
- Inspect dependencies.
- Inspect hardware configuration.
- Inspect existing tests.
- Inspect scripts.
- Inspect DBC and other data files.
- Inspect documentation.
- Inspect existing firmware binaries, but do not treat their presence as proof of a successful current source build.
- Trace all relevant function calls and subsystem dependencies.

Previous audit observations are starting hypotheses, not permission to skip verification.

In particular, recheck the following previously identified areas:

- CAN1/CAN2 integration and actual independent operation.
- MCP2515 oscillator configuration.
- OBD-II and ISO-TP completeness.
- CAN Monitor and recording integration.
- BLE functionality.
- Five-way input.
- SD support.
- TFT and Touch hardware abstraction.
- Headless behavior.
- Runtime hardware status reporting.
- 4MB partition and filesystem constraints.
- DBC loading and storage.
- Task lifecycle and concurrency.
- OTA and user-data preservation.
- Existing legacy CAN command paths.

Do not assume an earlier defect still exists if it has already been fixed. Verify the current code first.

---

3. HARDWARE PRIORITY

3.1 Primary reference board

The primary development and validation target is:

ESP32-S3 N16R8

- 16MB Flash
- 8MB PSRAM
- Native TWAI
- Wi-Fi
- BLE
- USB
- SPI
- Optional TFT
- Optional Touch
- Optional Five-way controls
- Optional SD

This is the main reference profile for architecture, memory planning, feature integration, and testing.

3.2 Primary CAN interface — CAN1

Hardware:

ESP32-S3 internal TWAI → TJA1051 → CAN bus.

CAN1 is the default primary CAN interface.

3.3 Secondary CAN interface — CAN2

Hardware:

ESP32-S3 SPI → MCP2515 → TJA1050 → CAN bus.

The user's reference MCP2515 module has an 8MHz oscillator.

The firmware must correctly support 8MHz timing.

Do not silently assume 16MHz.

3.4 Other supported board configurations

Support, within actual hardware limits:

- ESP32-S3 with 4MB Flash and no PSRAM.
- ESP32-S3 with 4MB Flash and available PSRAM, including approximately 2MB variants.
- ESP32-S3 N16R8.
- Other ESP32-S3 Flash/PSRAM combinations through validated profiles.

Do not make N16R8 the only usable board.

Do not expand to unrelated chip families such as ESP32-C6 unless explicitly required and technically implemented.

---

4. PRIMARY ARCHITECTURAL REQUIREMENT

CarTouch must be a modular, resource-aware, multi-hardware system.

Its architecture must separate:

- Hardware detection and configuration.
- CAN drivers.
- CAN service management.
- OBD-II.
- ISO-TP.
- Learn.
- CAN Monitor.
- CAN Recording.
- DBC decoding and management.
- Storage.
- TFT.
- Touch.
- Five-way controls.
- Web.
- BLE.
- USB Serial.
- Diagnostics.
- Configuration.
- OTA.

No optional peripheral may become a single point of failure for the entire device.

The system must support full operation when all supported hardware is available and graceful reduced operation when individual components are missing or fail.

Do not confuse hardware absence with a software defect.

Do not confuse a successfully initialized driver with verified real-world functionality.

---

5. DUAL-CAN IMPLEMENTATION

5.1 Independent CAN backends

CAN1 and CAN2 must have:

- Independent initialization.
- Independent configuration.
- Independent bitrate.
- Independent listen-only state.
- Independent RX/TX handling.
- Independent diagnostics.
- Independent error reporting.
- Independent recovery.
- Independent bus-off handling.
- Independent queue management.
- Independent availability state.

A failure in CAN1 must not automatically stop CAN2.

A failure in CAN2 must not automatically stop CAN1.

5.2 Simultaneous CAN operation

When hardware and resources permit, support:

- CAN1 receive + CAN2 receive.
- CAN1 monitor + CAN2 monitor.
- Recording both channels simultaneously.
- Independent DBC decoding.
- OBD on one channel while monitoring the other.
- Learn on one channel while another is monitored.
- Independent diagnostics and statistics.

Do not implement two CAN names that secretly route through one physical backend.

5.3 Per-function CAN selection

At minimum, allow explicit CAN selection for:

- OBD-II.
- ISO-TP.
- Learn.
- Monitor.
- Recorder.
- Diagnostics.
- Vehicle communication.
- Supported control functions.

Example:

- OBD → CAN1.
- Learn → CAN2.
- Monitor → CAN1.
- Recording → CAN1 + CAN2.

The actual selection must be reflected in status and configuration.

5.4 Transmission safety

Never silently switch an outgoing CAN operation from one physical bus to another.

If OBD is configured for CAN1 and CAN1 fails, do not silently transmit through CAN2.

Receiving fallback and transmitting fallback must be separate policies.

No automatic replay of recorded traffic.

No automatic transmission of unverified learned frames.

No invented vehicle-specific commands.

Keep safe listen-only startup behavior where appropriate, with clear user-visible information about when transmission is disabled and how a deliberate operation enables it.

---

6. SIMULTANEOUS MULTI-INTERFACE OPERATION

This is a mandatory architectural and acceptance requirement.

The supported interfaces are:

1. TFT display.
2. Touchscreen.
3. Five-way physical controls.
4. Web over Wi-Fi.
5. BLE.
6. USB Serial.

When present and resources permit, they must be capable of operating at the same time.

Required example:

TFT + Touch + Five-way + Web + BLE + USB Serial

must be able to coexist.

Do not disable one interface merely because another is active.

Examples:

- TFT displaying live CAN data while Web is connected.
- Touch input while BLE is connected.
- Five-way controls while Web and USB Serial are active.
- Web configuration while TFT displays status.
- BLE status access while USB Serial provides diagnostics.
- Multiple interfaces accessing the same underlying service without inconsistent state.

6.1 Shared service model

All interfaces must use common underlying services and validated commands.

Do not implement separate conflicting CAN, storage, or configuration logic in each UI.

Use appropriate:

- Queues.
- Mutexes.
- Event mechanisms.
- State ownership.
- Command validation.
- Operation serialization.
- Connection lifecycle management.

Prevent:

- Race conditions.
- Deadlocks.
- Use-after-free.
- Queue overflow.
- Memory leaks.
- Unbounded blocking.
- One slow interface starving unrelated services.

6.2 Concurrent configuration changes

If two interfaces attempt to modify the same setting, the system must resolve the conflict deterministically.

All interfaces must reflect the resulting authoritative state.

No interface may retain a misleading stale configuration.

6.3 Resource-aware operation

Simultaneous operation is required where hardware and resources permit.

On smaller boards:

- Protect essential CAN and core functions.
- Use bounded buffers.
- Monitor memory and task stack.
- Reduce optional workload gracefully when necessary.
- Report limitations.
- Do not crash.
- Do not corrupt user data.
- Do not claim full operation when a feature has been disabled.

---

7. OPTIONAL HARDWARE AND GRACEFUL DEGRADATION

Every optional component must be independently initialized, diagnosed, and isolated.

7.1 Missing or failed TFT

The device must still support available:

- CAN.
- OBD.
- Learn.
- Monitor.
- Recording.
- Storage.
- Web.
- BLE.
- USB Serial.

No boot failure or reboot loop.

7.2 Missing or failed Touch

TFT may continue displaying information.

Five-way, Web, BLE, and USB Serial remain available where present.

7.3 Missing or failed Five-way

Touch and remote interfaces remain available.

7.4 Missing Wi-Fi/Web

BLE, USB, TFT, physical controls, CAN, and storage continue where available.

7.5 Missing BLE

Web, USB, TFT, physical controls, CAN, and storage continue where available.

7.6 Missing USB connection

No boot dependency on a connected computer.

7.7 Missing SD

Use internal storage for essential data where capacity permits.

7.8 Missing PSRAM

Core functions must run with internal RAM.

7.9 Missing CAN1

CAN2 remains usable if available.

7.10 Missing CAN2

CAN1 remains usable if available.

7.11 Both CAN interfaces unavailable

The device must still provide available local configuration, diagnostics, firmware management, and storage functions.

Vehicle communication must be reported as unavailable rather than simulated or falsely reported as working.

7.12 Failure isolation

A failed optional component must affect only the functionality that genuinely depends on it.

Implement accurate states such as:

- Not installed.
- Disabled.
- Not detected.
- Initializing.
- Ready.
- Active.
- Limited.
- Error.
- Recovering.
- Offline.

Do not report absent hardware as READY.

Do not convert every optional peripheral failure into a global application failure.

---

8. HARDWARE PROFILES AND GPIO

Implement or complete a validated hardware profile system.

Profiles must represent:

- Flash capacity.
- PSRAM capacity.
- Board type.
- TFT controller.
- Display resolution.
- Display orientation.
- Touch controller.
- Five-way input type.
- SD interface.
- SPI assignments.
- TWAI TX/RX pins.
- MCP2515 CS/INT/RESET.
- MCP2515 oscillator.
- USB capability.
- Wi-Fi/BLE capability.

Use runtime detection where technically possible.

Where runtime detection is impossible, use explicit validated board profiles.

Do not invent pin mappings.

Audit GPIO conflicts, reserved pins, boot-strapping requirements, voltage compatibility, and peripheral sharing.

ESP32-S3 GPIO must not be assumed 5V tolerant.

Verify actual module electrical requirements and use suitable level shifting where necessary.

8.1 Runtime configuration

Where supported, allow safe configuration through available interfaces.

Before applying:

- Validate GPIO.
- Detect conflicts.
- Validate hardware compatibility.
- Save a recoverable configuration.
- Apply safely.
- Roll back if initialization fails.
- Preserve a recovery path.
- Provide factory reset.

An invalid pin configuration must not permanently prevent access to recovery interfaces.

---

9. TFT, TOUCH, AND FIVE-WAY

9.1 TFT

Audit and complete the existing display driver.

Preserve supported hardware such as ILI9341 where present.

Avoid hardcoding all project behavior to one controller or one pin layout.

Support validated profiles for different supported displays.

A TFT failure must not break the core.

9.2 Touch

Initialize independently from TFT.

Touch failure must not disable display output or unrelated services.

Fix unsafe task deletion, initialization races, and resource lifecycle problems.

Provide safe calibration persistence.

9.3 Five-way

Implement actual Five-way support if absent.

Support the validated physical input method, such as GPIO or ADC.

Provide debouncing, key state handling, and safe integration with the UI.

Five-way must operate independently of Touch when hardware supports it.

---

10. WEB, BLE, AND USB SERIAL

10.1 Web

Complete the real Web interface for the supported core features:

- Device status.
- Hardware status.
- CAN1/CAN2 status.
- CAN selection.
- Monitor.
- OBD.
- Recording.
- Learn.
- DBC management.
- Storage.
- Diagnostics.
- Configuration.
- Firmware update where supported.

Web must work without TFT.

10.2 BLE

Audit the existing BLE implementation.

If BLE currently provides mainly STATUS and OTA, complete the missing useful device interaction.

Where technically feasible, provide:

- Device information.
- Hardware status.
- CAN status.
- Configuration access.
- Diagnostics.
- Monitor/status data.
- Learn and storage operations.
- Controlled OBD operations.

Use a documented protocol, bounded payloads, validation, connection lifecycle handling, and appropriate access controls.

BLE must coexist with Web, TFT, USB Serial, and CAN workloads where resources permit.

Do not claim BLE feature parity if only a subset is implemented.

10.3 USB Serial

Provide a reliable diagnostic and control interface.

Expose important status, errors, hardware information, configuration, CAN status, storage, and supported operations.

USB Serial must not be required for normal boot.

---

11. OBD-II AND ISO-TP

Audit the complete real execution path, not just helper classes or tests.

Complete supported OBD-II operations with:

- Correct request formatting.
- Supported services and PIDs.
- Response validation.
- Timeouts.
- Retries.
- Unsupported-PID handling.
- Correct scaling and units.
- ECU response identification.
- Configurable CAN channel.
- Accurate communication state.

Do not claim complete OBD support merely because a small PID list works.

11.1 ISO-TP

Implement and integrate:

- Single Frame.
- First Frame.
- Consecutive Frame.
- Flow Control.
- Sequence number handling.
- Block size.
- Separation time.
- Timeout.
- Abort.
- Malformed frame handling.
- Reassembly.
- Incomplete transfer handling.

Ensure the actual OBD execution paths use ISO-TP where required.

Test both successful and failure paths.

---

12. CAN MONITOR AND RECORDING

12.1 Monitor

Complete functional CAN monitoring:

- CAN1.
- CAN2.
- Simultaneous monitoring.
- Standard and extended IDs.
- DLC and payload.
- Timestamp.
- Filters.
- Search.
- Statistics.
- Error reporting.
- Pause/resume.
- Channel identification.
- Web and available remote access.

Monitor must not depend on TFT.

12.2 Recording

Implement a real persistent recorder, not merely a live display or in-memory buffer.

Required:

- Start/stop.
- CAN channel identification.
- Timestamp.
- Frame integrity.
- File validation.
- Storage capacity checks.
- Listing.
- Export/import.
- Backup.
- Recovery from interrupted recording.
- Safe close and flush.

Support simultaneous recording of CAN1 and CAN2 where resources permit.

Do not automatically replay recorded frames.

Any replay function must require explicit user action and safety controls.

---

13. LEARN

Audit and complete the real Learn workflow:

- Capture.
- Candidate identification.
- Analysis.
- Differential comparison.
- User review.
- Save.
- Pending state.
- Verification.
- Confirmed state.
- Export/import.
- Persistent storage.

Unverified learned data must not be automatically trusted or transmitted.

Audit legacy mechanisms such as:

- "_legacyOverrideActive"
- "setCustomCANIDs()"
- "_sendLegacyCommand()"

Remove or migrate legacy paths only after verifying dependencies and preserving required functionality.

---

14. DBC MANAGEMENT

Audit all DBC files and loading mechanisms.

Verify:

- Syntax.
- Duplicate definitions.
- Broken references.
- Scaling.
- Endianness.
- Signedness.
- Multiplexing.
- Value tables.
- CAN identifier handling.
- Memory consumption.
- Profile mapping.
- Runtime loading.

Preserve useful definitions.

Organize essential and supplementary databases.

Support vehicle coverage relevant to the user's intended markets, including domestic, regional, Chinese, imported, and international platforms where valid data exists.

Do not invent vehicle-specific definitions.

Avoid loading all DBC data into RAM unnecessarily.

Allow database management without firmware reflashing where technically feasible.

---

15. STORAGE AND SD

Implement a unified storage layer supporting:

- Internal Flash.
- Optional SD.

If SD support is missing, implement it using a validated configuration and correct filesystem/library integration.

Required functions:

- Capacity.
- Free space.
- Health/status.
- File listing.
- Recording storage.
- Database management.
- Import/export.
- Backup/restore.
- Integrity checks.
- Corruption detection.
- Safe replacement.
- Removal/failure handling.

When SD is unavailable, internal storage must continue supporting essential data within its capacity.

Do not silently erase user data.

Do not assume a storage operation succeeded until the result is checked.

---

16. MEMORY AND PARTITION VALIDATION

Support appropriate build and partition configurations for:

- N16R8.
- 4MB no-PSRAM.
- 4MB with supported PSRAM.
- Other validated ESP32-S3 profiles.

Check actual:

- Firmware size.
- Application partition.
- NVS.
- OTA partitions.
- Filesystem.
- Web assets.
- DBC assets.
- User-data capacity.

A 4MB configuration must not use an incompatible large-Flash partition layout.

The core must not require PSRAM.

Use bounded memory allocation and handle allocation failures safely.

---

17. OTA AND USER-DATA PRESERVATION

Audit OTA and update behavior.

Verify:

- Image validation.
- Board compatibility.
- Partition compatibility.
- Available space.
- Interrupted update handling.
- Rollback where supported.
- User-data preservation.
- Filesystem update behavior.
- Recovery behavior.

Do not claim firmware signing, anti-rollback, or security properties unless they are actually implemented and verified.

Firmware updates must not unnecessarily erase:

- User settings.
- Learn profiles.
- Recordings.
- Imported databases.
- Calibration.
- User configuration.

---

18. TASKS, CONCURRENCY, AND RELIABILITY

Perform a systematic review of:

- Task creation and deletion.
- Stack sizes.
- Priorities.
- Mutexes.
- Queues.
- Event groups.
- ISR interactions.
- Callbacks.
- Shared buffers.
- Network clients.
- CAN receive/transmit.
- Storage operations.
- UI lifecycle.

Fix reproducible:

- Race conditions.
- Deadlocks.
- Memory leaks.
- Use-after-free.
- Stack overflows.
- Queue overflow.
- Unsafe task deletion.
- Unbounded blocking.
- Resource starvation.
- Incorrect service state transitions.

A failure in one interface must not unnecessarily terminate unrelated interfaces.

Use controlled recovery and supervision.

---

19. BOOT AND DEGRADED MODES

The boot process must initialize independent subsystems independently.

A missing TFT, SD, Touch, Five-way, BLE, or CAN2 must not prevent a usable boot.

Support operational states such as:

- Full Mode.
- Headless Mode.
- CAN1 Only.
- CAN2 Only.
- Dual CAN.
- No PSRAM.
- Limited Storage.
- Degraded Mode.
- Recovery Mode.

The system must report the actual state rather than falsely presenting all services as READY.

---

20. CONFIGURATION AND DATA MIGRATION

Audit NVS and configuration persistence.

Implement:

- Versioned configuration.
- Safe migration.
- Validation.
- Recovery from corrupt settings.
- Reasonable write frequency.
- Safe defaults.
- Factory reset.
- Preservation of unrelated user data.

Do not erase all configuration or storage because one setting is invalid.

---

21. DOCUMENTATION

Update documentation to match the actual implemented project.

Remove or correct:

- Outdated roadmaps.
- Duplicate instructions.
- Contradictory documentation.
- Broken links.
- Obsolete bug descriptions.
- Unsupported feature claims.

Document:

- Primary N16R8 profile.
- CAN1/TJA1051.
- CAN2/MCP2515/TJA1050 8MHz.
- Supported ESP32-S3 memory variants.
- GPIO profiles.
- Optional peripherals.
- Simultaneous interfaces.
- Headless mode.
- Degraded modes.
- Storage behavior.
- CAN channel selection.
- Build environments.
- Partition requirements.
- OTA limitations.
- Actual known limitations.

Do not document an unimplemented feature as completed.

---

22. IMPLEMENTATION PRIORITY

Perform the work in this order, without stopping after each phase merely to produce another report.

Priority 1 — Critical functional and safety defects

- Build failures.
- Boot failures.
- Incorrect hardware initialization.
- CAN routing errors.
- Unsafe transmission behavior.
- Data corruption.
- Crashes.
- Deadlocks.
- Invalid configuration recovery.

Priority 2 — Core functionality

- Dual-CAN independence.
- OBD/ISO-TP execution paths.
- CAN Monitor.
- CAN Recording.
- Learn.
- DBC decoding and management.
- Internal storage reliability.

Priority 3 — Multi-interface and hardware completeness

- TFT independence.
- Touch independence.
- Five-way implementation.
- BLE completion.
- Web/Serial integration.
- SD support.
- Hardware profiles.
- Graceful degradation.
- Simultaneous workloads.

Priority 4 — Compatibility and optimization

- 4MB builds.
- No-PSRAM behavior.
- Memory/resource limits.
- OTA/data preservation.
- Documentation.
- Noncritical cleanup.

Do not rewrite stable subsystems unnecessarily.

Make the smallest coherent set of changes that achieves the required behavior.

---

23. TESTING REQUIREMENTS

Create or update tests for the actual implementation.

At minimum, test the following where technically possible.

Hardware profiles

- N16R8.
- 4MB no PSRAM.
- 4MB with supported PSRAM.
- Headless.
- Different validated pin profiles.

CAN

- CAN1 only.
- CAN2 only.
- Both active.
- CAN1 initialization failure.
- CAN2 initialization failure.
- MCP2515 8MHz.
- Bitrate handling.
- Listen-only.
- Bus-off.
- Recovery.
- No silent TX fallback.
- Independent channel selection.

Interfaces

- TFT only.
- TFT + Touch.
- TFT + Five-way.
- Web + BLE.
- Web + USB.
- BLE + USB.
- TFT + Web + BLE + USB.
- All available interfaces simultaneously.

Combined operation

Where resources permit:

- CAN1 + CAN2 monitoring.
- OBD CAN1 + Monitor CAN2.
- Learn CAN2 + Monitor CAN1.
- Recording both buses.
- TFT + Web + BLE + USB while CAN traffic is active.
- Storage operations during monitoring.
- Interface disconnection and reconnection.
- Optional peripheral initialization failure.

Storage

- SD present.
- SD absent.
- SD full.
- SD unavailable.
- Internal storage near capacity.
- Corrupted configuration.
- Interrupted recording.
- User-data preservation during update.

---

24. BUILD AND VERIFICATION

Run actual builds wherever the environment permits.

At minimum attempt all relevant supported build environments.

Record:

- Exact environment.
- Build result.
- Firmware size.
- Partition compatibility.
- Relevant warnings.
- Relevant errors.

Run available automated tests.

Do not claim a successful build merely because:

- A previous binary exists.
- Source code appears syntactically correct.
- A build configuration exists.
- A test was written but not executed.

Use these result labels:

PASS — Actually executed and passed.

FAIL — Actually executed and failed.

NOT RUN — Not executed.

HARDWARE REQUIRED — Requires physical hardware unavailable to the current execution environment.

PARTIAL — Only a documented subset was verified.

Physical tests must be distinguished from source-level and simulated tests.

---

25. MAIN BRANCH AND REPOSITORY RULES

The user's latest instruction authorizes direct application of the completed changes to the repository's "main" branch.

If repository permissions and branch protection allow direct changes:

- Apply the completed changes to "main".
- Preserve existing valid work.
- Do not force-push.
- Do not discard unrelated changes.
- Do not rewrite repository history unnecessarily.

If direct modification of "main" is blocked by repository protection or tool limitations:

- Do not bypass protection.
- Do not claim the changes were applied to "main".
- Use the permitted working branch or provide exact changes.
- Clearly report what prevented direct application.

Do not create unnecessary branches or pull requests if direct authorized modification is available.

---

26. FINAL DELIVERY REQUIREMENTS

The final delivery must include:

1. Complete corrected project.
2. All necessary source changes.
3. Updated build configuration.
4. Updated tests.
5. Updated documentation.
6. No incomplete placeholder implementations.
7. No unnecessary duplicate files.
8. No mixture of conflicting old and new versions.
9. A complete change summary.
10. Actual verification results.

If archive creation is available, provide one complete coherent ZIP of the corrected project.

Do not repeatedly send unchanged files.

Do not deliver only a list of filenames when the user needs the corrected project.

---

27. FINAL REPORT FORMAT

At completion, provide a concise but technically complete report containing:

A. Actual defects found

List verified defects, not speculative ones.

B. Changes implemented

List what was actually modified.

C. Core functionality

Report the actual status of:

- CAN1.
- CAN2.
- Dual-CAN.
- OBD-II.
- ISO-TP.
- Monitor.
- Recording.
- Learn.
- DBC.
- Storage.
- OTA.

D. Multi-interface operation

Report actual implementation and verification of:

- TFT.
- Touch.
- Five-way.
- Web.
- BLE.
- USB Serial.
- Simultaneous use.

E. Hardware compatibility

Report each supported ESP32-S3 profile separately.

F. Build and test results

Use PASS / FAIL / NOT RUN / HARDWARE REQUIRED / PARTIAL.

G. Remaining limitations

List every known unresolved issue, its impact, and why it remains.

Do not hide an unresolved critical defect.

H. Repository status

State whether changes were actually applied to "main", or explain the exact limitation.

I. Final project

Provide the complete corrected project archive when possible.

---

28. FINAL ACCEPTANCE CONDITIONS

Do not declare the project complete merely because code changes have been made.

The core completion target requires:

- N16R8 as primary reference.
- Working independent CAN1 and CAN2 architecture.
- Correct MCP2515 8MHz configuration.
- Explicit per-function CAN selection.
- No silent transmission fallback.
- Independent optional hardware initialization.
- Headless operation.
- Web/BLE/USB independence from TFT.
- Simultaneous interface architecture.
- Resource-aware concurrent operation.
- Functional OBD/ISO-TP to the extent claimed.
- Functional Monitor and Recording to the extent claimed.
- Learn data protection.
- DBC integrity.
- Internal storage without SD.
- Optional SD support where implemented.
- No-PSRAM core operation.
- Valid supported partition configurations.
- User-data preservation.
- Safe recovery from invalid configuration.
- Updated truthful documentation.
- Actual builds and tests wherever available.
- Transparent remaining limitations.

If any acceptance condition cannot be completed, implement all safe feasible work and explicitly report the exact remaining limitation.

---

29. FINAL ENGINEERING DIRECTIVE

Do not optimize for the appearance of completion.

Optimize for actual correctness, reliability, and usability.

Do not remove a working capability to make the build easier without proving that the removal is necessary and documenting the impact.

Do not introduce fake hardware detection, fake CAN responses, simulated success states, or placeholder features into production paths.

Do not suppress errors merely to obtain a successful build.

Do not claim that physical hardware works without physical verification.

Complete the actual CarTouch project, verify what can be verified, preserve user data and existing functionality, and deliver the corrected project with an honest final report.