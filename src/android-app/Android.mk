LOCAL_PATH:= $(call my-dir)
NGD_BLUETOOTH_LOCAL_PATH := $(LOCAL_PATH)

# MAP API module

include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := $(call all-java-files-under, lib/mapapi)
LOCAL_MODULE := bluetooth.mapsapi
include $(BUILD_STATIC_JAVA_LIBRARY)

# Bluetooth APK

include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := $(call all-java-files-under, src)
LOCAL_PACKAGE_NAME := BluetoothNGD
LOCAL_PRIVATE_PLATFORM_APIS := true
LOCAL_CERTIFICATE := platform
LOCAL_USE_AAPT2 := true
LOCAL_AAPT_FLAGS += --custom-package com.android.bluetooth
LOCAL_JNI_SHARED_LIBRARIES := libbluetooth_jni_ngd libngd_runtime_bluetooth_binder libngd_runtime_chrome libngd_runtime_nativehelper libngd_runtime_event
LOCAL_JAVA_LIBRARIES := javax.obex telephony-common services.net
LOCAL_STATIC_JAVA_LIBRARIES := \
        com.android.vcard \
        bluetooth.mapsapi \
        sap-api-java-static \
        services.net \
        libprotobuf-java-lite \
        bluetooth-protos-lite \

LOCAL_STATIC_ANDROID_LIBRARIES := \
        androidx.core_core \
        androidx.lifecycle_lifecycle-livedata \
        androidx.room_room-runtime \

LOCAL_ANNOTATION_PROCESSORS := \
        bt-androidx-annotation-nodeps \
        bt-androidx-room-common-nodeps \
        bt-androidx-room-compiler-nodeps \
        bt-androidx-room-migration-nodeps \
        bt-antlr4-nodeps \
        bt-apache-commons-codec-nodeps \
        bt-auto-common-nodeps \
        bt-javapoet-nodeps \
        bt-kotlin-metadata-nodeps \
        bt-sqlite-jdbc-nodeps \
        bt-jetbrain-nodeps \
        bt-guava-21-nodeps \
        kotlin-stdlib

LOCAL_ANNOTATION_PROCESSOR_CLASSES := \
        androidx.room.RoomProcessor

LOCAL_MULTILIB := 64
# Existing QTI library is used on-device; do not pull a replacement stack.
# LOCAL_REQUIRED_MODULES := libbluetooth
LOCAL_PROGUARD_ENABLED := disabled
# This platform app needs the hidden OBEX API. In tapas, java_common.mk otherwise
# redirects javax.obex to an empty public SDK stub even with private platform APIs.
# Select full platform headers only while processing this package, then restore
# the SDK mode before processing any other module. Soong's SDK mode is unchanged.
NGD_SAVED_PREBUILT_SDK_MODE := $(TARGET_BUILD_APPS_USE_PREBUILT_SDK)
TARGET_BUILD_APPS_USE_PREBUILT_SDK :=
include $(BUILD_PACKAGE)
TARGET_BUILD_APPS_USE_PREBUILT_SDK := $(NGD_SAVED_PREBUILT_SDK_MODE)

ifneq ($(filter BluetoothNGD,$(TARGET_BUILD_APPS)),)
# App-only build: do not pull robotest/VTS harnesses into the APK dependency set.
include $(filter-out $(LOCAL_PATH)/tests/%,$(call all-makefiles-under,$(LOCAL_PATH)))
else
include $(call all-makefiles-under,$(LOCAL_PATH))
endif

LOCAL_PATH := $(NGD_BLUETOOTH_LOCAL_PATH)
include $(CLEAR_VARS)

# Keep imports relative to this file, independently of nested test makefiles.
COMMON_LIBS_PATH := ../../../prebuilts/tools/common/m2/repository
ROOM_LIBS_PATH := lib/room

LOCAL_PREBUILT_STATIC_JAVA_LIBRARIES := \
        bt-androidx-annotation-nodeps:$(ROOM_LIBS_PATH)/annotation-1.0.0-beta01.jar \
        bt-androidx-room-common-nodeps:$(ROOM_LIBS_PATH)/room-common-2.0.0-beta01.jar \
        bt-androidx-room-compiler-nodeps:$(ROOM_LIBS_PATH)/room-compiler-2.0.0-beta01.jar \
        bt-androidx-room-migration-nodeps:$(ROOM_LIBS_PATH)/room-migration-2.0.0-beta01.jar \
        bt-antlr4-nodeps:$(COMMON_LIBS_PATH)/org/antlr/antlr4/4.5.3/antlr4-4.5.3.jar \
        bt-apache-commons-codec-nodeps:$(COMMON_LIBS_PATH)/org/eclipse/tycho/tycho-bundles-external/0.18.1/eclipse/plugins/org.apache.commons.codec_1.4.0.v201209201156.jar \
        bt-auto-common-nodeps:$(COMMON_LIBS_PATH)/com/google/auto/auto-common/0.9/auto-common-0.9.jar \
        bt-javapoet-nodeps:$(COMMON_LIBS_PATH)/com/squareup/javapoet/1.8.0/javapoet-1.8.0.jar \
        bt-kotlin-metadata-nodeps:$(COMMON_LIBS_PATH)/me/eugeniomarletti/kotlin-metadata/1.2.1/kotlin-metadata-1.2.1.jar \
        bt-sqlite-jdbc-nodeps:$(COMMON_LIBS_PATH)/org/xerial/sqlite-jdbc/3.20.1/sqlite-jdbc-3.20.1.jar \
        bt-jetbrain-nodeps:$(COMMON_LIBS_PATH)/org/jetbrains/annotations/13.0/annotations-13.0.jar \
        bt-guava-21-nodeps:$(COMMON_LIBS_PATH)/com/google/guava/guava/21.0/guava-21.0.jar

include $(BUILD_HOST_PREBUILT)
