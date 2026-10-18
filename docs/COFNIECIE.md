# Cofnięcie — PL

Pełny powrót OEM jest osobną, świadomą operacją. Codzienny OFF kafelkiem pozostawia nadzorcę gotowego do następnego ON i nie wymaga odinstalowania.

Dla jednostki zainstalowanej NOWYM instalatorem tej paczki, po uzyskaniu ADB root i odczycie logów:

```sh
python tools/ngd_patch.py remove --accept-oem-return
```

Polecenie weryfikuje profil, własny init/nadzorcę i baseline, żąda kontrolowanego zamknięcia natywnego BT oraz wymaga braku procesu i niedokończonego dziennika. Usuwa wyłącznie własny marker i dokładnie zgodny plik init poprzez pomocnika z przywróceniem RO, przywraca zapisane właściwości audio, ustawienia, stan włączenia pakietu i sprawdza uprawnienia przez istniejący pomocnik. Zachowuje wszystkie kopie i pliki w /data. Nie usuwa APK, nie zmienia UID ani właścicieli danych i nie uruchamia BLINK w działającym widoku z maską.

Następnie dopiero po świadomej decyzji:

```sh
adb reboot
```

Bind mounty znikają; oryginalne pliki są nadal na partycjach, init nie blokuje BLINK. Sprawdź panel/MCU, fabryczny Bluetooth i audio. Nie deklarujemy, że cały ten nowy przepływ powrotu był przetestowany na drugiej jednostce. Kontrola instalacji/usunięcia dokładnego pliku init oraz cofnięcie własnego APK/nadzorcy były testowane osobno na radiu.

Jeżeli nadzorca nie potwierdzi zamknięcia, radio nie bootuje albo brak ADB, skrypt nie naprawi tego automatycznie. Użyj wcześniej przygotowanej, rzeczywiście znanej procedury odzyskania konkretnego modelu. Nie flashuj cudzych obrazów i nie kasuj packages.xml. Sam backup nie zapewnia kanału odzyskiwania.

Na radiu autora zainstalowanym wcześniejszym procesem nie ma `distribution-baseline.json`; ten ogólny remover celowo odmówi. Należy użyć oryginalnego baseline/procedury tamtej instalacji. Skrypt przełączenia HID→wersja audio z prac rozwojowych także nie jest pełnym powrotem do OEM.
