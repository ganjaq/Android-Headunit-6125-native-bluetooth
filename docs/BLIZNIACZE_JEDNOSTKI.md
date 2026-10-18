# Jednostki bliźniacze / hardware twins

## Co rozszerzono

Domyślna paczka nadal rozpoznaje profil referencyjny. Nowe narzędzie `tools/ngd_twins.py` umożliwia ocenę innej jednostki i przygotowanie osobnego, instalowalnego wariantu dla innego fingerprintu. Nie wymaga identycznego obrazu całego firmware, nazwy producenta ani obudowy. Nie ma opcji force.

Pierwsza automatyzowana rodzina to Android 10/API29 ARM64, platforma trinket, kontroler deklarowany jako Cherokee, z identycznymi zweryfikowanymi **komponentami integracji Bluetooth/audio/Settings** wskazanymi w profilu bazowym oraz tym samym układem instalacji. To węższe niż cały obraz systemu, ale bardziej konkretne niż sam procesor. Properties są deklaracją firmware, nie fizycznym pomiarem kontrolera; potwierdzenie sprzętowe pozostaje wymagane.

Inna biblioteka QTI/HAL, Settings, usługa BLINK, polityka audio lub układ partycji daje `NEEDS_EVIDENCE`, nie deklarację niezgodności sprzętu. Wtedy użyj `$ngd-headunit-compatibility`: porównanie ABI/interfejsów i dostosowanie integracji może dodać następną rodzinę. Nie przypisuj po prostu nowego hasha do profilu. Zwykłe dopisanie fingerprintu do listy akceptowanych nie wystarcza.

## 1. Ocena: tylko odczyty urządzenia

Radio w stanie OEM; jedna jawnie wybrana jednostka ADB. Użytkownik osobno uruchamia wspierane `adb root`. Narzędzie nie restartuje adbd, nie wysyła plików, nie otwiera HCI i niczego nie instaluje.

```sh
python tools/ngd_twins.py assess --serial SERIAL --output .local/twin-assessment.json
```

Wynik zbiera hashe wszystkich przypiętych komponentów i różnice. Oprócz rodziny SoC/kontrolera wykonuje dotychczasowe kontrole UID, stanu OEM, MCU, audio, SELinux, systemu plików i braku wcześniejszej instalacji. `TECHNICAL_CANDIDATE` znaczy kandydat do próby, nie sprawdzony sprzęt. Inny fingerprint jest zapisywany jako cel zamiast powodować automatyczne odrzucenie. Brak/rozbieżność identyfikacji Cherokee w `qcom.bluetooth.soc`/`vendor.qcom.bluetooth.soc` wymaga dowodów i przeglądu, nie zgadywania.

## 2. Przygotowanie źródeł konkretnego celu: offline

```sh
python tools/ngd_twins.py prepare --assessment .local/twin-assessment.json --id trinket-moj-wariant --out .local/twin-target
```

Powstają `target_profile.json`, kopia `src/`, inwentarz hashy źródeł i instrukcja `BUILD_TARGET.md`. Nie nadpisuje istniejącego katalogu. Dotychczasowe profile i payload zostają bez zmian. Dowody testów Audi/Xbox z innej jednostki nie są dziedziczone.

Zmiana fingerprintu trafia do obu narzędzi urządzenia: kontrolera sesji i pomocnika instalacji init, także ich kopii w eksporcie aplikacji. Przejrzyj różnice. Buduj te dwa moduły ARM64 w istniejącym AOSP zgodnie z [BUILD.md](BUILD.md), z przygotowanych źródeł. Nie używaj starego kontrolera z nowym JSON. APK/JNI i sprawdzone biblioteki nie wymagają przebudowy dla tej rodziny.

Oba narzędzia zawierają odczytowy `--print-target`, zwracający `NGD_TARGET_FINGERPRINT=...`. Kontrola ELF i znacznika wykrywa pomylenie kompilacji; nie jest podpisem kryptograficznym, dowodem pochodzenia kompilacji ani testem działania.

## 3. Osobny pakiet kandydujący: offline

Po przeglądzie źródeł, logów i kompilacji:

```sh
python tools/ngd_twins.py seal --workspace .local/twin-target --controller /build/ngd16_native_bt --installer /build/ngd16_init_installer --base-release release --out .local/twin-kit --ack-reviewed-build
python tools/ngd_patch.py --profile .local/twin-kit/profile.json --release .local/twin-kit verify-release
python tools/ngd_patch.py --profile .local/twin-kit/profile.json --release .local/twin-kit plan
```

`seal` kontroluje inwentarz źródeł, referencyjną paczkę, ELF64/AArch64 i znacznik celu obu narzędzi. Kopiuje bazowy payload, zmienia tylko te dwa pliki, tworzy osobne hashe i manifest związany z pełnym profilem. Nie kopiuje danych użytkownika. Nie oznacza wydania jako sprawdzonego sprzętowo. Nie overwrituje istniejącego zestawu.

Opcjonalna paczka lokalna (zawiera OEM; nie publikuj automatycznie):

```sh
python tools/package_release.py --profile .local/twin-kit/profile.json --release .local/twin-kit
```

Archiwum zawiera `target-profile.json`; do komend instalatora podaj ten plik jako `--profile`. Używaj sparowanego release/profile także podczas cofania. Zmiana profilu po seal unieważnia powiązanie manifestu.

## 4. Próba na konkretnym radiu

Najpierw świeży `check`, potem [pełna instalacja](INSTALACJA.md) i [cofnięcie](COFNIECIE.md). Nie wykonuj poniższej komendy bez własnego backupu i potwierdzonej drogi odzyskania także bez ADB po starcie:

```sh
python tools/ngd_patch.py --serial SERIAL --profile .local/twin-kit/profile.json --release .local/twin-kit check
# Dopiero po zgodzie na konkretną operację i potwierdzeniu odzyskania:
python tools/ngd_patch.py --serial SERIAL --profile .local/twin-kit/profile.json --release .local/twin-kit install --accept-system-write --accept-whole-blink-pause --recovery-confirmed --accept-unverified-twin
```

Dodatkowe potwierdzenie kandydata nie omija kontroli. Pozostałe zasady nie zmieniły się: pełna bramka HCI, sprawdzony zapis/usunięcie init, brak automatycznego rebootu, zachowanie oryginałów, brak instalacji /data/app, brak flashowania i modyfikacji UID.

Sprawdź ON, skan Classic/BLE, parowanie, rzeczywisty odsłuch SBC, HID równocześnie z audio, ręczne OFF/ON, ponowne połączenie, wznowienie oraz zimny start i dłuższą stabilność. Dopiero wyniki danej jednostki są jej dowodami sprzętowymi. Obecny generator emituje profile kandydatów; promocja do wydania sprzętowo zweryfikowanego jest osobnym przeglądem, nie automatyczną zmianą flagi.

## English

`ngd_twins.py assess` reads the selected OEM unit and collects differences instead of rejecting only its fingerprint. The first supported candidate family is trinket/Cherokee, Android10 ARM64 with the reviewed integration components and installation layout; it does not require a bit-identical full firmware image. Different critical binaries/layout need the repository compatibility skill and an engineered port, not force installation.

`prepare` generates per-target native sources and a candidate profile without inherited hardware success. Rebuild both the supervisor and init installer; keep the verified APK/JNI. `seal` verifies the base kit, source inventory and target-specific ARM64 markers and emits a separately hashed release/profile pair. `ngd_patch.py --profile ... --release ...` accepts that pair for check/install/remove; installing an untested twin additionally requires `--accept-unverified-twin`, alongside existing explicit approvals and recovery.

No hardware support is claimed from offline results. Follow the Polish commands above, the compatibility skill, and the existing installation/rollback guides. No device access, installation or publication is performed by prepare/seal. Never redistribute private logs, pairing data or OEM runtime binaries without appropriate rights.

## Skill Codexa po odrzuceniu jednostki / Codex skill after rejection

Otwórz repo jako projekt i wywołaj:

```text
$ngd-headunit-compatibility
Instalator odrzucił moją jednostkę. Oceń sprzęt i interfejsy, przygotuj
najmniejszy port, test i rollback. Nie obchodź zabezpieczeń.
```

[Skill i jego materiały](../.agents/skills/ngd-headunit-compatibility/SKILL.md) opisują cały tor Android → JNI/QTI → HAL/HCI oraz audio, MCU, budowę i odzyskanie. Jeżeli klient nie wykrywa skilli projektowych, skopiuj cały katalog `.agents/skills/ngd-headunit-compatibility` do `~/.codex/skills/ngd-headunit-compatibility` i rozpocznij nową sesję, albo wskaż SKILL.md do przeczytania. Skill nie nadaje uprawnień i nie instaluje automatycznie.

English: open this repository in Codex and invoke `$ngd-headunit-compatibility`. If project discovery is unavailable, copy the whole skill folder into your Codex user skills directory and start a new session, or explicitly ask Codex to read SKILL.md. The skill supplies the evidence/porting workflow, not additional privileges or an installer bypass.
