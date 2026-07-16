package com.myiui.state;

import com.myiui.MyiuiClient;
import com.myiui.ws.OverlaySocketServer;
import com.myiui.ws.Protocol;
import net.minecraft.client.Minecraft;
import org.lwjgl.glfw.GLFW;

/**
 * Tracks whether the Electron overlay is connected and has acknowledged readiness.
 * Mixins must only cancel vanilla UI when the overlay is actually covering it.
 */
public final class OverlayConnection {
    private static volatile boolean overlayReady;
    private static volatile int lastWindowX = Integer.MIN_VALUE;
    private static volatile int lastWindowY = Integer.MIN_VALUE;
    private static volatile int lastWindowW;
    private static volatile int lastWindowH;

    private OverlayConnection() {}

    public static boolean shouldSuppressVanillaUi() {
        return MyiuiClient.isOverlayConnected() && overlayReady;
    }

    public static boolean isOverlayReady() {
        return overlayReady;
    }

    public static void setOverlayReady(boolean ready) {
        overlayReady = ready;
        MyiuiClient.LOGGER.info("Overlay ready={}", ready);
    }

    public static void onClientDisconnected() {
        if (!MyiuiClient.isOverlayConnected()) {
            overlayReady = false;
        }
    }

    public static void tickWindow(Minecraft client) {
        if (!MyiuiClient.isOverlayConnected()) {
            return;
        }
        var window = client.getWindow();
        int w = window.getWidth();
        int h = window.getHeight();
        long handle = glfwHandle(window);
        int[] xpos = new int[1];
        int[] ypos = new int[1];
        if (handle != 0L) {
            GLFW.glfwGetWindowPos(handle, xpos, ypos);
        }
        int x = xpos[0];
        int y = ypos[0];
        if (x == lastWindowX && y == lastWindowY && w == lastWindowW && h == lastWindowH) {
            return;
        }
        lastWindowX = x;
        lastWindowY = y;
        lastWindowW = w;
        lastWindowH = h;
        OverlaySocketServer server = MyiuiClient.getSocketServer();
        if (server != null) {
            server.enqueue(Protocol.push("window", Protocol.obj()
                    .add("x", x)
                    .add("y", y)
                    .add("width", w)
                    .add("height", h)
                    .add("guiScale", window.getGuiScale())
                    .add("fullscreen", window.isFullscreen())));
        }
    }

    /** 1.21: getWindow(); 26.x: getHandle() — resolve reflectively. */
    private static long glfwHandle(Object window) {
        try {
            return (long) window.getClass().getMethod("getWindow").invoke(window);
        } catch (ReflectiveOperationException e) {
            try {
                return (long) window.getClass().getMethod("getHandle").invoke(window);
            } catch (ReflectiveOperationException e2) {
                return 0L;
            }
        }
    }
}
