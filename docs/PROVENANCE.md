# Pochodzenie / Provenance

- Complete public installer 0.3.0-rc1: `installer/components` contains project/AOSP binaries, a Bluetooth APK fragment excluding its four OEM library payloads, and Settings ZIP/signing metadata only. Every Settings entry payload is reconstructed from the owner's original APK; the sole code transformation is the already reviewed visibility flag/checksums. No signing private key is required or distributed. SHA-256 of both reconstructed APKs and all runtime files stays identical to the reviewed AVRCP kit. `profiles/public-components-20261020.json` records every distributed component and required local OEM input.
- Official bundled ADB 37.0.1 is pinned in `profiles/public-adb-3701.json`, downloaded from Google's repository and checked against its repository metadata; original ADB notices are retained. It is separate from project Apache-2.0 code. The public installer makes no OEM-data upload.

- AVRCP bridge: project source from the locally tested 2026-10-19 integration, with the exact tested smali preserved in `patches/avrcp-legacy`. Java is an equivalent source reconstruction, not a claim of a clean bit-identical rebuild. Reviewed public hashes are in `profiles/payload-avrcp-20261019.json`; OEM payload and raw device logs remain local.
- Qualcomm extension headers retain their original notices. Upstream reference: [AOSPA Bluetooth extension, quartz](https://github.com/AOSPA/android_vendor_qcom_opensource_commonsys_bluetooth_ext/blob/quartz/vhal/include/hardware/bt_rc_ext.h). These layouts were checked for the tested runtime; their existence is not evidence of ABI compatibility elsewhere.

- Open-source AOSP Android 10 package baseline: Android10 r47 workflow; packages/apps/Bluetooth commit `fb756ca94cca87aa2d4e95431290ed4228a1bf94` as recorded in the local source checkout.
- Current guard source: ETAP17 including ETAP16 /proc captured-inode race fix, exact-cookie hwbinder ownership lookup and renewable root leases. No guessed HIDL virtual calls.
- JNI/Java: Qualcomm callback/layout integration, ScanParameters correction, host BLE filtering instead of unverified APCF, SBC offload mapping, AdapterService/GattService/A2dpService and HID Host enabled.
- Original platform certificate fingerprint: `c8a2e9bccf597c2fb6dc66bee293fc13f2fc47ec77bc6b2b0d52c11f51192ab8`. This is the Android platform test certificate used by the verified OEM build. No private signing key is included.
- OEM original `libbluetooth_qti.so`, BLINK and original BluetoothExt are NOT distributed or replaced on disk. Profiles only contain their hashes.
- The local binary bundle DOES contain a modified OEM Settings.apk plus runtime ELF dependencies originally extracted from the radio: libbluetooth-binder.so, libchrome.so, libnativehelper.so and libevent.so. Their licenses/provenance are separate from the project's source license. They are Git-ignored and must not be automatically pushed as project-owned binaries.
- `patches/settings-pbap-null-guards.patch` records a small AOSP-derived Settings bytecode fix against the exact intermediate Bluetooth-visible APK hash. The tested APK, signing key, private backup and raw logs are excluded; see `docs/SETTINGS_NULL_PROFILE.md` for scope and device evidence.
- `release/manifest.json` records exact tested payload hashes. Source rebuild output is not automatically trusted as that tested release. Hashes detect changes but are not a publisher signature: obtain the release and external SHA256 from a trusted channel.
- No pairing keys, personal accounts, original selective backup, baseline receipts or user logcat/btsnoop are in the distributable repository/bundle. Installer-created private backup directories must remain private.

## Evidence boundaries

One physical radio, multiple live tests and owner-confirmed sound/input. Latest owner report: ignition-cycle auto reconnection succeeds for Xbox and Audi (~30 s) with audio. Not a guarantee for every branded6125 unit, every MCU function, future firmware, suspend/resume state or long-duration stability. The newly assembled installer was validated offline, not by reinstalling the working radio.
