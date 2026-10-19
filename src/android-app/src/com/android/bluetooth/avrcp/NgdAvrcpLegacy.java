/* Copyright 2026 NEXTGENDRIVE contributors. SPDX-License-Identifier: Apache-2.0 */
package com.android.bluetooth.avrcp;

import android.util.Log;

/** Java equivalent of the device-tested NgdAvrcpLegacy smali patch. */
final class NgdAvrcpLegacy {
    private static boolean loaded;
    private static boolean active;

    static {
        try {
            System.loadLibrary("ngd_avrcp_legacy");
            loaded = true;
        } catch (LinkageError error) {
            Log.e("NgdAvrcpLegacy", "Bridge library load failed");
        }
    }

    static boolean start(AvrcpNativeInterface owner) {
        if (!loaded) return false;
        active = init(owner);
        return active;
    }

    static boolean notifyUpdate(boolean metadata, boolean playStatus, boolean queue) {
        if (active) update(metadata, playStatus, queue);
        return active;
    }

    static boolean notifyFolder(boolean availablePlayers, boolean addressedPlayers, boolean uids) {
        return notifyUpdate(false, false, availablePlayers | addressedPlayers | uids);
    }

    static void stop() {
        if (active) {
            cleanup();
            active = false;
        }
    }

    private static native boolean init(AvrcpNativeInterface owner);
    private static native void update(boolean metadata, boolean playStatus, boolean queue);
    private static native void cleanup();
}
