package com.myiui.bridge;

import com.google.gson.JsonObject;
import com.myiui.MyiuiClient;
import com.myiui.ws.OverlaySocketServer;
import com.myiui.ws.Protocol;
import net.minecraft.network.chat.Component;

/**
 * Pushes chat lines to Electron for the frosted chat UI.
 */
public final class ChatBridge {
    private static volatile boolean electronChatEnabled = true;

    private ChatBridge() {}

    public static boolean isElectronChatEnabled() {
        return electronChatEnabled;
    }

    public static void setElectronChatEnabled(boolean enabled) {
        electronChatEnabled = enabled;
    }

    public static void onMessage(Component message) {
        if (message == null) {
            return;
        }
        OverlaySocketServer server = MyiuiClient.getSocketServer();
        if (server == null || !server.hasClients()) {
            return;
        }
        JsonObject data = Protocol.obj()
                .add("text", message.getString())
                .add("timestamp", System.currentTimeMillis())
                .build();
        server.enqueue(Protocol.push("chat", data));
    }
}
