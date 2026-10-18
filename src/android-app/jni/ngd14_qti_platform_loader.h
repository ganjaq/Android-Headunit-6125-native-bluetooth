// Android 10 platform-only loader. Called only after root admission and HCI release.
// Reuse OEM platform namespace; never bundle a second linker or Qualcomm stack.
#include <android/dlext.h>
#include <nativeloader/dlext_namespaces.h>
#include <dlfcn.h>
static void* ngd14LoadOemQti() {
  // This actual ROM explicitly exposes libandroid_runtime.so to app namespaces.
  // dlsym(handle) walks that library's original platform dependency graph.
  static void* runtime = dlopen("libandroid_runtime.so", RTLD_NOW | RTLD_NOLOAD);
  if (!runtime) { ALOGE("NGD14_QTI_LOAD_DENIED: platform runtime handle unavailable"); return nullptr; }
  auto get_namespace = reinterpret_cast<decltype(&android_get_exported_namespace)>(
      dlsym(runtime, "android_get_exported_namespace"));
  if (!get_namespace) { ALOGE("NGD14_QTI_LOAD_DENIED: exported namespace API unavailable"); return nullptr; }
  android_namespace_t* ns = get_namespace("platform");
  if (!ns) ns = get_namespace("default");
  if (!ns) { ALOGE("NGD14_QTI_LOAD_DENIED: OEM platform namespace not exported"); return nullptr; }
  android_dlextinfo info = {};
  info.flags = ANDROID_DLEXT_USE_NAMESPACE;
  info.library_namespace = ns;
  void* qti = android_dlopen_ext("/system/lib64/libbluetooth_qti.so", RTLD_NOW, &info);
  if (!qti) ALOGE("NGD14_QTI_LOAD_DENIED: %s", dlerror());
  else ALOGI("NGD14_QTI_PLATFORM_NAMESPACE: original OEM stack loaded after root admission");
  return qti;
}
