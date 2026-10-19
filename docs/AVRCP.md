# Tytuły utworów w Audi / Track titles in Audi

## PL

19 października 2026 użytkownik potwierdził muzykę i tytuły utworów w Audi MMI 3G+ z naszego radia trinket/6125. Po jednym restarcie Androida ponownie potwierdził muzykę i tytuły; odczyty potwierdziły automatyczny start Bluetooth i agenta CLI. Ten test nie obejmował pełnego odłączenia zasilania ani instalacji od czystego OEM na drugiej jednostce. Wielogodzinna stabilność nadal wymaga obserwacji.

### Co udostępniamy

- Źródła `NgdAvrcpLegacy.java`, mostu `jni/ngd_avrcp_legacy/` i nadzorcy w `src/`.
- Dokładne poprawki smali z testowanego APK w `patches/avrcp-legacy/`.
- Sprawdzony zestaw sum w [profilu pakietu](../profiles/payload-avrcp-20261019.json) i narzędzie do jego złożenia z lokalnych artefaktów.
- Osobną [naprawę ekranu szczegółów Bluetooth](SETTINGS_NULL_PROFILE.md). Pakiet AVRCP zachowuje bazowy Settings; ta naprawa jest osobnym krokiem.

Tor metadanych: aktywna sesja multimediów Androida → AVRCP Target → most JNI → istniejący interfejs AVRCP Qualcomm → Audi. Dźwięk pozostaje na działającym A2DP Source/SBC. Most korzysta z już zainicjowanego stosu, bez drugiego właściciela HCI. Główny JNI, audio i firmware pozostają te same. Nie włączamy przez to A2DP Sink, HFP ani AVRCP Controller. Potwierdzono tytuły; wszystkie przyciski i rodzaje metadanych nie były osobno testowane.

### Dlaczego wcześniejsza wersja nie przeżyła restartu

Aktualizacja Bluetooth przez `pm install -r` przeniosła aktywny pakiet do `/data/app`. Nadzorca wymaga systemowego pakietu w `/system/app/BluetoothExt`, odpowiednich sum i zamkniętej bramki HCI. Odrzuca `UPDATED_SYSTEM_APP`. Poprawna integracja umieszcza zgodne APK i bibliotekę w istniejącym katalogu candidate przykrywanym podczas startu oraz aktualizuje przypięcia nadzorcy. Nie usuwaj tej kontroli, żeby ukryć konflikt.

Przy podmianie w działającym systemie Package Manager może zachować wyłączony komponent `org.codeaurora.bluetooth/com.android.bluetooth.avrcp.AvrcpTargetService` z poprzednich zasobów. W teście włączono go jawnie. Nowy APK ma `profile_supported_avrcp_target=true`. Nie traktuj samego wpisu w manifeście jako dowodu uruchomienia usługi.

### Złożenie paczki offline

Wymagane są własna kompletna paczka bazowa 0.1.0-rc1 oraz trzy dokładnie sprawdzone artefakty. Repo źródłowe nie zawiera OEM APK ani bibliotek urządzenia. Narzędzie niczego nie pobiera i nie kontaktuje się z ADB:

```sh
python tools/ngd_avrcp_release.py --base-release release --apk /local/BluetoothNGD.apk --controller /local/ngd16_native_bt --bridge /local/libngd_avrcp_legacy.so --out .local/avrcp-release
python tools/ngd_patch.py --release .local/avrcp-release verify-release
python tools/ngd_patch.py --release .local/avrcp-release check
```

Ostatnie polecenie tylko odczytuje radio. Dalej obowiązują [instalacja](INSTALACJA.md), jej zgody, backup i procedura cofnięcia. **To nie jest aktualizator już zmodyfikowanego radia**: istniejąca instalacja jest odrzucana. Na takim urządzeniu potrzebny jest przegląd jego plików i indywidualna integracja z zachowaniem parowania oraz osobnym planem powrotu. Złożenie i weryfikację paczki sprawdzono offline; całego instalatora tego wariantu od czystego OEM nie przetestowano sprzętowo.

Wariant jest przypisany do bazowego profilu sprzętowego. Narzędzia jednostek bliźniaczych nadal tworzą kandydatów bazowego zestawu A2DP/HID; nie dziedziczą potwierdzenia AVRCP. Zmienione artefakty wymagają nowego przeglądu i testu, a nie zmiany sum tylko po to, by instalator je przyjął.

### Budowa i sprawdzenie

Patrz [BUILD](BUILD.md). Most jest osobnym modułem ARM64 `libngd_avrcp_legacy`. Nagłówki Qualcomm definiują konkretny ABI (interfejs 192 B, callbacks 184 B), a nie uniwersalny kontrakt dla każdego radia. Java jest rekonstrukcją testowanej poprawki smali; czystego, bitowo identycznego rebuilda całego APK z eksportu nie wykonano. Zachowano dokładną poprawkę smali, by można było porównać wynik.

Po wdrożeniu sprawdź kod systemowy bez `UPDATED_SYSTEM_APP`, sumy całego zestawu, działający `AvrcpTargetService`, ON/A2DP oraz rzeczywiste tytuły i odsłuch. Następnie przetestuj restart i autostart bez ręcznego uruchamiania usług. Przy błędzie wróć do własnego zachowanego zestawu APK/nadzorcy/bibliotek; nie mieszaj wersji. Nie kasuj parowań. Lokalny problem startu agenta dotyczył też praw jego katalogu tymczasowego (wymagane 0700); nie był powodem przebudowy Bluetooth. Dane logowania agenta nie należą do paczki.

## EN

On 19 October 2026 the user confirmed music and track titles in Audi MMI 3G+ from our trinket/6125 head unit, and confirmed both again after one Android restart. Device readings confirmed automatic Bluetooth and CLI agent startup. A full power removal cycle, a clean-OEM installation on a second unit and long-term stability have not been verified.

The repository shares the Java shim, native bridge, supervisor source, exact tested smali changes and a reviewed artifact inventory. Metadata travels from the Android media session through AVRCP Target and the bridge to the existing Qualcomm AVRCP interface. Music retains A2DP Source/SBC. The bridge reuses the initialized stack; it does not create another HCI owner or enable Sink, HFP or AVRCP Controller. Track titles were confirmed; all controls and metadata types were not separately tested.

The earlier Bluetooth update used `pm install -r`, leaving an updated system app in `/data/app`. The supervisor correctly rejected it: its contract requires the system code path, matching hashes and the HCI gate. The working integration places the matching APK and bridge in the existing boot-bound candidate directory and pins the matching supervisor. Retain that protection. During a live replacement, Package Manager may retain the old disabled state of `org.codeaurora.bluetooth/com.android.bluetooth.avrcp.AvrcpTargetService`; it was explicitly enabled in the device test. The new resource enables Target by default.

Use the three commands above to assemble a kit **offline** from your own complete baseline 0.1.0-rc1 kit and the three exact reviewed artifacts, verify it and perform a read-only device check. OEM binaries are not distributed in this source repository. Follow [installation](INSTALLATION.en.md) and rollback requirements next. The existing installer rejects already modified units; this is not an in-place updater. Existing installations require individual review, preserved pairings and a matching rollback set. Kit assembly was checked offline; the complete clean-OEM installation of this variant was not hardware-tested.

The variant is bound to the reference hardware profile. Twin preparation continues to use the baseline A2DP/HID payload and does not inherit AVRCP validation. Other artifacts require a separately reviewed build and hardware test.

See [BUILD](BUILD.md). `libngd_avrcp_legacy` is a separate ARM64 module, using specific Qualcomm interface/callback layouts (192/184 bytes). The Java source reconstructs the tested smali patch; a clean bit-identical full APK rebuild from the source export has not been performed. The exact smali is retained for comparison. The [Settings null guard](SETTINGS_NULL_PROFILE.md) is separate; this AVRCP kit retains baseline Settings.

Validate the system code path, hashes, active Target service, ON/A2DP, audible music and actual titles, followed by a restart with no manual service starts. Roll back to your own complete preserved set on failure; do not mix revisions or delete pairings. The agent's separate startup issue involved a temporary directory requiring mode 0700; agent credentials are outside this package.
