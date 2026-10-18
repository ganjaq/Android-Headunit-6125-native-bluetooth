// Layout checks derived from inspected ARM64 device ELF and source headers.
// These checks do not prove semantic callback compatibility or runtime safety.
#include <cstddef>
#include <vector>
#include <hardware/bluetooth.h>
#include <hardware/bt_av.h>
#include <hardware/bt_gatt.h>
static_assert(sizeof(void*) == 8, "Build ARM64 JNI only");
static_assert(sizeof(bt_interface_t) == 280, "Device interface size mismatch");
static_assert(offsetof(bt_interface_t, get_profile_interface) == 176, "Slot mismatch");
static_assert(sizeof(bt_callbacks_t) == 112, "Firmware callbacks object is 112 bytes");
static_assert(sizeof(btav_source_interface_t) == 64, "Source table size mismatch");
static_assert(sizeof(btav_source_callbacks_t) == 32, "Audit source callbacks");
static_assert(sizeof(btav_a2dp_codec_config_t) == 56, "Vector element mismatch");
static_assert(sizeof(std::vector<btav_a2dp_codec_config_t>) == 24, "Vector layout mismatch");
static_assert(BTAV_A2DP_CODEC_INDEX_SOURCE_SBC == 0, "SBC enum mismatch");
static_assert(BTAV_A2DP_CODEC_INDEX_SOURCE_LDAC == 5, "Firmware LDAC enum mismatch");
static_assert(sizeof(btgatt_interface_t) == 56, "Firmware GATT interface size mismatch");
static_assert(offsetof(btgatt_interface_t, scanner) == 40, "QTI scanner slot mismatch");
static_assert(offsetof(btgatt_interface_t, advertiser) == 48, "QTI advertiser slot mismatch");
static_assert(sizeof(btgatt_callbacks_t) == 32, "Original JNI GATT callback container size");
static_assert(sizeof(btgatt_client_callbacks_t) == 152, "Original JNI client callback table size");
static_assert(sizeof(btgatt_server_callbacks_t) == 128, "Original JNI server callback table size");
