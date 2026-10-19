# Publiczny instalator ADB / Public ADB installer

## Polski

Wersja **0.3.0-preview1** to publiczna aplikacja Windows do istniejącego instalatora. Nie wymaga konta, serwera, opłat ani zatwierdzenia przez Admina. Udostępniamy źródła i EXE. Interfejs obsługuje sprawdzanie, instalację, cofnięcie oraz osobno potwierdzany restart i kontrolę autostartu.

**Paczka aplikacji nie zawiera plików OEM ani kompletnego wsadu Bluetooth.** Do instalacji potrzebny jest lokalny folder ze sprawdzonym `manifest.json` i `payload/`. Repo źródłowe lub sam EXE nie zastępuje takiego folderu. Własne biblioteki OEM trzeba pozyskać z własnej zgodnej jednostki; opis budowy: `docs/BUILD.md`. Zestaw AVRCP przygotowuje się zgodnie z `docs/AVRCP.md`. To wydanie upraszcza obsługę istniejących paczek; nie jest jeszcze kompletnym instalatorem pobierającym i budującym wszystko na dowolnym radiu.

### Obsługa

1. Pobierz publiczne wydanie z https://github.com/ganjaq/Android-Headunit-6125-native-bluetooth/releases i rozpakuj ZIP.
2. Pobierz oficjalne Android SDK Platform-Tools: https://developer.android.com/tools/releases/platform-tools . Wskaż `adb.exe` w aplikacji. Możesz też umieścić folder `platform-tools` obok EXE.
3. W radiu włącz opcje programistyczne i debugowanie USB. Ustaw port USB na **Device/ADB, nie Host**. Podłącz tylko jedno urządzenie i zatwierdź komunikat autoryzacji USB.
4. Wskaż folder sprawdzonej paczki. Opcjonalny profil wybieraj wyłącznie dla przygotowanego wariantu bliźniaka. Naciśnij **Sprawdź radio**. Zaznaczona opcja ADB root restartuje tylko połączenie ADB; nie instaluje Magiska i nie wyłącza SELinux.
5. Jeśli kontrole przejdą, przycisk **Zainstaluj** zostanie odblokowany. Potwierdzenie obejmuje modyfikację systemową, zatrzymanie BLINK przy następnym starcie i posiadanie sprawdzonego kanału odzyskania. Instalator weryfikuje backup przed pierwszym zapisem.
6. Po zakończeniu wybierz osobno **Restart i kontrola**. Muzyka zostanie przerwana. Jeżeli USB wróci do Host, ustaw ponownie Device/ADB. Kontrola trwa do trzech minut. Stan autostartu nie zastępuje odsłuchu ani sprawdzenia tytułów w aucie.

Backupy pozostają lokalnie w `%LOCALAPPDATA%\NEXTGENDRIVE\BluetoothInstaller\backups`. Aplikacja nie wysyła raportów. **Cofnij** przygotowuje powrót OEM według własnego zachowanego baseline i zgodnej paczki; restart potwierdza się osobno. Aplikacja nie usuwa kopii i danych instalacji. Do cofnięcia nie jest potrzebny internet.

Istniejąca modyfikacja jest rozpoznawana, ale nie jest automatycznie aktualizowana. Nie uruchamiaj świeżej instalacji nad nią. Potrzebny jest osobny przegląd zestawu i plan zachowania danych. Program odmawia przy wielu urządzeniach, braku roota, zmianie połączenia lub stanu, niezgodnej jednostce albo paczce. Nie ma opcji „force”. Puste pole paczki pozwala rozpoznać istniejącą instalację, lecz nie umożliwia instalacji.

### Weryfikacja i granice wydania

Testy offline sprawdzają m.in. odmowę przy zmianie urządzenia, stanu i połączenia oraz brak zapisu bez sprawdzenia i potwierdzenia. EXE ma test zasobów `--self-test`, bez ADB. Ścieżka diagnostyczna może odczytać rzeczywiste radio; **cała nowa aplikacja GUI od czystego OEM przez instalację do cofnięcia nie została jeszcze sprawdzona sprzętowo**. Dotychczasowe potwierdzenia muzyki/AVRCP dotyczą opisanej jednostki referencyjnej, a nie każdego kandydata.

### Budowanie EXE

Windows x64, Python 3.11+ z Tk i odizolowane środowisko z `PyInstaller==6.22.3`:

```powershell
python -m venv .local/build-env
.local/build-env/Scripts/python -m pip install PyInstaller==6.22.3
./tools/build_public_installer.ps1 -Python .local/build-env/Scripts/python.exe
```

Ze źródeł: `python tools/ngd_public_installer.py`. EXE zawiera interpreter i interfejs; użytkownik nie musi instalować Pythona. ADB oraz lokalna paczka są osobnymi wejściami. Skrypt budowy nie publikuje danych OEM i nie kontaktuje się z radiem.

## English

**0.3.0-preview1** is a public Windows frontend for the existing guarded ADB installer. No account, server, payment or Admin approval is required. It provides checking, installation, local rollback and a separately confirmed restart/autostart check.

**The EXE download contains no OEM binaries or complete Bluetooth payload.** Installation requires a locally held reviewed kit (`manifest.json` plus `payload/`). A source archive or the EXE alone is not a kit. Obtain OEM runtime inputs from your own compatible unit and follow `docs/BUILD.md`; see `docs/AVRCP.md` for AVRCP kit assembly. This release simplifies existing kits; it does not yet download and build everything for arbitrary head units.

Download/extract the GitHub release. Obtain official Android Platform-Tools from the link above and choose `adb.exe` (or put `platform-tools` beside the EXE). Enable Developer options and USB debugging, set **Device/ADB instead of Host**, connect exactly one radio and accept its USB authorization prompt. Choose your reviewed kit and press **Check**. The ADB root option only restarts the ADB connection; it does not install root software or change SELinux.

**Install** is available only after successful checks. Confirm the system modification, whole-BLINK pause at next startup and your verified recovery method. Original backups are verified before the first device write. Restart is a separate action; USB may need switching back to Device/ADB. The post-boot check waits up to three minutes. Verify actual audio and track titles yourself.

Backups stay in `%LOCALAPPDATA%\NEXTGENDRIVE\BluetoothInstaller\backups`. Nothing is uploaded. **Roll back** prepares OEM return from the device's saved baseline using the matching kit; restart separately. Backups and installation files remain. No internet is needed for rollback.

Existing modifications are detected but not automatically updated. Multiple devices, missing root, changed state/connection or incompatible hardware/payload cause refusal. There is no force option. Twin profiles still require their separately reviewed candidate kits. Leaving the kit field empty can identify an existing installation but cannot install anything.

Offline tests exercise refusal and no-write boundaries. The EXE supports `--self-test` without ADB. A read-only diagnostic can inspect real hardware; **the complete new GUI workflow from clean OEM through installation and rollback has not been hardware-tested**. Previous A2DP/AVRCP results apply to the reference unit.

Build with the commands above on Windows x64, Python 3.11+ with Tk, and isolated PyInstaller 6.22.3. Alternatively run `python tools/ngd_public_installer.py`. The EXE bundles Python and Tk; ADB and the reviewed kit are separate inputs. The build script neither contacts a device nor packages OEM files.
