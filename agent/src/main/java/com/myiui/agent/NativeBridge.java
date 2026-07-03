package com.myiui.agent;

public final class NativeBridge {
    private NativeBridge() {}

    public static native boolean startup();

    public static native int redefineClasses(Class<?> targetClass, byte[] newBytes);

    public static native byte[] getClassBytes(Class<?> targetClass);

    /** JVMTI GetLoadedClasses — populates real JVM class set for transformers. */
    public static native Class<?>[] jvmLoadedClasses();

    public static native void pushScreenState(byte kind, int seq, boolean overlayActive, boolean islandActive,
                                              boolean overlayAck);

    public static native void pushIslandState(byte valid, byte mode, byte activeSlot, byte notifyCount, short islandSeq,
                                              short fps, String title, String subtitle, String lyrics, byte[] slots,
                                              int notifyExpireMs);

    public static native void pushHudState(byte[] packed);

    public static native void pushTabListState(byte[] packed);

    public static native void pushVideoFrame(byte[] rgba, int width, int height, int frameIndex);
}
