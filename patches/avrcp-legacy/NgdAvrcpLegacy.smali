.class final Lcom/android/bluetooth/avrcp/NgdAvrcpLegacy;
.super Ljava/lang/Object;
.field private static loaded:Z
.field private static active:Z
.method static constructor <clinit>()V
    .registers 2
    :try_start
    const-string v0, "ngd_avrcp_legacy"
    invoke-static {v0}, Ljava/lang/System;->loadLibrary(Ljava/lang/String;)V
    const/4 v0, 0x1
    sput-boolean v0, Lcom/android/bluetooth/avrcp/NgdAvrcpLegacy;->loaded:Z
    :try_end
    .catch Ljava/lang/LinkageError; {:try_start .. :try_end} :failed
    return-void
    :failed
    move-exception v0
    const-string v0, "NgdAvrcpLegacy"
    const-string v1, "Bridge library load failed"
    invoke-static {v0, v1}, Landroid/util/Log;->e(Ljava/lang/String;Ljava/lang/String;)I
    return-void
.end method
.method static start(Lcom/android/bluetooth/avrcp/AvrcpNativeInterface;)Z
    .registers 2
    sget-boolean v0, Lcom/android/bluetooth/avrcp/NgdAvrcpLegacy;->loaded:Z
    if-eqz v0, :done
    invoke-static {p0}, Lcom/android/bluetooth/avrcp/NgdAvrcpLegacy;->init(Lcom/android/bluetooth/avrcp/AvrcpNativeInterface;)Z
    move-result v0
    sput-boolean v0, Lcom/android/bluetooth/avrcp/NgdAvrcpLegacy;->active:Z
    :done
    return v0
.end method
.method static notifyUpdate(ZZZ)Z
    .registers 4
    sget-boolean v0, Lcom/android/bluetooth/avrcp/NgdAvrcpLegacy;->active:Z
    if-eqz v0, :done
    invoke-static {p0, p1, p2}, Lcom/android/bluetooth/avrcp/NgdAvrcpLegacy;->update(ZZZ)V
    :done
    return v0
.end method
.method static notifyFolder(ZZZ)Z
    .registers 5
    or-int p0, p0, p1
    or-int p0, p0, p2
    const/4 v0, 0x0
    invoke-static {v0, v0, p0}, Lcom/android/bluetooth/avrcp/NgdAvrcpLegacy;->notifyUpdate(ZZZ)Z
    move-result v1
    return v1
.end method
.method static stop()V
    .registers 1
    sget-boolean v0, Lcom/android/bluetooth/avrcp/NgdAvrcpLegacy;->active:Z
    if-eqz v0, :done
    invoke-static {}, Lcom/android/bluetooth/avrcp/NgdAvrcpLegacy;->cleanup()V
    const/4 v0, 0x0
    sput-boolean v0, Lcom/android/bluetooth/avrcp/NgdAvrcpLegacy;->active:Z
    :done
    return-void
.end method
.method private static native init(Lcom/android/bluetooth/avrcp/AvrcpNativeInterface;)Z
.end method
.method private static native update(ZZZ)V
.end method
.method private static native cleanup()V
.end method
