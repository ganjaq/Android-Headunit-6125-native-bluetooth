# Ocena jednostki odrzuconej przez instalator

Commit narzędzi / manifest wydania:
Identyfikator konfiguracji (bez numerów seryjnych/kont):
Powód odmowy i rozbieżności:
Stan początkowy: OEM / zmodyfikowany / nieustalony:

| Obszar | Dowód i źródło | PASS / FAIL / UNKNOWN | Potrzebna adaptacja |
|---|---|---|---|
| SoC / kontroler / transport HCI | | | |
| Android / ABI / HAL VINTF | | | |
| ABI/JNI QTI i callbacki | | | |
| Certyfikat / UID / resolveSystemService | | | |
| Linker i zależności ELF | | | |
| Audio HAL / policy / SBC | | | |
| Profile aktywne Source / HID / GATT | | | |
| Wyłączność HCI / BLINK / nadzorca | | | |
| MCU i funkcje OEM | | | |
| SELinux / init / namespaces | | | |
| Partycje / montowania / zapis | | | |
| Backup / odzyskanie bez ADB | | | |

Werdykt: NEEDS_EVIDENCE / INCOMPATIBLE_CURRENT_PORT / TECHNICAL_CANDIDATE / HARDWARE_VERIFIED.
Uzasadnienie i niewiadome:
Zmiany w profilu, narzędziach i kodzie urządzenia:
Zachowana wersja powrotna:
Wynik kompilacji i testów offline:
Plan jednej próby sprzętowej, zakres zgody i warunki STOP:
Dokładne cofnięcie każdej operacji i dowód wykonalności:
Wyniki sprzętowe (ON, Classic, BLE, pairing, odsłuch, HID, OFF/ON, resume/cold boot, MCU, czas stabilności):
Jedna konkretna następna operacja:

Brak dowodu pozostaje UNKNOWN. Sam fingerprint/hash różny od bazy nie jest technicznym FAIL; hash zgodny nie jest samodzielnym dowodem pełnej zgodności.