package com.myiui.agent;

/** Screen classification broadcast to native overlay via SHM. */
public final class ScreenKind {
    public static final byte NONE = 0;
    public static final byte MAIN_MENU = 1;
    public static final byte SUB_MENU = 2;
    public static final byte VIDEO_SETTINGS = 3;
    public static final byte IN_GAME = 4;

    private ScreenKind() {}

    static byte classify(Object screen) {
        if (screen == null) {
            return NONE;
        }
        if (ClassUtil.isTitleScreenInstance(screen)) {
            return MAIN_MENU;
        }
        String name = screen.getClass().getName();
        if (name.contains("SodiumOptionsGUI") || name.contains("VideoOptionsScreen")
                || name.contains("class_526")) {
            return VIDEO_SETTINGS;
        }
        return SUB_MENU;
    }

    static String label(byte kind) {
        return switch (kind) {
            case MAIN_MENU -> "MainMenu";
            case SUB_MENU -> "SubMenu";
            case VIDEO_SETTINGS -> "VideoSettings";
            case IN_GAME -> "InGame";
            default -> "None";
        };
    }
}
