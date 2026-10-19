# Źródła i budowa / Source and build

Aktualny eksport zawiera [AVRCP Target i integrację startową](AVRCP.md). Dodatkowy moduł do budowy: `libngd_avrcp_legacy` (ARM64). Dokładnie testowane smali są w `patches/avrcp-legacy`; źródło Java odtwarza ich zachowanie, lecz nowy pełny rebuild APK/Soong nie został potwierdzony. Nadzorca w aktualnych źródłach przypina nowy zestaw APK/bridge. Przygotowanie bliźniaczego wariantu jawnie przywraca przypięcia bazowego APK bez AVRCP, zgodnie z używaną tam paczką bazową.

The current source export includes [AVRCP Target and startup integration](AVRCP.md). Build the additional ARM64 `libngd_avrcp_legacy` module. Exact tested smali is preserved separately; equivalent Java and the new full APK/Soong build have not been validated by a clean rebuild. The current supervisor pins the newer APK/bridge pair. Twin preparation restores baseline APK pins without the bridge, matching its baseline payload. See the AVRCP guide for offline kit assembly; do not pair newly compiled binaries with an old manifest.

## PL

Gotowy pakiet lokalny zachowuje dokładne hashe sprawdzonej kompilacji. Nie przebudowywaliśmy jej tylko na potrzeby pakowania.

`src/android-app` zawiera kopię źródeł pakietu AOSP z wprowadzonymi poprawkami JNI/Java/manifestu/zasobów. `src/session` to źródła faktycznie zbudowanego nadzorcy ETAP17, `src/installer` — pomocnik zapisu init, `src/permissions` — pomocnik baseline. `src/abi-headers` przechowuje użyte nagłówki ABI Qualcomm; nie wolno zastępować ich zgadywanymi prototypami. W repo zachowano historyczne patche ETAP12/14 oraz minimalny patch HID; nie są to skrypty instalacji.

Wymagane środowisko budowania: Linux/WSL2, drzewo AOSP Android10 odpowiadające zapisanej bazie, JDK9/prebuilty z tej gałęzi, Soong/Ninja/Clang/Bionic i ukryte biblioteki frameworka. To nie jest aplikacja do zbudowania samym Gradle/Android Studio. Repo nie pobiera automatycznie dużego drzewa AOSP.

W osobnym worktree drzewa AOSP wykorzystaj `tools/export_source_patch.py --upstream /sciezka/do/czystego/packages/apps/Bluetooth --output /tmp/ngd.patch`, przejrzyj i nałóż patch względem czystego pakietu. Nagłówki z `src/abi-headers/hardware` wymagają osobnego przeglądu w `system/bt/include/hardware`. OEM runtime biblioteki wskazane w jni/Android.bp trzeba dostarczyć z własnej zgodnej jednostki do `jni/ngd_runtime_oem`. JAR-y Room i pozostałe zależności muszą pochodzić z przypiętych zależności AOSP; archiwum źródłowe celowo nie zawiera lokalnego cache JAR-ów.

Uruchom standardowe envsetup i konfigurację ARM64/app-only BluetoothNGD zgodną z gałęzią, a następnie build modułów `BluetoothNGD`, `libbluetooth_jni_ngd`, `ngd16_native_bt`, `ngd_blink_hold12` i `ngd16_init_installer`. JNI źródeł/aplikacji i nadzorcy muszą korzystać z tych samych nagłówków IPC/ABI. `ngd12_artifact_digests.h` musi przypinać finalny podpisany APK/JNI. Nie kopiuj przebudowanych plików pod manifest sprawdzonego wydania: po zmianie hashów potrzebny jest nowy manifest, profil przeglądu oraz osobny test sprzętowy.

Eksport źródeł to rekonstrukcja aktualnego stanu projektu. Nie wykonano czystego, bitowo identycznego rebuilda całego APK z tego eksportu; sprawdzono źródła, kontrakt JNI i gotowe artefakty. Przenośny instalator celowo odrzuca inne hashe APK/nadzorcy.

## EN

The local binary bundle retains the exact hardware-tested hashes; packaging did not rebuild working components.

`src/android-app` contains the assembled AOSP app source with JNI/Java/manifest/resource fixes. `src/session` is the compiled ETAP17 guard's source; installer and permission helpers are separate. `src/abi-headers` contains the reviewed QTI layouts. Historical patches are source references, not installation scripts.

Building requires Linux/WSL2 and the matching Android10 AOSP tree, branch-provided JDK9/prebuilts, Soong/Ninja/Clang/Bionic and hidden framework libraries. This is not a standalone Gradle/Android Studio app. No large tree is automatically downloaded.

In an isolated AOSP worktree generate/review an overlay with `tools/export_source_patch.py --upstream /path/to/clean/packages/apps/Bluetooth --output /tmp/ngd.patch`. Review ABI header updates in system/bt separately. Supply the runtime ELF inputs referenced by jni/Android.bp from your own compatible unit, and restore Room JAR dependencies from pinned AOSP dependencies. Local build caches are not vendored.

Configure the branch's ARM64 app-only BluetoothNGD environment and build BluetoothNGD, libbluetooth_jni_ngd, ngd16_native_bt, ngd_blink_hold12 and ngd16_init_installer. Use consistent IPC/ABI headers. Update ngd12_artifact_digests.h to final signed APK/JNI hashes. A changed build needs its own reviewed manifest and hardware verification; it cannot masquerade as the tested release.

The export reconstructs the current source state. A clean bit-identical full APK rebuild from this export has not been performed. The distribution installer deliberately rejects unreviewed APK/controller hashes.
