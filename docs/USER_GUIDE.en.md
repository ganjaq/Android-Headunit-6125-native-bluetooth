# User guide — NEXTGENDRIVE native Bluetooth

[Polski](OBSLUGA.md) · [Installation](INSTALLATION.en.md) · [OEM return](ROLLBACK.en.md)

This guide is for a compatible radio on which the patch has already been installed correctly. Downloading the repository does not install Bluetooth. A new installation requires the separate compatibility check and installation procedure.

## 1. Everyday startup

1. Switch on the vehicle/radio and the factory Audi system.
2. Wait for Android to start and Bluetooth to turn on automatically.
3. Select Bluetooth Audio in Audi and start music in an Android app such as YouTube or a media player.

Once paired, you do not need to search for devices or connect a laptop every time. In the confirmed ignition-cycle test, Audi reconnected automatically after approximately 30 seconds and Xbox almost immediately. Timing depends on both devices being ready. This is not a session deadline: normal operation has no 60/120/300-second test countdown.

All deep-sleep and complete power-loss scenarios have not been tested separately. Firmware updates require a new compatibility review.

## 2. Finding Bluetooth

- **Bluetooth tile:** expand Quick Settings at the top of the screen; the tile sits alongside Wi-Fi and the other switches.
- **Device menu:** Settings → Connected devices → Connection preferences → Bluetooth.
- **Add a device:** choose Pair new device in the connected-device settings. Exact layout and translated labels depend on OEM Settings.

If the entry is missing, search Settings for “Bluetooth”. Report a missing tile/menu after installation with the firmware version; do not blindly reinstall. During development the pairing screen was searchable even when its OEM menu entry was hidden.

## 3. First pairing with Audi

1. Enable Bluetooth on the radio and open the new-device pairing screen.
2. Enable Bluetooth discovery/pairing in Audi. Select the radio in Audi, or select Audi on the radio.
3. Compare the code on both screens. Confirm only matching codes. If a code is missing or different, cancel that attempt and start again.
4. After connection, select Bluetooth Audio in Audi and play music on Android.

The test radio initially appeared as **QCOM-BTD**, later **Audi android**. Your unit may use another name: check its Bluetooth screen. An entry in the paired-device list means a stored bond, not an active audio connection.

If saved Audi does not reconnect automatically, select it in the radio's Bluetooth settings and choose Connect, or select the saved radio in Audi. Do not immediately delete the pairing. If manual connection is required on every startup, collect `status` output and report the behavior.

## 4. FM and returning to Android audio

You can listen to factory FM with the radio's Bluetooth enabled. When you want Android audio, select Bluetooth Audio in Audi and start/resume playback on Android. FM-to-Bluetooth switching was tested with a small delay.

Enabled Bluetooth allows a previously paired device to connect. It does not imply permanent discoverability to new devices. Open the appropriate Settings screen for new pairing; unlimited background discoverability is not promised.

## 5. Manual OFF and ON

1. Tap the Bluetooth tile to switch Bluetooth off. Audi and the controller disconnect.
2. Wait until the switch shows OFF.
3. Enable Bluetooth with the same tile and wait for reconnection.

Manual OFF/ON followed by audible Audi playback was tested. Switching off does not erase bonds or uninstall the patch. Persistence of a deliberate OFF choice across every type of restart has not been verified: the current boot integration is designed to enable Bluetooth automatically when the radio starts.

## 6. Xbox controller

1. For the initial test, connect Audi first and verify music.
2. Turn on Xbox and use its pairing button to enter pairing mode; the logo flashes.
3. Open Pair new device on the radio and select Xbox Wireless Controller.
4. Wait for connection, then test the D-pad/buttons in a menu or a controller-aware app.

The tested controller reconnected automatically when turned back on after pairing. Simultaneous Xbox input and Audi music were confirmed; a one-device-only limitation has not been demonstrated. A brief audio interruption occurred while adding the controller. Not every app supports gamepads; every button, vibration feature and controller model has not been independently verified.

If the logo keeps flashing, open the saved controller's details and choose Connect. “Paired” alone does not prove an active HID connection. If the controller is connected to a console or another device, disconnect it there before trying the radio again.

## 7. No sound

1. Check that Audi shows an active radio connection, not just a saved-device entry.
2. Select Bluetooth Audio in Audi and check volume/mute.
3. Ensure playback is running on Android. Resume it after the connection completes.
4. If there is still silence or Bluetooth turns OFF unexpectedly, collect the state below. Do not arbitrarily change offload properties or restart audio services.

Volume, an active connection and a moving playback bar do not replace a listening test. During development A2DP could connect while its audio route was still broken.

## 8. Diagnostics and reporting

On a laptop with working authorized ADB, this command only reads state:

```sh
python tools/ngd_patch.py status
```

Provide the release and firmware versions, Audi/controller startup order, approximate failure time, whether Bluetooth stayed ON, whether the touchscreen worked and whether sound returned. `status` does not enable Bluetooth, reset the radio or restore BLINK. ADB is unnecessary for everyday use.

Remove device names/addresses and other personal data before posting output publicly. Never upload `.local/backups`, snapshot/receipt files, pairing keys or raw logcat/btsnoop.

This version primarily provides A2DP Source/SBC, GATT and HID Host. It does not certify HFP calls, microphone, contacts or all CarPlay/Android Auto functions. The whole factory BLINK is stopped and its functions are affected. OEM return is a separate [rollback procedure](ROLLBACK.en.md), not normal tile OFF.


## USB / ADB

For service access, enable head-unit Developer options and USB debugging, and select USB **Device/ADB**, not Host; see [USB preparation](INSTALLATION.en.md#before-connecting-usb). Everyday Bluetooth and the hotspot companion do not require a connected laptop.
