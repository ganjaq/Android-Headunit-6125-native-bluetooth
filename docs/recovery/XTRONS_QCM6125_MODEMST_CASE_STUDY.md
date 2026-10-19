# Przypadek do analizy odzyskiwania: XTRONS IQ92MTVL / QCM6125 po OTA

Data zapisania: 2026-10-18. Status: **relacja zewnętrzna dostarczona przez użytkownika, nie próba na naszym radiu**. Materiał służy przyszłej diagnostyce i planowaniu kanału odzyskania. Nie jest instrukcją automatycznego uruchamiania poleceń ani dowodem, że nasz BLINK_6125 da się tak odzyskać. Nie wykonano ADB/EDL/fastboot/UART, kasowania lub flashowania; nie pobrano cudzej kopii partycji.

## 1. Jednostka opisana w relacji

| Pole | Dane przekazane w opisie |
|---|---|
| Model | XTRONS IQ92MTVL |
| Platforma | Qualcomm QCM6125; nazwy trinket / nicobar w relacji |
| Moduł | Fibocom SC138-EAU |
| RAM / flash | 6 GB / 128 GB |
| Android / rodzina | Android 12, QCM6125 / S10A_123 |
| Poprzedni build | eng.hct2.20221115.160243 EAU |
| Późniejsze firmware | hct3 Android 12 / SDK31 |
| MCU | MTCH_GS_V3.95_3, data 2022-11-02 09:33:20 |
| Objaw | Logo producenta → logo Androida → czarny ekran / restart po nieudanym OTA |

Tłumaczenie „drobiazg” odpowiada nazwie trinket; nie jest osobnym oznaczeniem układu. To nie ta sama wersja Androida, MCU ani potwierdzony moduł co nasza referencyjna jednostka Android10. Wspólny numer 6125 nie przenosi automatycznie procedury ani backupu.

## 2. Wykorzystane kanały i narzędzia

| Kanał | Narzędzie / element | Zastosowanie według autora | Co pozostaje nieustalone |
|---|---|---|---|
| OEM Recovery / aktualizacja | Pakiety firmware oficjalne i niestandardowe | Próby instalacji, komunikaty BSP i kInstallDeviceOpenError | Dokładna metoda wejścia, wersja recovery, nośnik i kompletne logi |
| UART na PCB | Adapter FTDI zgodny z logiką **1,8 V**, PuTTY | Odczyt boot logu z linii TX jednostki | Baud rate, pozostałe parametry terminala, mapowanie pinów naszej płyty i faktyczny poziom elektryczny |
| Qualcomm EDL przez USB | QFIL z pakietu QPST, Partition Manager | Zalecany backup partycji przed ingerencją | Sposób wejścia EDL na danej płycie, zgodny programmer Firehose, obsługa pamięci i ewentualna autoryzacja |
| Bootloader fastboot przez USB | fastboot | Skasowanie dokładnie modemst1/modemst2 i restart w opisanym egzemplarzu | Droga EDL → fastboot, stan blokady bootloadera i dostępność erase na naszej jednostce |
| EDL / automatyzacja backupu | QFILHelper, autor Beliathal | Backup partycji po odzyskaniu, automatyzacja QFIL | Wersja narzędzia i pełny inwentarz faktycznie odczytanych LUN/partycji |
| EDL / alternatywa | bkerler/edl | Wymieniony jako opcja dla zaawansowanych | Nie potwierdzono, że autor użył go do tej konkretnej naprawy |
| ADB | Nie podano użycia | Brak dowodu użycia ADB w tej naprawie | Nie zakładać, że ADB działa w bootloopie lub EDL |

Odczyt UART jest innym kanałem niż powłoka ADB. Sama obserwacja TX nie dowodzi możliwości sterowania bootloaderem. Do planu potrzebna jest zweryfikowana masa i zgodny poziom logiczny; nie traktować opisu adaptera jako dowodu tolerancji wszystkich pinów ani zasilać płyty z adaptera na podstawie tego wpisu. Nie podano komend terminala ani parametrów transmisji — nie zgadywać ich.

Dokumentacja [QFILHelper](https://github.com/Beliathal/QFILHelper/blob/main/README.md) potwierdza automatyzację odczytu partycji/LUN z QFIL i pracę z otwartym Partition Manager. Narzędzie ma też funkcje zapisu/kasowania: sam wybór programu nie czyni operacji odczytową. Zakres kopii, zwłaszcza userdata, zależy od wersji i opcji; nie nazywać kopii „pełną” bez sprawdzenia inwentarza. [bkerler/edl](https://github.com/bkerler/edl) jest narzędziem Sahara/Firehose, nie uniwersalnym programmerem pasującym do każdego QCM6125.

## 3. Tor diagnostyczny autora

1. Ponowne OTA nie usuwało bootloopa. Część paczek była odrzucana jako niezgodny BSP, część pozornie instalowała się pomyślnie.
2. Autor opisuje układ A/B i nieskuteczność samej zmiany slotu. Nie mamy jego pełnych odczytów stanu obu slotów. Ogólny mechanizm instalowania aktualizacji do nieaktywnego slotu opisuje [dokumentacja AOSP](https://source.android.com/docs/core/ota/ab); nie dowodzi to identycznego układu wszystkich partycji każdej jednostki.
3. Odczyt UART wskazał rmt_storage i dostęp do pamięci modemu. W przytoczonej relacji występowały: `/boot/modem_fs2`, `sdf2 I/O error`, CRC/ECC error, modem crash i kernel panic.
4. Według autora po usunięciu dwóch partycji runtime modemu radio zaczęło uruchamiać się poprawnie.

Nie mamy niezależnej weryfikacji całego boot logu ani testów po długim czasie. CRC/ECC/I/O samo nie dowodzi wyłącznie uszkodzonej zawartości: wymaga rozróżnienia błędu pamięci/transportu od błędu danych. Nie zakładać bez mapy GPT i logów, że ścieżka `modem_fs2` albo numer `sdf2` zawsze odpowiada tej samej partycji. To hipoteza diagnostyczna dla podobnych objawów, nie uniwersalna naprawa bootloopa.

## 4. Zakres zgłoszonej operacji — zapis historyczny, NIE DO WYKONANIA

Autor podał następujące **destrukcyjne** polecenia, wykonane na opisanym XTRONS:

```text
fastboot erase modemst1
fastboot erase modemst2
fastboot reboot
```

Nie są częścią instalatora Bluetooth, skilla automatycznej naprawy ani naszego rollbacku. Nie uruchamiać ich na podstawie podobnego procesora czy samego bootloopa. Nie przyjmować, że każdy modem bezpiecznie odtworzy skasowane dane NV lub zachowa wszystkie funkcje/identyfikatory. Najpierw potrzebna jest własna zweryfikowana kopia oraz realna możliwość przywrócenia dokładnie tych danych. Nie wykonywać unlock/reset/flash jako ukrytego etapu przygotowania.

Relacja wyraźnie wykluczała kasowanie **fsg, fsc, persist** i przypadkowych partycji. Nie rozszerzać zakresu kasowania.

## 5. Backup wskazany w materiale

Przed ingerencją autor zalecił backup: `modemst1`, `modemst2`, `fsg`, `fsc`, `persist`, `super`, `metadata`, `misc`. W przekazanym tłumaczeniu „Trwa”, „Metadane” i „Różne” należy rozumieć jako odwołanie do tych nazw, ale rzeczywiste nazwy/numery/LUN/rozmiary muszą wynikać z GPT konkretnej jednostki.

Dla analizy zachować również identyfikację hardware/rewizji PCB i modułu, mapę partycji, właściwy loader i jego pochodzenie/hash, sloty oraz konfigurację narzędzi. Sprawdzić rozmiary i SHA-256 odczytów oraz możliwość powtórnego odczytu. Kopie danych modemu, persist i userdata mogą zawierać dane unikalne lub prywatne; nie dodawać ich do publicznego Git.

Cudza kopia XTRONS nie jest naszą kopią bezpieczeństwa. Nie przenosić cudzych modemst/persist/kalibracji/identyfikatorów między jednostkami. Link poniżej jest tylko adresem z relacji: nie pobrano, nie sprawdzono zawartości, kompletności ani zgodności obrazów.

## 6. Znaczenie dla NEXTGENDRIVE

- Obecny selektywny backup 137 plików OEM nie jest obrazem partycji i nie zastępuje odczytu przez EDL.
- Ten materiał nie wystarcza do zaznaczenia `--recovery-confirmed` w naszym instalatorze.
- Przed uznaniem kanału odzyskania trzeba sprawdzić na właściwej rewizji sprzętu: punkty i parametry UART, wejście do EDL/fastboot, wykrywanie USB, zgodny Firehose, odczyt GPT/partycji i realny sposób przywrócenia bez działającego Androida. Sam port 9008 lub znaleziony program nie dowodzi dostępu do pamięci.
- Dla awarii po naszej łatce zacząć od własnych logów i zmian init/bind mount/nadzorcy. Nie przypisywać jej modemst bez pasujących dowodów.
- Każda operacja zapisu/kasowania, zmiana sterownika Windows, rozbieranie płyty lub restart zakłócający działanie wymaga osobnego konkretnego planu i autoryzacji w zakresie danej jednostki.

## 7. Odnośniki z relacji użytkownika

Nie otrzymano URL całego wpisu forum; oryginalnego autora i pełnego wątku nie ustalono. Użytkownik dostarczył tekst oraz poniższe linki. Bezpośrednich zdjęć i archiwum nie kopiowano do projektu.

- [Recovery — błędny BSP](https://xdaforums.com/attachments/recovery-screen-incorrect-bsp-jpg.6340388/)
- [kInstallDeviceOpenError](https://xdaforums.com/attachments/kinstalldeviceopenerror-jpg.6340390/)
- [Recovery, próby 1 i 2](https://xdaforums.com/attachments/recovery-try-1-and-2-jpg.6340389/)
- [Punkt UART](https://xdaforums.com/attachments/uart-test-point-jpg.6340391/) — dotyczy pokazanej płyty, nie automatycznie naszej.
- [Adapter wskazany przez autora](https://www.amazon.co.uk/dp/B07WX2DSVB) — nie zweryfikowano parametrów; nie jest rekomendacją zakupu.
- [QFILHelper — repozytorium autora](https://github.com/Beliathal/QFILHelper)
- [bkerler/edl — repozytorium autora](https://github.com/bkerler/edl)
- [Cudza kopia opisanego XTRONS, link niezweryfikowany](https://drive.google.com/file/d/1tjK6CfwQYuX9A5JgcgvoOF7l9bb3fo-P/view?usp=drive_link)
- [PCB / Fibocom](https://xdaforums.com/attachments/main-pcb-fibocom-jpg.6340392/)
- [PCB i obudowa](https://xdaforums.com/attachments/main-pcb-and-enclosure-jpg.6340394/)
- [KJD_H7019ZB_B](https://xdaforums.com/attachments/kjd_h7019zb_b-20229-14-jpg.6340393/)
- [TFT](https://xdaforums.com/attachments/tft-panel-jpg.6340395/)
- [Dotyk, tył](https://xdaforums.com/attachments/touch-panel-back-jpg.6340396/) / [przód](https://xdaforums.com/attachments/touch-panel-front-jpg.6340397/)
- [STM32G070 MCU](https://xdaforums.com/attachments/stm32g070-mcu-jpg.6340398/)
- [HCTS-4G-V1.3](https://xdaforums.com/attachments/hcts-4g-v1-3-20210429-jpg.6340399/)

Sprawdzenie źródeł narzędzi: otwarto dokumentację QFILHelper i AOSP A/B. Repo bkerler/edl znaleziono w wynikach wyszukiwania; bezpośrednie pobranie strony i strony załącznika UART przez przeglądarkę narzędzi zakończyło się błędem. Nie przedstawiać zdjęć, zawartości Google Drive ani całej naprawy jako niezależnie zweryfikowanych.