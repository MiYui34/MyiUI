package com.myiui.state;

import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.screens.Screen;

/**
 * Cross-version screen accessors.
 * <p>
 * 1.21 – 26.1.x: {@code Minecraft.screen} / {@code Minecraft#setScreen}<br>
 * 26.2+: {@code Minecraft.gui.screen()} / {@code Minecraft.gui.setScreen(...)}
 */
public final class McScreens {
    private McScreens() {}

    public static Screen get(Minecraft client) {
        //? if <26.2 {
        return client.screen;
        //?} else {
        /*return client.gui.screen();
        *///?}
    }

    public static void set(Minecraft client, Screen screen) {
        //? if <26.2 {
        client.setScreen(screen);
        //?} else {
        /*client.gui.setScreen(screen);
        *///?}
    }
}
