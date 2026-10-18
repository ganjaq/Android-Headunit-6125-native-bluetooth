# Instrukcja obsługi — natywny Bluetooth NEXTGENDRIVE

[English](USER_GUIDE.en.md) · [Instalacja](INSTALACJA.md) · [Powrót do OEM](COFNIECIE.md)

Ta instrukcja dotyczy radia, na którym zgodna łatka została już poprawnie wdrożona. Samo pobranie repozytorium nie instaluje Bluetooth. Nowa instalacja wymaga sprawdzenia zgodności i wykonania osobnej instrukcji instalacji.

## 1. Codzienne uruchamianie

1. Włącz auto/radio oraz fabryczny system Audi.
2. Poczekaj na uruchomienie Androida i automatyczne włączenie Bluetooth.
3. W Audi wybierz źródło dźwięku Bluetooth Audio. Uruchom muzykę w aplikacji Androida, np. YouTube lub odtwarzaczu.

Po zapisaniu parowania nie trzeba za każdym razem wyszukiwać urządzeń ani podłączać laptopa. W potwierdzonym teście po cyklu wyłączenia/włączenia auta Audi łączyło się automatycznie po około 30 sekundach, a pad Xbox niemal natychmiast. Czas zależy od gotowości obu urządzeń. Nie jest to limit sesji: w normalnej pracy nie ma odliczania 60/120/300 sekund.

Nie potwierdzono osobno wszystkich wariantów głębokiego uśpienia i pełnego zaniku zasilania. Po aktualizacji firmware zgodność łatki trzeba sprawdzić ponownie.

## 2. Gdzie znaleźć Bluetooth

- **Kafelek Bluetooth:** rozwiń szybkie ustawienia u góry ekranu; kafelek znajduje się obok Wi-Fi i innych przełączników.
- **Menu urządzeń:** Ustawienia → Connected devices / Połączone urządzenia → Connection preferences / Preferencje połączeń → Bluetooth.
- **Dodanie urządzenia:** wybierz Pair new device / Sparuj nowe urządzenie w menu połączonych urządzeń. Układ i tłumaczenie nazw zależą od OEM Settings.

Jeśli nie widzisz pozycji w menu, wyszukaj „Bluetooth” w ustawieniach. Brak kafelka lub pozycji po nowej instalacji zgłoś wraz z wersją firmware; nie ponawiaj instalacji w ciemno. Ekran parowania był wcześniej dostępny przez wyszukiwarkę nawet wtedy, gdy fabryczny wpis menu był ukryty.

## 3. Pierwsze parowanie z Audi

1. Włącz Bluetooth na radiu i otwórz ekran dodawania nowego urządzenia.
2. W Audi włącz wyszukiwanie/parowanie Bluetooth. Wybierz radio na liście Audi albo Audi na liście radia.
3. Porównaj kod widoczny na obu ekranach. Zatwierdź wyłącznie zgodne kody. Jeżeli kod się nie pojawia lub jest inny, przerwij tę próbę i rozpocznij ją ponownie.
4. Po połączeniu wybierz w Audi Bluetooth Audio i odtwórz muzykę na Androidzie.

W jednostce testowej radio występowało początkowo jako **QCOM-BTD**, później jako **Audi android**. Twoja jednostka może mieć inną nazwę; sprawdź ją na ekranie Bluetooth. Sama obecność na liście zapisanych urządzeń oznacza zapamiętane parowanie, a nie aktualne połączenie audio.

Jeśli zapisane Audi nie połączy się automatycznie, wybierz je w ustawieniach Bluetooth radia i naciśnij Połącz. Możesz też wybrać zapisane radio w Audi. Nie trzeba od razu usuwać parowania. Jeżeli powtarza się problem po każdym starcie, zachowaj diagnostykę z `status` i zgłoś go.

## 4. FM i powrót do muzyki Androida

Możesz słuchać fabrycznego FM przy włączonym Bluetooth radia. Gdy zechcesz słuchać Androida, wybierz w Audi źródło Bluetooth Audio i uruchom/wznów odtwarzanie na Androidzie. Powrót z FM został sprawdzony z niewielkim opóźnieniem.

Włączone Bluetooth pozwala na połączenie z wcześniej sparowanym urządzeniem. Nie oznacza to ciągłej widoczności radia dla nowych urządzeń. Do nowego parowania otwórz odpowiedni ekran w ustawieniach; nie zakładamy nieograniczonej wykrywalności w tle.

## 5. Ręczne OFF i ON

1. Dotknij kafelka Bluetooth, aby wyłączyć BT. Audi i pad zostaną rozłączone.
2. Poczekaj, aż przełącznik pokaże OFF/Wyłączone.
3. Włącz Bluetooth tym samym kafelkiem i poczekaj na ponowne połączenie.

Ręczne OFF/ON i ponowna muzyka w Audi były sprawdzone na jednostce testowej. Wyłączenie kafelkiem nie usuwa parowań i nie odinstalowuje łatki. Nie sprawdzono jeszcze, czy świadomie ustawione OFF będzie zachowane przy każdym typie ponownego uruchomienia — obecny autostart służy automatycznemu włączeniu BT po uruchomieniu radia.

## 6. Pad Xbox

1. Na czas pierwszego testu najpierw połącz Audi i sprawdź muzykę.
2. Włącz pad i uruchom na nim tryb parowania przyciskiem parowania; logo Xbox miga.
3. Na radiu otwórz Pair new device / Sparuj nowe urządzenie i wybierz Xbox Wireless Controller.
4. Poczekaj na zakończenie łączenia. Sprawdź krzyżak/przyciski w menu lub w aplikacji obsługującej kontroler.

Po zapisaniu parowania testowany pad łączył się automatycznie po ponownym włączeniu. Jednoczesne działanie pada i muzyki w Audi zostało potwierdzone; nie ma tu potwierdzonego ograniczenia do jednego urządzenia. Przy dołączaniu pada wystąpiła krótka przerwa dźwięku. Nie wszystkie aplikacje obsługują pada, a działanie każdego przycisku, wibracji i innych modeli kontrolerów nie zostało osobno zweryfikowane.

Jeśli logo nadal miga, otwórz szczegóły zapisanego pada i wybierz Połącz. Samo „Sparowano” nie dowodzi aktywnego HID. Jeżeli pad jest połączony z konsolą lub innym urządzeniem, rozłącz go tam przed ponowną próbą z radiem.

## 7. Gdy dźwięku nie ma

1. Sprawdź, czy Audi pokazuje aktywne połączenie z radiem, a nie tylko wpis na liście parowań.
2. Wybierz w Audi źródło Bluetooth Audio i sprawdź głośność/wyciszenie.
3. Upewnij się, że odtwarzasz dźwięk na Androidzie. Wznów odtwarzanie po zakończeniu łączenia.
4. Jeśli nadal jest cisza albo Bluetooth sam przechodzi do OFF, zbierz stan opisany poniżej. Nie zmieniaj samodzielnie właściwości offload i nie restartuj usług audio w ciemno.

Głośność, potwierdzone połączenie i ruch paska odtwarzania nie zastępują odsłuchu. Podczas prac połączenie A2DP było możliwe również przy niedziałającym torze dźwięku.

## 8. Diagnostyka i zgłoszenie

Na laptopie z działającym, autoryzowanym ADB można tylko odczytać stan:

```sh
python tools/ngd_patch.py status
```

Podaj wersję wydania, firmware, kolejność włączania Audi/pada, przybliżony czas problemu, czy BT pozostało ON, czy ekran reagował i czy dźwięk wrócił. `status` nie uruchamia BT, nie resetuje radia i nie przywraca BLINK. Do samego codziennego używania ADB nie jest potrzebne.

Przed publicznym zgłoszeniem usuń z odczytów nazwy/adresy urządzeń i inne dane osobowe. Nie publikuj `.local/backups`, plików snapshot/receipt, kluczy parowania ani surowego logcat/btsnoop.

Wersja przywraca przede wszystkim A2DP Source/SBC, GATT i HID Host. Nie traktuj jej jako potwierdzenia rozmów HFP, działania mikrofonu, książki telefonicznej czy wszystkich funkcji CarPlay/Android Auto. Cały fabryczny BLINK jest zatrzymany — jego funkcje są objęte zmianą. Powrót do OEM to osobna procedura z [instrukcji cofnięcia](COFNIECIE.md), a nie normalny przycisk OFF.


## USB / ADB

Opcje programistyczne, debugowanie USB i rola portu **Device/ADB zamiast Host** są potrzebne przy połączeniu serwisowym z komputerem; zobacz [przygotowanie USB](INSTALACJA.md#przed-połączeniem-przez-usb). Codzienne Bluetooth i aplikacja hotspotu nie wymagają podłączonego laptopa.
