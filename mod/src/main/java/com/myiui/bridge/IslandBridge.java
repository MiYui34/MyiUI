package com.myiui.bridge;

import com.google.gson.JsonArray;
import com.google.gson.JsonObject;
import com.myiui.MyiuiClient;
import com.myiui.ws.OverlaySocketServer;
import com.myiui.ws.Protocol;
import net.minecraft.client.Minecraft;
import net.minecraft.client.multiplayer.PlayerInfo;

import java.util.Collection;

/**
 * Publishes Dynamic Island + tab-list snapshots when dirty.
 */
public final class IslandBridge {
    private static String lastFingerprint = "";

    private IslandBridge() {}

    public static void tick(Minecraft client) {
        if (client.getConnection() == null) {
            return;
        }
        Collection<PlayerInfo> players = client.getConnection().getOnlinePlayers();
        StringBuilder fp = new StringBuilder();
        JsonArray list = new JsonArray();
        for (PlayerInfo info : players) {
            String name = resolveName(info);
            int latency = info.getLatency();
            fp.append(name).append(':').append(latency).append(';');
            JsonObject p = new JsonObject();
            p.addProperty("name", name);
            p.addProperty("latency", latency);
            list.add(p);
        }
        String fingerprint = fp.toString();
        if (fingerprint.equals(lastFingerprint)) {
            return;
        }
        lastFingerprint = fingerprint;

        OverlaySocketServer server = MyiuiClient.getSocketServer();
        if (server == null) {
            return;
        }
        JsonObject island = Protocol.obj()
                .add("mode", "tablist")
                .add("playerCount", players.size())
                .add("players", list)
                .build();
        server.enqueue(Protocol.push("island", island));
        server.enqueue(Protocol.push("tablist", Protocol.obj().add("players", list).build()));
    }

    private static String resolveName(PlayerInfo info) {
        try {
            var profile = info.getProfile();
            try {
                return (String) profile.getClass().getMethod("getName").invoke(profile);
            } catch (NoSuchMethodException e) {
                return (String) profile.getClass().getMethod("name").invoke(profile);
            }
        } catch (ReflectiveOperationException e) {
            return "?";
        }
    }
}
