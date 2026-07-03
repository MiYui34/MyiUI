package com.myiui.agent;

final class ClassUtil {
    private ClassUtil() {}

    static boolean isTitleScreen(String internalName) {
        return "net/minecraft/class_442".equals(internalName)
                || "net/minecraft/client/gui/screen/TitleScreen".equals(internalName);
    }

    static boolean isScreen(String internalName) {
        return "net/minecraft/class_437".equals(internalName)
                || "net/minecraft/client/gui/screen/Screen".equals(internalName);
    }

    static boolean isMinecraftClient(String internalName) {
        return "net/minecraft/class_310".equals(internalName)
                || "net/minecraft/client/MinecraftClient".equals(internalName);
    }

    static boolean isInGameHud(String internalName) {
        return "net/minecraft/class_329".equals(internalName)
                || "net/minecraft/client/gui/hud/InGameHud".equals(internalName);
    }

    static boolean isPlayerListHud(String internalName) {
        return "net/minecraft/class_355".equals(internalName)
                || "net/minecraft/client/gui/hud/PlayerListHud".equals(internalName);
    }

    static boolean isTitleScreenInstance(Object screen) {
        if (screen == null) return false;
        String name = screen.getClass().getName();
        if ("net.minecraft.class_442".equals(name)
                || "net.minecraft.client.gui.screen.TitleScreen".equals(name)) {
            return true;
        }
        return name.endsWith("TitleScreen");
    }
}
