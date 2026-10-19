# Android-Headunit-6125-native-bluetooth

**Public Windows ADB installer — no account or Admin approval:** [download](https://github.com/ganjaq/Android-Headunit-6125-native-bluetooth/releases), [PL/EN guide](docs/PUBLIC_INSTALLER.md). The EXE uses a local reviewed kit; OEM binaries are not included. Preview release; a complete GUI installation on a clean unit is not yet hardware-verified.

**19 October 2026: music and AVRCP Target track titles work in Audi MMI 3G+, including after an Android restart.** [Source, matched kit and PL/EN instructions](docs/AVRCP.md). Confirmed on one unit; the new variant's complete clean-OEM installation has only been checked offline.

[Audi Bluetooth device-details crash fix](docs/SETTINGS_NULL_PROFILE.md)

A project developed using vibe coding - Native Android 10 Bluetooth for the qti/trinket/ QKQ1.200816.002 / eng.zouqiao.20250217.185927/ Qualcomm 6125 / BLINK_6125 head unit - the NEXTGENDRIVE project.

[Polski](README.md) · [User guide](docs/USER_GUIDE.en.md) · [Installation](docs/INSTALLATION.en.md) · [Rollback](docs/ROLLBACK.en.md) · [Source and build](docs/BUILD.md)

The patch partially restores Android system Bluetooth, A2DP Source/SBC to the car's factory system, and adds HID-Host support for Xbox One Bluetooth controllers. It uses the existing Qualcomm HAL and libbluetooth_qti.so. It does not install a second hardware stack or Bluetooth firmware.

## What has been confirmed

On the QKQ1.200816.002 - BLINK_6125 unit, we managed to play audio from the Android head unit through the car's audio system using A2DP directly from the device, without cables. Simultaneous operation of an Xbox One Bluetooth controller, manually switching Bluetooth OFF/ON, and automatic connection between the car's Bluetooth unit and the head unit after restarting the car have been confirmed. In the latest test, after switching the car off and on, the controller connects immediately, Audi connects automatically after about 30 seconds, and audio works. The patch is still being tested, but everything points to us finally having regained the Android Bluetooth stack for our own use (e.g. OBDeleven, etc.).

**The new distribution installer is release 0.1.0-rc1, tested offline.** Working components and individual operations were tested on the head unit; the entire new procedure from clean OEM through installation and rollback has not been repeated on a second unit. Long-duration stability and all MCU functions have not been confirmed.

## Compatibility

A “Qualcomm 6125” label, Android 10, or the appearance of the enclosure is not enough. The default package uses the verified profile `blink6125-qti-20250217-arm64-a`. For hardware-twin units with a different fingerprint, [assessment and preparation of a separate variant](docs/BLIZNIACZE_JEDNOSTKI.md) have been added; we do not require the entire firmware to be identical. Base fingerprint:

```text
qti/trinket/trinket:10/QKQ1.200816.002/eng.zouqiao.20250217.185927:userdebug/test-keys
```

Identical SHA-256 hashes of the original BluetoothExt.apk, blink, libbluetooth_qti.so and Settings.apk, as well as HAL/audio, ARM64/API29, shared UID1002, ADB root, SELinux Permissive, slot _a and the verified ext4/dm-3 configuration are required. The installer refuses when there are differences. There is no “force” option. [Compatibility profile](profiles/blink6125-qti-20250217-arm64-a.json).

## How it works

Data and the project's own files go into `/data/local/ngd-native-bt`. The only system file added is `/system/etc/init/ngd-native-bt.rc`. At boot, init recreates RAM bind mounts over BluetoothExt, Settings and the BLINK executable. OEM originals remain on the partitions. The project's JNI gets access to Qualcomm only after HCI checks, MCU/audio readiness and supervisor authorization. The gate is closed by default; there is no normal session limit or dependency on a laptop.

**The entire BLINK service is stopped, not just its Bluetooth part.** Its CarPlay/Android Auto and other functions may stop working. mcuserver is not stopped separately; this does not prove the safety of every function of a particular head unit. A supervisor error shuts down native Bluetooth without automatically starting OEM.

## Quick start

Python 3.11+, official ADB in PATH, one head unit connected by USB. First run `adb devices` and, if the device supports it, `adb root`. Do not run the installer on an already modified head unit.

```sh
python tools/ngd_patch.py plan
python tools/ngd_patch.py verify-release
python tools/ngd_patch.py check
```

`plan` and `verify-release` do not contact the head unit; `check` only reads. Then follow the [full instructions](docs/INSTALLATION.en.md). The installer does not reboot automatically. Installation through /data/app, pm uninstall, flashing, UID repair and disabling SELinux are not used.

## Repository versus installation package

The repository contains source, the profile, patches, tools and documentation. `release/` and `dist/` are excluded from Git: the local package also contains modified OEM Settings and libraries taken from the head unit. We do not present these OEM binaries as our own Apache code and do not publish them automatically. The Git source archive does not replace a package with a manifest. [Contents and provenance](docs/PROVENANCE.md).

No pairing data, accounts, the original 137 backup files or raw user logs go into the repository or distribution. Backups are created separately on each device and computer.

AOSP code retains its headers and Apache-2.0; the remaining project source files are provided under Apache-2.0. The project is not a product of Audi, Qualcomm or Microsoft.


## Other units and a skill for Codex

Rejection by the current profile does not prove hardware incompatibility. The [procedure for assessing other units](docs/BLIZNIACZE_JEDNOSTKI.md) and the [ngd-headunit-compatibility skill](.agents/skills/ngd-headunit-compatibility/SKILL.md) walk through hardware, ABI/JNI, HCI, audio, a new profile, testing and rollback. The current installer has not been unlocked for unverified configurations.


## Internet from the phone - hotspot when a charger is connected

The [NGD Hotspot app with APK, source and instructions](apps/hotspot-on-charge/README.md) has been added, tested on P30 Pro / Android10. Install it on the phone, not the head unit. [Before installing Bluetooth, prepare USB Device/ADB on the head unit](docs/INSTALLATION.en.md#before-connecting-usb).


## Test photographs

[Gallery: head unit, mainboard and Xbox controller](docs/GALERIA.md).
