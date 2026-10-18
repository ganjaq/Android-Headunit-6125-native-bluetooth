# Rollback — EN

Returning to OEM is a separate explicit operation. Normal tile OFF keeps the supervisor available for the next ON and is not an uninstall.

For a unit installed with THIS NEW distribution installer, inspect logs and obtain root ADB, then run:

```sh
python tools/ngd_patch.py remove --accept-oem-return
```

It checks the profile, exact owned init/controller and baseline, requests controlled native closure and requires no remaining native process or transaction journal. It removes only its marker and matching init using the bounded RO-restoring helper, restores recorded audio properties/settings/package enabled state and checks permissions through the existing helper. All backups and /data payload are preserved. It does not uninstall an APK, alter UID/data ownership or start BLINK under a live executable mask.

Only when ready, explicitly run:

```sh
adb reboot
```

RAM mounts disappear; originals remain on disk and init no longer blocks BLINK. Check panel/MCU, OEM Bluetooth and audio. The new combined OEM-return flow has not been tested on a second unit. Exact init install/remove and own-APK/controller restoration were exercised separately on the radio.

If native closure cannot be confirmed, the unit fails to boot or ADB is lost, this script cannot automatically recover it. Use the previously established recovery procedure for the exact model. Do not flash another model's images or delete packages.xml. A backup does not itself provide a recovery transport.

The original development radio uses an older installation baseline without `distribution-baseline.json`; the generic remover intentionally refuses it. Use that installation's original recorded baseline/procedure. The development HID→audio-only rollback is not a complete OEM return.
