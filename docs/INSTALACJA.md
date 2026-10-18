# Instalacja — PL

## Przed połączeniem przez USB

Na **head unit** włącz opcje programistyczne (zwykle kilka dotknięć numeru kompilacji w informacji o urządzeniu) oraz **debugowanie USB**. W ustawieniach roli portu USB wybierz **Device / urządzenie / ADB**, nie **Host**. Nazwy i lokalizacja przełącznika zależą od OEM. To tryb portu radia, nie ustawienie telefonu. Potwierdź autoryzację RSA dla zaufanego komputera, jeśli pojawi się monit.

`adb devices` powinno pokazać radio jako `device`, nie `unauthorized`/`offline`. Debugowanie USB samo nie daje roota: osobno sprawdź, czy firmware wspiera `adb root` i czy `adb shell id` zwraca UID0. Nie zakładaj tego dla każdego 6125. `adb root` restartuje adbd i może wymagać ponownego wykrycia urządzenia.

Jeśli Windows zgłasza „żądanie deskryptora urządzenia nie powiodło się”, urządzenie nie zostało jeszcze poprawnie rozpoznane na poziomie USB; nie jest to błąd profilu Bluetooth ani powód do uruchamiania instalacji. Nie przełączaj roli USB podczas zapisu. Laptop/ADB jest potrzebny do instalacji i diagnostyki, nie do codziennego autostartu Bluetooth.


1. Przeczytaj README i wpływ zatrzymania całego BLINK. Zapewnij stabilne zasilanie i zaparkowane auto. Ta paczka jest dla identycznego firmware i hashów, nie wszystkich jednostek 6125.
2. Przygotuj znaną procedurę odzyskania TEJ jednostki od sprzedawcy/producenta i sprawny ADB root. `--recovery-confirmed` oznacza Twoje potwierdzenie; skrypt nie potrafi dowieść działania recovery ani odzyskania po utracie bootowania. Nie sprawdza go restartem do recovery. Sam selektywny backup nie jest pełnym obrazem systemu.
3. Rozpakuj kompletny pakiet (ma `release/manifest.json`, `release/payload/`, `tools/`, `profiles/`). Python 3.11+ oraz ADB mają być w PATH. Przy wielu urządzeniach podaj `--serial SERIAL` przed nazwą akcji. Przy alternatywnym ADB użyj `--adb /ścieżka/adb`.
4. Uruchom odczyty i sprawdzenie plików:

```sh
adb devices
adb root
adb shell id
python tools/ngd_patch.py verify-release
python tools/ngd_patch.py check
python tools/ngd_patch.py plan
```

`check` jest dla CZYSTEGO OEM. Odmowa na już zmodyfikowanym radiu jest prawidłowa. Nie kasuj markerów ani baseline, by ją obejść. Inna rewizja firmware wymaga osobnego audytu ABI/HCI i profilu, a nie dopisania samego fingerprintu.

5. Jeżeli wynik jest zgodny i akceptujesz zakres:

```sh
python tools/ngd_patch.py install --accept-system-write --accept-whole-blink-pause --recovery-confirmed
```

Najpierw powstaje lokalny backup wszystkich przypiętych oryginałów, konfiguracji BT z metadanymi i baseline uprawnień oraz ustawień. Nie nadpisuje istniejącego katalogu instalacji. Domyślny katalog backupów `.local/backups/` jest wyłączony z Git; można wskazać `--backups /bezpieczny/katalog` przed `install`. Backup zawiera prywatne klucze parowania — zachowaj go dla siebie.

Narzędzie etapuje nieaktywny payload i sprawdza SHA-256 po odczycie. Kompilowany pomocnik tylko na tej konfiguracji przełącza dm-3/root RW, zmniejsza rezerwę ext4 w RAM 4096→1024, tworzy dokładnie nowy plik init, sprawdza i usuwa go, przywracając rezerwę i obie blokady RO. Dopiero po PASS zapisuje docelowy init, marker autostartu oraz właściwości audio false/true. Jeżeli lista kafelków jest jawna, dodaje `bt`, zachowując pozostałe.

6. Instalator NIE restartuje radia. Sprawdź komunikat końcowy, lokalizację backupu i brak STOP. Dopiero przy gotowości na przerwę panelu/aplikacji/audio wykonaj:

```sh
adb reboot
```

Po starcie init zatrzyma cały BLINK i udostępni natywny BT; bramka HCI pozostaje zamknięta do pełnej kontroli. Nie uruchamiaj starych skryptów sesji lub instalacji /data/app.

7. Gdy panel działa i ADB wróci:

```sh
python tools/ngd_patch.py status
```

Wymagaj ON, działającego MCU, stopped BLINK i braku błędów nadzorcy. W ustawieniach Connected devices → Bluetooth/Pair new device sparuj Audi, porównując rzeczywiste kody. Wybierz Bluetooth Audio w Audi i sprawdź odsłuch. Następnie sparuj Xbox. Potwierdź oba jednocześnie i dopiero potem cykl wyłączenia/włączenia auta, FM→BT oraz kafelek OFF/ON.

STOP: utrata ADB, reakcje MCU inne niż normalny rozruch, obcy właściciel HCI, niezgodne hashe/UID lub błąd odzyskiwania RO. Nie wymuszaj QTI, nie restartuj HAL w ciemno i nie edytuj packages.xml. Zachowaj backup i komunikat STOP; nie powtarzaj instalacji na częściowo przygotowanym stanie.

Jeśli check zgłosi nierozwiązaną domyślną listę kafelków (null), otwórz edytor szybkich ustawień na radiu i zapisz bieżący układ. Ponów check. Instalator nie zgaduje ani nie zastępuje listy OEM.
