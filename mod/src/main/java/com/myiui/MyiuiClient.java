package com.myiui;

import com.myiui.bridge.GameDataBridge;
import com.myiui.bridge.HudBridge;
import com.myiui.bridge.IslandBridge;
import com.myiui.state.OverlayConnection;
import com.myiui.state.ScreenTracker;
import com.myiui.ws.OverlaySocketServer;
import net.fabricmc.api.ClientModInitializer;
import net.fabricmc.fabric.api.client.event.lifecycle.v1.ClientTickEvents;
import net.minecraft.client.Minecraft;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * Fabric client entry — pure data source + native UI suppressor.
 * All modern UI lives in the Electron overlay over WebSocket.
 */
public final class MyiuiClient implements ClientModInitializer {
    public static final String MOD_ID = "myiui";
    public static final Logger LOGGER = LoggerFactory.getLogger(MOD_ID);
    public static final String VERSION = /*$ mod_version*/ "2.0.0";
    public static final String MINECRAFT = /*$ minecraft*/ "1.21.6";

    public static final int WS_PORT = 25566;

    private static OverlaySocketServer socketServer;

    @Override
    public void onInitializeClient() {
        LOGGER.info("MyiUI {} starting (MC {})", VERSION, MINECRAFT);

        socketServer = new OverlaySocketServer(WS_PORT);
        socketServer.startAsync();

        ClientTickEvents.END_CLIENT_TICK.register(MyiuiClient::onEndTick);

        LOGGER.info("MyiUI WebSocket listening on ws://127.0.0.1:{}", WS_PORT);
    }

    private static void onEndTick(Minecraft client) {
        if (client == null) {
            return;
        }

        ScreenTracker.tick(client);
        OverlayConnection.tickWindow(client);

        if (client.player != null && client.level != null) {
            HudBridge.tick(client);
            IslandBridge.tick(client);
        }

        GameDataBridge.drainPendingActions(client);
        OverlaySocketServer server = socketServer;
        if (server != null) {
            server.flushOutbound();
        }
    }

    public static OverlaySocketServer getSocketServer() {
        return socketServer;
    }

    /** True when at least one Electron overlay is connected. */
    public static boolean isOverlayConnected() {
        OverlaySocketServer server = socketServer;
        return server != null && server.hasClients();
    }
}
