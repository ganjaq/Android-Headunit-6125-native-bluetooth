# Installation — EN

## Before connecting USB

On the **head unit**, enable Developer options (usually by tapping Build number several times) and **USB debugging**. Set the radio USB port role to **Device / ADB**, not **Host**. OEM labels/menu locations vary. This is the head-unit port role, not a phone USB mode. Accept the RSA authorization prompt for your trusted computer if shown.

`adb devices` must show the radio as `device`, not unauthorized/offline. USB debugging does not grant root: independently confirm firmware support for `adb root` and UID0 from `adb shell id`. Rooting adbd restarts the connection. A Windows device-descriptor failure occurs before working ADB enumeration; it is not a Bluetooth-profile error. Do not switch USB roles during a write. The laptop is needed for installation/diagnostics, not everyday Bluetooth autostart.


1. Read the README, including the whole-BLINK impact. Use stable power and a parked vehicle. This release targets matching firmware and original hashes, not all 6125 products.
2. Obtain a known recovery method for THIS unit from its vendor/manufacturer and working root ADB. `--recovery-confirmed` is your acknowledgement; the tool cannot prove recovery or recover a non-booting unit. It does not reboot into recovery for inspection. The selective backup is not a full system image.
3. Extract the complete bundle containing `release/manifest.json`, `release/payload/`, `tools/` and `profiles/`. Install Python 3.11+ and official ADB. For multiple devices, use `--serial SERIAL` before the action; use `--adb /path/to/adb` for a custom ADB location.
4. Run the read-only checks:

```sh
adb devices
adb root
adb shell id
python tools/ngd_patch.py verify-release
python tools/ngd_patch.py check
python tools/ngd_patch.py plan
```

`check` expects FACTORY OEM state. Refusal on an already modified radio is intentional. Do not erase receipts/baselines to bypass it. Other firmware requires a separate ABI/HCI review, not just an edited fingerprint.

5. If compatible and you accept the exact scope:

```sh
python tools/ngd_patch.py install --accept-system-write --accept-whole-blink-pause --recovery-confirmed
```

The tool first saves pinned originals, BT configuration with metadata, permission flags and settings. No existing installation directory is overwritten. Default private backups are in `.local/backups/` (Git-ignored); `--backups /safe/path` may be supplied before `install`. Backups contain private pairing keys: do not publish them.

The inactive payload is staged and readback SHA-256 verified. On this exact layout the compiled helper temporarily makes dm-3/root writable, reduces ext4's in-memory reserve4096→1024, installs/checks/removes precisely the new init file, then restores the reserve and both RO protections. Only after this exercise passes does it install the final init and activation marker and set the two audio properties to false/true. The existing explicit tile list is preserved while adding `bt`.

6. The installer does NOT reboot. Confirm the success message, backup location and absence of STOP. Once ready for interrupted panel/apps/audio, run:

```sh
adb reboot
```

At boot the whole BLINK service is stopped and the native system package exposed. QTI stays closed until HCI admission checks pass. Do not run old session scripts or /data/app installers.

7. After the panel and ADB return:

```sh
python tools/ngd_patch.py status
```

Require Bluetooth ON, functioning MCU, BLINK stopped and no guard failures. Use Connected devices → Bluetooth/Pair new device, compare pairing codes and select Bluetooth Audio in Audi. Verify real sound, then pair Xbox and test both simultaneously. Finally test an ignition cycle, FM→Bluetooth and manual tile OFF/ON.

STOP on ADB loss, abnormal MCU behavior, foreign HCI ownership, hash/UID mismatch or failed RO restoration. Never force QTI, blindly restart HALs or edit packages.xml. Preserve the backup and STOP message; do not rerun installation over a partial state.

If check refuses an unresolved default tile list (null), open the radio Quick Settings editor and save the current layout, then rerun check. The installer never guesses or replaces the OEM default tile list.
