package com.myiui.agent;

import com.myiui.agent.mapping.Mappings;

final class ClassUtil {
    private ClassUtil() {}

    static boolean isTitleScreen(String internalName) {
        return Mappings.isClass(Mappings.TITLE_SCREEN_CLASS, internalName);
    }

    static boolean isScreen(String internalName) {
        return Mappings.isClass(Mappings.SCREEN_CLASS, internalName);
    }

    static boolean isMinecraftClient(String internalName) {
        return Mappings.isClass(Mappings.MINECRAFT_CLIENT_CLASS, internalName);
    }

    static boolean isInGameHud(String internalName) {
        return Mappings.isClass(Mappings.IN_GAME_HUD_CLASS, internalName);
    }

    static boolean isPlayerListHud(String internalName) {
        return Mappings.isClass(Mappings.PLAYER_LIST_HUD_CLASS, internalName);
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
