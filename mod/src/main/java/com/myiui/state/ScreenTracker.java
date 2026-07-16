package com.myiui.state;

import com.myiui.MyiuiClient;
import com.myiui.ws.OverlaySocketServer;
import com.myiui.ws.Protocol;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.screens.Screen;
import net.minecraft.client.gui.screens.TitleScreen;

/**
 * Publishes coarse screen kind changes to the Electron overlay.
 */
public final class ScreenTracker {
    public enum Kind {
        UNKNOWN,
        TITLE,
        IN_GAME,
        PAUSE,
        OTHER
    }

    private static Kind last = Kind.UNKNOWN;

    private ScreenTracker() {}

    public static void tick(Minecraft client) {
        Kind kind = resolve(client);
        if (kind == last) {
            return;
        }
        last = kind;
        OverlaySocketServer server = MyiuiClient.getSocketServer();
        if (server != null && server.hasClients()) {
            Screen screen = McScreens.get(client);
            String screenClass = screen == null ? "" : screen.getClass().getSimpleName();
            server.enqueue(Protocol.push("screen", Protocol.obj()
                    .add("kind", kind.name())
                    .add("hasWorld", client.level != null)
                    .add("screenClass", screenClass)));
        }
    }

    public static Kind current() {
        return last;
    }

    private static Kind resolve(Minecraft client) {
        Screen screen = McScreens.get(client);
        if (screen == null) {
            return client.level != null ? Kind.IN_GAME : Kind.UNKNOWN;
        }
        if (screen instanceof TitleScreen) {
            return Kind.TITLE;
        }
        if (client.level != null) {
            return Kind.PAUSE;
        }
        return Kind.OTHER;
    }
}
