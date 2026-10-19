# Changelog

## Unreleased - 2026-10-19

- Added public Windows ADB frontend 0.3.0-preview1 with local checking, installation, rollback and separately confirmed restart. No Customer/Admin approval backend or account required.
- Added device/connection/state binding and offline tests for frontend refusal boundaries, plus full baseline payload inventory pinning. Public EXE excludes OEM payload and uses existing reviewed local kits; existing modifications require separate update review.

- Published the device-tested legacy Qualcomm AVRCP bridge and exact smali changes; user confirmed music and track titles in Audi MMI 3G+ before and after an Android restart.
- Integrated matching APK/bridge pins into the supervisor, retaining the system-code-path and HCI gates.
- Added an offline assembler and full reviewed inventory for the reference-profile AVRCP development variant. Clean-OEM installation of this variant and long-term stability remain unverified.
- Preserved baseline twin preparation with explicit baseline APK pins; AVRCP validation is not inherited by other units.

- Added the Settings PBAP/MAP null-profile guard patch and documented its single-unit device validation; no unsupported Bluetooth profiles are enabled.

## Unreleased - 2026-10-18

- Consolidated compatibility documentation and refreshed offline validation.
- Added three selected user-provided test photographs without EXIF or notification/account content.

- Added the P30-tested charging-to-hotspot companion APK, matching source, build script and PL/EN guide.
- Added explicit head-unit Developer options / USB debugging / Device-not-Host setup instructions.

- Added read-only trinket/Cherokee twin assessment independent of full firmware fingerprint identity.
- Added per-target native source preparation, ARM64 build markers, release/profile binding and candidate-kit sealing.
- Added explicit target-profile installation/removal and unverified-hardware acknowledgement; reference checks remain.
- Added project compatibility skill, PL/EN workflow and offline negative tests.
- No second-unit hardware validation or universal firmware support claimed.


## 0.1.0-rc1 — 2026-10-16

- Packaged the exact sound/HID-confirmed ETAP17 APK, JNI and guard.
- Restored native Android10 system integration using RAM bind mounts and bounded init installation.
- Added strict one-firmware compatibility profile, per-unit backup and explicit OEM-return preparation.
- Added Polish/English usage and recovery instructions and offline installer guard tests.
- Owner confirmed ignition-cycle Xbox/Audi reconnect and sound; last cycle not classified cold boot/resume.
- Release installer remains a new offline-tested integration, not a universally hardware-validated installer.
