# Tor techniczny i pułapki referencyjnego portu

## Kolejność integracji

Android Settings/SystemUI → BluetoothManagerService → systemowy pakiet / AdapterService, GattService, A2dpService i HidHostService → JNI NGD → libbluetooth_qti.so → Qualcomm Bluetooth HAL → jedyny kontroler HCI.

Audio: aplikacja radia → AudioFlinger/AudioPolicy → właściwy Bluetooth audio HAL/offload SBC → Qualcomm → Audi A2DP Sink. Audio, HAL i natywna sesja muszą mieć spójną konfigurację przed startem stosu.

Na jednostce referencyjnej muzyka ruszyła dopiero po konfiguracji sprzętowego SBC i odświeżeniu audio HAL. Historyczne wartości `persist.bluetooth.a2dp_offload.disabled=false` i `persist.vendor.bt.a2dp.hal.implementation=true` nie są receptą dla każdego OEM. Zbadaj, gdzie dany HAL odczytuje ustawienia i co trzeba zrestartować; restart audio może objąć powiązane usługi.

## Tożsamość systemowa

Integracja wykorzystuje org.codeaurora.bluetooth jako systemowy pakiet z UID 1002. Historia prób /data/app pokazała problemy z tożsamością i wyborem usługi. Nie powtarzaj takiej aktualizacji w ciemno. Nie zgaduj platformowego podpisu po nazwie klucza. Systemowy bind view i init autostart są mechanizmem konkretnego portu, nie uniwersalnym API Androida.

`libbluetooth-binder.so` istniejąca w /system/lib64 nie była automatycznie dostępna aplikacji /data/app: namespace linkera i pełne zależności mają znaczenie. Znalezienie pliku nie jest testem jego załadowania.

## Wyłączność HCI i MCU

BLINK na jednostce referencyjnej zajmował HAL. AT+P0 kończył worker, który mógł zostać ponownie uruchomiony przez nadzorcę. Zastosowano zatrzymanie całej usługi BLINK i maskowanie jej pliku wykonywalnego, nie potwierdzono selektywnej pauzy samego BT. mcuserver pozostał osobnym procesem. Dla nowego OEM trzeba sprawdzić tę zależność ponownie.

Autostart ma zamkniętą bramkę QTI do czasu pełnych kontroli. Odróżnij przejściowy błąd odczytu /proc od rzeczywistej niezgodności; ponowienie musi wykonać pełną kontrolę. Nie zachowuj starego PASS po błędzie i nie wyłączaj kontroli namespace.

## Zdarzenia z 2026-10-17

- Zapisano OFF po `CHECK barrier=0`; stary log nie podawał dokładnego warunku. Nie udowodniono związku z YouTube ani Yanosikiem.
- W osobnym, odtworzonym zdarzeniu własny root chroot Codexa widział oryginalny `/system/bin/blink`, choć Android widział maskę. Nadzorca poprawnie zgłosił NAMESPACE_MISMATCH i zamknął BT.
- Zwykły bind katalogu /system nie gwarantował zachowania istniejących podmontowań. Sprawdzaj dev/inode przez `/proc/PID/root/system/bin/blink` z rzeczywistego procesu. Nie zakładaj, że nazwa rbind w narzędziu OEM gwarantuje oczekiwane działanie.
- Dodatek root Codexa został na żądanie właściciela usunięty. Nie instaluj go w ramach portowania Bluetooth.
- Lokalnie zbudowany kontroler z ponowieniami i dokładniejszym logowaniem nie musi być tym samym kontrolerem co publiczne wydanie. Ustal aktualny kod, manifest i hash; nie promuj poprawki bez testów.

## Status dowodów

Właściciel potwierdził odsłuch SBC, jednoczesny Xbox HID, manualne OFF/ON, FM→BT, automatyczne połączenie po cyklu zapłonu oraz podział telefonu do rozmów i radia do audio w Audi. Nie oznacza to testu na innym firmware, pełnego pokrycia MCU ani gwarancji wielogodzinnej stabilności. Każde nowe wydanie musi jawnie podawać własny zakres dowodów.