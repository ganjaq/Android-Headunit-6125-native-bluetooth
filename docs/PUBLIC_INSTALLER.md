# Kompletny instalator ADB / Complete ADB installer

## Polski

**0.3.0-rc1 zawiera wszystkie składniki potrzebne po stronie komputera:** aplikację Windows, oficjalne ADB 37.0.1, nasze APK/części APK, JNI, most AVRCP, nadzorcę i pomocniki oraz przepisy lokalnego odtworzenia kompletnych APK. Nie trzeba instalować Pythona, Javy, Android Studio ani wybierać dodatkowego folderu payload. Nie ma konta, serwera, opłat ani zgody Admina.

### Pobierz → podłącz → sprawdź → zainstaluj

1. Pobierz ZIP wydania `v0.3.0-rc1` z https://github.com/ganjaq/Android-Headunit-6125-native-bluetooth/releases i rozpakuj go. Uruchom `NGD-Bluetooth-Installer.exe`.
2. Na radiu włącz **opcje programistyczne i debugowanie USB**, ustaw port w trybie **Device/ADB, nie Host**. Podłącz tylko jedno radio kablem danych i zatwierdź jego komunikat autoryzacji USB. Windows musi już rozpoznawać interfejs ADB; aplikacja nie instaluje sterowników.
3. Kliknij **Sprawdź radio**. Program uzyska dostępny root przez ADB, sprawdzi zgodność i odczyta pięć oryginalnych plików: Settings.apk oraz cztery biblioteki z `/system/lib64`. Następnie lokalnie przygotuje pełną paczkę. W tym kroku **nie zapisuje niczego na radiu**; `adb root` restartuje tylko połączenie ADB.
4. Po wyniku READY kliknij **Zainstaluj**. Potwierdź modyfikację systemową, zatrzymanie BLINK przy następnym starcie oraz posiadanie sprawdzonego sposobu odzyskania radia. Przed pierwszym zapisem instalator ponownie sprawdza urządzenie/paczkę/stan i wykonuje zweryfikowany backup.
5. Po przygotowaniu instalacji kliknij osobno **Restart i kontrola**. Muzyka zostanie przerwana. Jeżeli USB wróci do Host, ustaw ponownie Device/ADB. Kontrola trwa do trzech minut. Na końcu sprawdź odsłuchem muzykę i tytuły w Audi.

Opcje wyboru własnej paczki, własnego ADB i profilu bliźniaka są schowane w **Opcjach zaawansowanych**. Do podstawowej instalacji profilu referencyjnego nie są potrzebne.

### Dlaczego OEM nie jest w pobraniu, a paczka jest kompletna

Nie dołączamy cudzych bibliotek ani całego zmodyfikowanego OEM Settings. Zamiast tego dołączamy kod projektu oraz receptury:

- `bluetooth.blob` zawiera kod/zasoby naszej aplikacji oraz metadane ZIP, z usuniętą zawartością czterech bibliotek OEM. Instalator uzupełnia te miejsca oryginałami z radia.
- `settings.blob` zawiera wyłącznie nagłówki ZIP, wyrównanie, centralny katalog i metadane istniejącego podpisu. **Żaden payload wpisu Settings nie jest w tym pliku.** Każdy wpis pochodzi z własnego OEM APK; w DEX zmieniany jest sprawdzony warunek widoczności Bluetooth oraz jego sumy wewnętrzne.
- Całe APK i kompletna paczka muszą na końcu mieć dokładnie zapisane, testowane SHA-256. Zachowujemy ich istniejące podpisy; program nie potrzebuje prywatnego klucza podpisującego ani narzędzi Java.

Nie jest to budowanie dowolnego APK na nieznanym sprzęcie. Przy innej bazie lub wyniku program zatrzymuje się przed zapisem. Przygotowany zestaw to dokładnie wariant `avrcp-legacy-20261019`: Source/SBC, HID, GATT i AVRCP Target. Osobna poprawka null PBAP/MAP w Settings jest opisana w `SETTINGS_NULL_PROFILE.md`; automatyczna receptura zachowuje bazowy, testowany Settings z zestawu AVRCP.

### Zgodność, dane i cofnięcie

Automatyczna ścieżka dotyczy referencyjnego `blink6125-qti-20250217-arm64-a`, Androida 10/API29, ARM64 oraz jego sprawdzonych komponentów, partycji i uprawnień. Sam napis 6125 nie wystarcza. Dla bliźniaka z innym profilem nadal trzeba przygotować osobny sprawdzony wariant; nie ma „force”. Istniejące modyfikacje są wykrywane i nie są nadpisywane jako świeża instalacja.

Wszystko pozostaje lokalnie w `%LOCALAPPDATA%\NEXTGENDRIVE\BluetoothInstaller`: oryginały do rekonstrukcji w `prepared`, pełne backupy w `backups`, wskazanie zgodnej paczki w `receipts`. Nic nie jest wysyłane. Zachowaj ten katalog.

**Cofnij** automatycznie wybiera własny zachowany zestaw dla tego radia. Przy braku lokalnej paczki potrafi odtworzyć ją ponownie z zachowanego OEM widoku Settings i oryginalnych bibliotek na radiu, po sprawdzeniu zgodności nadzorcy/init i baseline. Następnie istniejąca procedura przygotowuje powrót OEM. Restart potwierdza się osobno. Kopie i dane nie są usuwane; internet nie jest potrzebny.

### Co sprawdzono

Pełną rekonstrukcję z rzeczywistych lokalnych oryginałów porównano z całym testowanym zestawem: oba APK i wszystkie pozostałe pliki są identyczne bitowo. Test obejmuje również przebieg odczytu/pull pięciu plików przez kontrolowany adapter ADB bez poleceń zapisu. Testy negatywne obejmują niezgodne pliki OEM, uszkodzone składniki, zmianę połączenia/stanu i brak zgody. Gotowy EXE podlega temu samemu testowi rekonstrukcji oraz testowi uruchomienia interfejsu.

**Nie wykonano nowej pełnej instalacji GUI i cofnięcia od czystego OEM na drugiej jednostce.** To wydanie RC, nie deklaracja uniwersalnej zgodności. Wcześniejsze potwierdzenia odsłuchu, tytułów i restartu dotyczą jednostki referencyjnej. Stan ON/Connected nie zastępuje odsłuchu.

### Budowa ze źródeł

Windows x64, Python 3.11+ z Tk oraz PyInstaller 6.22.3. Składniki projektu są w `installer/components`, a ich przypięcia w `profiles/public-components-20261020.json`. Pobierz oficjalne Platform-Tools 37.0.1 z adresu zapisanego w `profiles/public-adb-3701.json`; skrypt sprawdza ich pliki przed dołączeniem:

```powershell
python -m venv .local/build-env
.local/build-env/Scripts/python -m pip install PyInstaller==6.22.3
./tools/build_public_installer.ps1 -Python .local/build-env/Scripts/python.exe -AdbDirectory C:/tools/platform-tools
```

`tools/build_component_bundle.py` odtwarza publiczne składniki z prywatnej sprawdzonej paczki i OEM Settings, jednocześnie sprawdzając pełny roundtrip. Nie eksportuje danych parowania ani kluczy. Wynik zmienionej kompilacji wymaga nowego przeglądu, przypięć i testów.

## English

**0.3.0-rc1 includes the complete PC-side toolchain:** Windows application, official ADB 37.0.1, project APK portions/JNI/AVRCP bridge/supervisor/helpers and local APK reconstruction recipes. No Python, Java, Android Studio, separately supplied payload folder, account, server or Admin approval is required.

Download/extract the release ZIP and run the EXE. Enable Developer options and USB debugging, select **Device/ADB instead of Host**, connect exactly one radio and accept its USB authorization prompt. Windows must already recognize its ADB interface; drivers are not installed by this application.

Press **Check**. The application requests available ADB root, validates the reference target and reads five original OEM files: Settings.apk and four runtime libraries. It prepares and verifies the complete kit locally. This step makes no device file writes; requesting ADB root only restarts the ADB connection. Press **Install** after READY and confirm the system modification, whole-BLINK pause and your verified recovery method. Backups and fresh state checks precede device writes. Restart is separately confirmed; restore Device/ADB if the radio returns USB to Host. Verify audible music and actual track titles yourself.

The download contains no OEM runtime library payload or complete modified OEM Settings. The Bluetooth recipe combines project code/resources with libraries read from your own radio. The Settings recipe contains only ZIP/signing metadata and reconstructs every entry from your own OEM APK, applying the reviewed visibility-byte change and DEX checksums. **Both complete APKs and every kit file must match the existing reviewed SHA-256 values exactly.** Existing signatures are retained; no private signing keys or Java tools are needed.

The automatic path targets the reviewed reference profile, not every 6125-branded unit. Other twin profiles need their individually reviewed kits through Advanced options. Existing modified units are detected rather than overwritten. The output remains exactly `avrcp-legacy-20261019`; the separate Settings PBAP/MAP null guard is documented separately and is not silently introduced here.

Original inputs, prepared kits, backups and local kit receipts stay under `%LOCALAPPDATA%\NEXTGENDRIVE\BluetoothInstaller`; nothing is uploaded. **Roll back** selects the saved kit automatically, or reconstructs it from the device's preserved OEM view when local files are unavailable, subject to controller/init/baseline checks. OEM return uses the existing guarded procedure; confirm restart separately. Keep your local backups. No internet is needed for rollback.

The complete reconstruction was verified byte-for-byte against real privately held originals and the whole hardware-tested kit, including a read/pull-only controlled ADB adapter test. The packaged EXE undergoes the same reconstruction and GUI startup checks. **A fresh full GUI installation/rollback on a second clean-OEM unit has not been performed.** This is an RC with complete inputs, not a claim of universal hardware support. Build commands and pinned official ADB provenance are above; project source is Apache-2.0 with retained upstream notices.
