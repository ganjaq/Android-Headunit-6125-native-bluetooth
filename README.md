# Android-Headunit-6125-native-bluetooth

Projekt rozwijany metodą vibe coding - Natywny Bluetooth Androida 10 dla radia qti/trinket/ QKQ1.200816.002 / eng.zouqiao.20250217.185927/ Qualcomm 6125 / BLINK_6125 — projekt NEXTGENDRIVE.

[English](README.en.md) · [Instrukcja obsługi](docs/OBSLUGA.md) · [Instalacja](docs/INSTALACJA.md) · [Cofnięcie](docs/COFNIECIE.md) · [Źródła i budowa](docs/BUILD.md)

Łatka przywraca częściowo systemowy Bluetooth Androida, A2DP Source/SBC do fabrycznego systemu auta i dodaje obsługę HID-Host dla padów Xbox One bt. Korzysta z istniejącego Qualcomm HAL oraz libbluetooth_qti.so. Nie wgrywa drugiego stosu sprzętowego ani firmware Bluetooth.

## Co potwierdzono

Na jednostce QKQ1.200816.002 - BLINK_6125 udało się odtworzyć dźwięk z android head unit na systemie audio auta przez A2DP wprost z urządzenia z pominięciem przewodów. Potwierdzono równoczesne działanie pada Xbox One bt, ręczne OFF/ON Bluetooth oraz automatyczne łączenie się jednostki bt auta i headunit zaraz po ponownym odpaleniu samochodu. W najnowszym teście po wyłączeniu i włączeniu auta pad łączy się błyskawicznie, Audi automatycznie po około 30 s, dźwięk działa. Łatka jest wciąż w fazie testów ale wszystko wskazuje na to, że w końcu odzyskaliśmy androidowy stos bluetooth do własnej dyspozycji (np. obdeleven itp).

**Nowy instalator dystrybucyjny jest wydaniem 0.1.0-rc1, sprawdzonym offline.** Działające komponenty i poszczególne operacje były testowane na radiu; całej nowej procedury od czystego OEM do instalacji i powrotu nie powtórzono na drugiej jednostce. Wielogodzinna stabilność i wszystkie funkcje MCU nie są potwierdzone.

## Zgodność

Nie wystarczy napis „Qualcomm 6125”, Android 10 ani wygląd obudowy. Domyślna paczka używa sprawdzonego profilu `blink6125-qti-20250217-arm64-a`. Dla bliźniaczych jednostek z innym fingerprintem dodano [ocenę i przygotowanie osobnego wariantu](docs/BLIZNIACZE_JEDNOSTKI.md); nie wymagamy identycznego całego firmware. Bazowy fingerprint:

```text
qti/trinket/trinket:10/QKQ1.200816.002/eng.zouqiao.20250217.185927:userdebug/test-keys
```

Wymagane są identyczne SHA-256 oryginalnych BluetoothExt.apk, blink, libbluetooth_qti.so i Settings.apk oraz HAL/audio, ARM64/API29, shared UID1002, ADB root, SELinux Permissive, slot _a i sprawdzona konfiguracja ext4/dm-3. Instalator odmawia przy różnicach. Nie ma opcji „force”. [Profil zgodności](profiles/blink6125-qti-20250217-arm64-a.json).

## Jak działa

Dane i własne pliki trafiają do `/data/local/ngd-native-bt`. Jedyny dodawany plik systemowy to `/system/etc/init/ngd-native-bt.rc`. Przy rozruchu init odtwarza bind mounty w RAM nad BluetoothExt, Settings i plikiem wykonywalnym BLINK. Oryginały OEM pozostają na partycjach. Własny JNI dostaje dostęp do Qualcomm dopiero po kontroli HCI, gotowości MCU/audio i autoryzacji nadzorcy. Domyślna bramka jest zamknięta; nie ma limitu normalnej sesji i zależności od laptopa.

**Zatrzymywany jest cały BLINK, nie tylko jego część Bluetooth.** Jego CarPlay/Android Auto i inne funkcje mogą przestać działać. mcuserver nie jest osobno zatrzymywany; to nie dowód bezpieczeństwa wszystkich funkcji konkretnego radia. Błąd nadzorcy zamyka natywny BT, bez automatycznego uruchamiania OEM.

## Szybki start

Python 3.11+, oficjalny ADB w PATH, jedno radio przez USB. Najpierw `adb devices` i, jeżeli urządzenie wspiera tę funkcję, `adb root`. Nie uruchamiaj instalatora na już zmodyfikowanym radiu.

```sh
python tools/ngd_patch.py plan
python tools/ngd_patch.py verify-release
python tools/ngd_patch.py check
```

`plan` i `verify-release` nie kontaktują się z radiem; `check` tylko odczytuje. Następnie wykonaj [pełną instrukcję](docs/INSTALACJA.md). Instalator nie wykonuje rebootu automatycznie. Instalacja /data/app, pm uninstall, flash, naprawianie UID i wyłączanie SELinux nie są używane.

## Repo a paczka instalacyjna

Repo zawiera źródła, profil, łatki, narzędzia i dokumentację. `release/` oraz `dist/` są wyłączone z Git: lokalna paczka zawiera także zmodyfikowane OEM Settings oraz biblioteki pobrane z radia. Nie przedstawiamy tych binariów OEM jako własnego kodu Apache i nie publikujemy ich automatycznie. Archiwum źródłowe Git nie zastępuje pakietu z manifestem. [Zawartość i pochodzenie](docs/PROVENANCE.md).

Żadne dane parowania, konta, oryginalne 137 plików backupu ani surowe logi użytkownika nie trafiają do repo ani dystrybucji. Backup powstaje osobno na każdym urządzeniu i komputerze.

Kod AOSP zachowuje swoje nagłówki i Apache-2.0; pozostałe pliki źródłowe projektu udostępniono na Apache-2.0. Projekt nie jest produktem Audi, Qualcomm ani Microsoft.


## Inne jednostki i skill dla Codexa

Odrzucenie przez aktualny profil nie dowodzi niezgodności sprzętu. [Procedura oceny innych jednostek](docs/BLIZNIACZE_JEDNOSTKI.md) i [skill ngd-headunit-compatibility](.agents/skills/ngd-headunit-compatibility/SKILL.md) prowadzą przez sprzęt, ABI/JNI, HCI, audio, nowy profil, test i rollback. Bieżący instalator nie został odblokowany dla niezweryfikowanych konfiguracji.


## Internet z telefonu — hotspot po podłączeniu ładowarki

Dodano [aplikację NGD Hotspot z APK, źródłami i instrukcją](apps/hotspot-on-charge/README.md), przetestowaną na P30 Pro / Android10. Instaluj ją na telefonie, nie na radiu. [Przed instalacją Bluetooth przygotuj USB Device/ADB na head unit](docs/INSTALACJA.md#przed-połączeniem-przez-usb).


## Zdjęcia z testów

[Galeria: radio, płyta główna i pad Xbox](docs/GALERIA.md).
