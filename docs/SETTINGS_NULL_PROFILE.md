# Brakujący profil Bluetooth w ekranie szczegółów / Missing Bluetooth profile in device details

## Polski

### Problem

Na testowanej jednostce wejście w szczegóły sparowanego Audi przez ikonę ustawień powodowało awarię aplikacji Ustawienia. Zapisany stan zgody PBAP dla Audi powodował dodanie `PbapServerProfile`, mimo że aktywna konfiguracja Bluetooth nie tworzyła tego obiektu. `refresh()` wywoływał następnie `LocalBluetoothProfile.isProfileReady()` na wartości `null`.

Wniosek wynika z bytecode aktywnego Settings APK, stanu trwałej zgody oraz zgodnej osi czasu awarii. Nie zachowano zrzutu sterty z chwili wyjątku, więc identyfikacja profilu jest rekonstrukcją ścieżki kodu, nie bezpośrednim odczytem obiektu z pamięci.

### Poprawka i zakres

`patches/settings-pbap-null-guards.patch` dodaje kontrole `null` dla PBAP i MAP podczas budowania listy profili oraz przed wywołaniem `isProfileReady()` w `refresh()`. MAP otrzymuje zabezpieczenie w tej samej ścieżce, chociaż w zgromadzonym stanie Audi nie miało zapisanej zgody MAP.

Poprawka naprawia ekran szczegółów. Nie włącza PBAP, MAP, A2DP Sink ani HFP i nie zmienia parowania. To osobna zmiana względem modyfikacji widoczności Settings opisanej w `patches/settings-visibility.json`.

### Zgodność i weryfikacja

Patch dotyczy klasy zdekompilowanej z istniejącego pakietu Bluetooth-visible Settings. Oczekiwany SHA-256 wejścia to `fed58fe187827f26ee54b618f5f0dbc88f28d5cfc1fa1986bf07208a3569b927`; nie jest to niezmodyfikowany OEM APK, którego hash wynosi `46ea77f8832d248d9c0ed695016d2ecd56caf994fd31946f845d3cc94aec95e6`.

Kandydat testowany na urządzeniu miał SHA-256 `d6357b18acb210aa6274a4f4c0aac3b99e0d2100bd765feda5cfb493ec60f756`. Walidacja DEX po złożeniu i Android ART `verify` przeszły. Aktualizację samego pakietu Settings zainstalowano przez `pm install -r`; menedżer pakietów zwrócił `Success`. Po jednym tapnięciu trybiku przy Audi MMI 5359 otworzył się ekran szczegółów: Audi było oznaczone jako aktywne, a przełącznik Media audio pozostał włączony. W buforze crash po teście nie pojawił się nowy FATAL, `isProfileReady` ani `VerifyError`. Przed i po teście Bluetooth był ON, A2DP Connected i `mIsPlaying=true`; PID stosu Bluetooth, HAL i nadzorcy nie zmieniły się. To pojedynczy test na jednej jednostce, nie test długiej stabilności ani odsłuch.

APK, kopia `Settings.before.apk`, klucz podpisujący i surowe logi pozostają poza repozytorium. Na radiu CLI wykonał instalację pakietu przez `pm install -r`; plik bind-mount w `/data/local/ngd-native-bt` pozostał bez zmian. Kopię i skrypt cofnięcia zachowano prywatnie na radiu.

## English

### Issue

On the tested head unit, opening the paired Audi device details with its settings icon crashed Settings. A persisted PBAP permission caused the details controller to add `PbapServerProfile` although the active Bluetooth configuration had not created that profile object. `refresh()` then called `LocalBluetoothProfile.isProfileReady()` on `null`.

This conclusion is reconstructed from the active Settings bytecode, the persisted permission state, and the correlated crash timeline. No heap snapshot from the exception was retained, so the profile identity was not read directly from the crashed process.

### Fix and scope

`patches/settings-pbap-null-guards.patch` adds null checks for PBAP and MAP while building the profile list, and before `isProfileReady()` in `refresh()`. MAP is guarded along the same path even though Audi had no persisted MAP permission in the captured state.

The fix addresses the device-details screen. It does not enable PBAP, MAP, A2DP Sink, or HFP and does not change pairing. It is separate from the Settings visibility modification described by `patches/settings-visibility.json`.

### Compatibility and verification

The patch targets the class decompiled from the existing Bluetooth-visible Settings package. The expected input SHA-256 is `fed58fe187827f26ee54b618f5f0dbc88f28d5cfc1fa1986bf07208a3569b927`; this is not the unmodified OEM APK, whose hash is `46ea77f8832d248d9c0ed695016d2ecd56caf994fd31946f845d3cc94aec95e6`.

The device-tested candidate SHA-256 was `d6357b18acb210aa6274a4f4c0aac3b99e0d2100bd765feda5cfb493ec60f756`. DEX round-trip validation and Android ART `verify` passed. The Settings package update was installed with `pm install -r`, which returned `Success`. One tap on the Audi MMI 5359 settings icon opened device details: Audi was shown as active and Media audio remained enabled. No new FATAL, `isProfileReady`, or `VerifyError` appeared in the crash buffer after the test. Bluetooth remained ON, A2DP Connected, and `mIsPlaying=true`; Bluetooth stack, HAL, and supervisor PIDs did not change. This is one test on one unit, not a long-duration stability test or a listening test.

The APK, `Settings.before.apk`, signing key, and raw logs remain outside the repository. The on-device CLI installed the package with `pm install -r`; the bind-mounted file in `/data/local/ngd-native-bt` was left unchanged. The backup and rollback script remain private on the head unit.
