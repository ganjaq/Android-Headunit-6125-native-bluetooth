---
name: ngd-headunit-compatibility
description: "Assess an Android head unit rejected by the NEXTGENDRIVE native Bluetooth installer. Use when fingerprint, OEM hashes, partition layout or hardware differs from the reference Qualcomm 6125 unit, to determine hardware/interface compatibility and prepare a reviewed port with rollback rather than bypassing checks."
---

# NEXTGENDRIVE: odrzucona jednostka / rejected head unit

Pracuj w istniejącym projekcie. Odrzucenie przez instalator oznacza brak obsługi w bieżącym profilu, nie dowód niezgodności sprzętu. Celem jest natywny Bluetooth Androida z A2DP Source/SBC, systemowym menu i autostartem; zachowaj działający HID/GATT. Nie twórz drugiego właściciela HCI ani nowej niezależnej aplikacji zamiast integracji systemowej.

Skill nie upoważnia do instalacji, przejęcia HCI, restartów ani zmian partycji. Wykonuj samodzielnie odczyty i pracę offline w zakresie zlecenia. Przed zmianami na urządzeniu przedstaw konkretne komendy, skutki, kopie i przywracanie; uzyskaj zgodę na ten zakres. Wcześniejsze zgody dla innej jednostki nie przechodzą na tę jednostkę.

Najpierw sprawdź `docs/BLIZNIACZE_JEDNOSTKI.md`: ścieżka `ngd_twins.py assess → prepare → seal` potrafi przygotować wariant innego fingerprintu przy zgodnych komponentach integracji. Nie opisuj obecnego repo jako zawsze ograniczonego do jednego fingerprintu. Rozbieżne krytyczne komponenty nadal wymagają portowania.

## 1. Ustal bazę i powód odmowy

Znajdź katalog repo przez `tools/ngd_patch.py` i `profiles/`. Przeczytaj README, `docs/BUILD.md`, `docs/INSTALACJA.md`, `docs/COFNIECIE.md`, `docs/PROVENANCE.md`, bieżący profil i narzędzie instalacyjne. Sprawdź status Git i lokalne zmiany. Zachowaj wydanie działające; port rozwijaj na kopii lub osobnej gałęzi. Odróżnij pliki OEM od plików już podmienionych przez bind mount.

Zapisz dokładny komunikat odmowy, wersję/commit instalatora i manifest wydania. Najpierw zbierz wszystkie rozbieżności odczytowo; nie kończ na pierwszym innym fingerprintcie. Istniejący `check` może zatrzymać się na pierwszej różnicy: nie zmieniaj warunków na PASS, żeby uzyskać dalsze dane. Porównuj osobno materiał zebrany poniżej.

Przeczytaj [tor techniczny i znane pułapki](references/technical-path.md). Użyj [szablonu oceny](references/assessment-template.md). Nigdy nie wpisuj przykładowych PID, dm-3, slotu _a, ścieżek ani hashy z radia referencyjnego jako faktów o nowej jednostce.

## 2. Ocena odczytowa: sprzęt i interfejsy, nie identyczny firmware

Zidentyfikuj jeden konkretny transport ADB i potwierdź tożsamość jednostki przed każdym etapem. Nie kontaktuj się z radiem, jeśli użytkownik zlecił pracę offline. Nie wykonuj `adb root` jako ukrytego odczytu: restartuje adbd; użyj istniejącego dostępu lub uzgodnij jego włączenie.

Zbierz minimalny materiał potrzebny do oceny:
- Android/API, ABI procesów, SoC/board, fingerprint jako identyfikator; model kontrolera BT, transport HCI i dostępne dowody firmware/vendor. Nazwa 6125 nie identyfikuje kontrolera.
- Manifest VINTF/HAL, pliki init i aktualni klienci HAL/HCI. Sam działający proces HAL nie dowodzi wyłącznego dostępu. Nie zakładaj dwóch kontrolerów.
- ELF bibliotek QTI: ELFCLASS64/AArch64, NEEDED, symbole, wersje i build IDs. Dla struktur/callbacków C++ wymagaj zgodnych nagłówków lub dowodów z binariów: równa nazwa symbolu nie dowodzi układu struktury.
- APK OEM: nazwy pakietów, manifesty, system/privileged status, certyfikaty, sharedUserId/UID, permissions; wybór usługi przez BluetoothManagerService/resolveSystemService. Nie wywołuj kolejnej instalacji, aby tylko sprawdzić podpis.
- Linker namespaces i dostępność pełnego domknięcia zależności JNI, nie tylko libbluetooth-binder.so.
- Audio HAL, audio policy, dostępne transporty Bluetooth audio i ustawienia offload; sprawdź role A2DP/AVRCP/HID/GATT rzeczywiście skonfigurowane, a nie tylko obecne w kodzie.
- BLINK lub odpowiednik: nadzorca, fork/wait/restart, zależności z MCU i pozostałymi funkcjami. AT+P0/exit(0) nie jest potwierdzoną metodą selektywnego wyłączenia.
- SELinux i domeny, montowania z perspektywy init, dynamiczne partycje, slot, AVB, system plików, wolne miejsce, blokady RO i dostępna metoda odzyskania.

Czytaj tylko potrzebne dane. Kopie binariów trzymaj prywatnie i oblicz hashe; nie publikuj backupów, kont, kluczy parowania, certyfikatów prywatnych ani surowych logów. HCI/btsnoop może zawierać dane prywatne.

## 3. Decyzja i mapa różnic

Dla każdego warunku zapisz dowód, wynik PASS/FAIL/UNKNOWN i potrzebną adaptację. Hash jest identyfikatorem/integralnością, nie samodzielnym testem zgodności ABI. Inny fingerprint, wygląd lub hash Settings nie przekreśla sprzętu. Ta sama nazwa procesora lub biblioteki nie potwierdza zgodności.

Używaj wyników:
- `TECHNICAL_CANDIDATE`: wszystkie krytyczne interfejsy i droga odzyskania mają wystarczające dowody do kontrolowanego testu; nie oznacza zgody instalatora ani sukcesu sprzętowego.
- `NEEDS_EVIDENCE`: konkretna niewiadoma; podaj najkrótszy odczyt albo analizę offline, która ją rozstrzygnie.
- `INCOMPATIBLE_CURRENT_PORT`: udowodniona przeszkoda dla obecnego portu, z opisem możliwości adaptacji.
- `HARDWARE_VERIFIED`: testy na danej konfiguracji z wynikami i zakresem. Nie promuj na podstawie podobieństwa.

Nie dodawaj `force`, nie wyłączaj HCI gate i nie zamieniaj nieznanych wyników na PASS. Zachowaj sprawdzony profil referencyjny.

## 4. Przygotuj rzeczywisty port offline

Zmień tylko elementy wynikające z dowodów. Nowy profil powinien rozpoznawać konkretne zweryfikowane komponenty i wymagania interfejsów, a nie wymagać kopii całego firmware. Po zakwalifikowaniu innej wersji binarnej przypnij jej hash dla integralności tego profilu.

Przejrzyj nie tylko JSON w `profiles/`, ale też `tools/ngd_patch.py` (PROFILE, load_profile, inspect, verify_release), pakowanie i testy oraz stałe w `src/session`, `src/installer`, kodzie APK/JNI i init. Obecny instalator i kod urządzenia mogą wymagać refaktoryzacji, zanim zaakceptują kolejny profil. Sam nowy JSON nie dodaje obsługi.

Nie używaj profilu do maskowania różnic ABI. W razie potrzeby dostosuj JNI do udowodnionych callbacków, manifest do zasad rozwiązywania usług, audio policy/offload i sekwencję startu właściwą dla OEM. Nie wymieniaj potwierdzonych komponentów bez potrzeby. Po zmianie APK/JNI zaktualizuj zgodnie podpisany manifest i hashe IPC/nadzorcy; nie przedstawiaj nowych artefaktów jako starego wydania.

Buduj w istniejącym pasującym AOSP Android 10/WSL/Linux z przypiętymi zależnościami. Przed dużym pobraniem uzgodnij zakres. Uruchom testy z repo oraz testy nowej logiki: odrzucenie złej architektury/ABI, nieznanego HCI, błędnego payloadu, nieudokumentowanego układu partycji i niesprawnego rollbacku. Syntetyczny test nie dowodzi działania hardware.

## 5. Plan odzyskania przed instalacją

Przygotuj backup tej jednostki, metadane, hashe, dokładną listę dodawanych/zmienianych plików i stanów, komendy cofnięcia oraz kontrolę odczytem po zmianie. Potwierdź drogę odzyskania także bez ADB po starcie. Sam backup plików ani założenie, że restart usunie bind mount, nie wystarcza dla trwałego autostartu init.

Nie kopiuj na ślepo sposobu zapisu dm-3/ext4 ani wartości reserved_clusters. Nie edytuj ręcznie PackageManager, nie chownuj danych, nie obchodź podpisów. Nie flashuj i nie zmieniaj SELinux, aby wymusić zgodność. Zatrzymanie całego BLINK może odłączyć CarPlay/AA i funkcje OEM; zachowanie procesu MCU nie dowodzi działania wszystkich jego funkcji.

## 6. Jeden kontrolowany test sprzętowy

Po zgodzie na konkretne operacje, z niezależnym nadzorcą/cofnięciem i limitem próby:
1. Potwierdź prawidłowe rozwiązanie systemowych usług i ładowanie JNI.
2. Zwolnij HCI udowodnioną metodą; sprawdź brak obcych klientów, dopiero wtedy dopuść QTI.
3. Potwierdź trwałe ON, Classic Inquiry i BLE Scan oddzielnie. Telefon do Classic musi być wykrywalny; brak na liście nie dowodzi awarii BLE/Classic.
4. Parowanie z odbiornikiem, A2DP Source/SBC, realny odsłuch z aplikacji. Stan CONNECTED nie dowodzi audio.
5. HID i audio jednocześnie, ręczne OFF/ON, ponowne połączenie, wznowienie i zimny start rozróżnione boot ID, dłuższe odtwarzanie i stabilność MCU.

STOP: obcy właściciel HCI, utrata kontroli/ADB bez sprawnego lokalnego cofnięcia, nieoczekiwany MCU, błędy JNI/podpisu, zła maska, brak odzyskania. Zapisz zdarzenie przed restartem; zachowaj stan zgodnie z uzgodnionym planem, nie uruchamiaj OEM automatycznie, jeśli grozi to dwoma klientami.

Rozdzielenie telefonu HFP i radia A2DP w Audi zostało potwierdzone przez właściciela konfiguracji referencyjnej. W innym aucie wymaga osobnego testu. A2DP Sink i przekazywanie TELEFON → RADIO → AUDI nie wynikają z działającego Source i nie są obiecywane.

## 7. Dostarcz rezultat

Dostarcz mapę zgodności, źródła dowodów, konkretne zmiany i artefakty z hashami, wyniki build/testów, nowy profil kandydujący i jedną procedurę testu/cofnięcia. Wydanie dopiero po rozdzieleniu poziomu offline od hardware. Nie publikuj danych OEM bez ustalenia praw. Zaktualizuj dokumentację PL/EN; nie wysyłaj na Git bez zakresu publikacji uzgodnionego z użytkownikiem.

This skill is an engineering workflow for a rejected unit, not an installer bypass. Preserve the verified baseline, establish hardware and ABI evidence, port only necessary integration, and require device-specific recovery and hardware tests before claiming support.