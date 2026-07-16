package com.myiui.bridge;

import com.google.gson.JsonArray;
import com.google.gson.JsonObject;
import com.myiui.MyiuiClient;
import com.myiui.state.McScreens;
import com.myiui.state.OverlayConnection;
import com.myiui.ws.OverlaySocketServer;
import com.myiui.ws.Protocol;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.screens.TitleScreen;
import net.minecraft.client.gui.screens.multiplayer.JoinMultiplayerScreen;
import net.minecraft.client.gui.screens.worldselection.SelectWorldScreen;
import net.minecraft.client.multiplayer.ServerData;
import net.minecraft.client.multiplayer.ServerList;
import org.java_websocket.WebSocket;

import java.nio.file.Files;
import java.nio.file.Path;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.stream.Stream;

/**
 * Handles Electron → Mod request/response commands on the Minecraft client thread.
 */
public final class GameDataBridge {
    private record Incoming(WebSocket conn, String id, String type, String cmd, JsonObject data) {}

    private static final ConcurrentLinkedQueue<Incoming> QUEUE = new ConcurrentLinkedQueue<>();

    private GameDataBridge() {}

    public static void enqueueIncoming(WebSocket conn, String id, String type, String cmd, JsonObject data) {
        QUEUE.offer(new Incoming(conn, id, type, cmd == null ? "" : cmd, data == null ? new JsonObject() : data));
    }

    public static void drainPendingActions(Minecraft client) {
        Incoming in;
        while ((in = QUEUE.poll()) != null) {
            handle(client, in);
        }
    }

    private static void handle(Minecraft client, Incoming in) {
        try {
            JsonObject result = dispatch(client, in.cmd(), in.data());
            if (in.id() != null) {
                OverlaySocketServer server = MyiuiClient.getSocketServer();
                if (server != null) {
                    server.send(in.conn(), Protocol.response(in.id(), true, result));
                }
            }
        } catch (Exception e) {
            MyiuiClient.LOGGER.warn("Action failed [{}]: {}", in.cmd(), e.toString());
            if (in.id() != null) {
                OverlaySocketServer server = MyiuiClient.getSocketServer();
                if (server != null) {
                    server.send(in.conn(), Protocol.responseError(in.id(), String.valueOf(e.getMessage())));
                }
            }
        }
    }

    private static JsonObject dispatch(Minecraft client, String cmd, JsonObject data) {
        if (cmd.startsWith("NE_")) {
            return NetEaseBridge.dispatch(client, cmd, data);
        }

        return switch (cmd) {
            case "OVERLAY_READY" -> {
                OverlayConnection.setOverlayReady(true);
                yield Protocol.obj().add("ready", true).build();
            }
            case "OVERLAY_SUSPEND" -> {
                OverlayConnection.setOverlayReady(false);
                yield Protocol.obj().add("ready", false).build();
            }
            case "QUIT" -> {
                client.stop();
                yield ok();
            }
            case "OPEN_SINGLEPLAYER" -> {
                McScreens.set(client, new SelectWorldScreen(safeParent(client)));
                yield ok();
            }
            case "OPEN_MULTIPLAYER" -> {
                McScreens.set(client, new JoinMultiplayerScreen(safeParent(client)));
                yield ok();
            }
            case "OPEN_OPTIONS" -> {
                openOptions(client);
                yield ok();
            }
            case "GET_PLAYER" -> getPlayer(client);
            case "GET_WORLDS" -> getWorlds(client);
            case "GET_SERVERS" -> getServers(client);
            case "UI_FLAGS" -> {
                if (data.has("chat")) {
                    ChatBridge.setElectronChatEnabled(data.get("chat").getAsBoolean());
                }
                yield Protocol.obj().add("applied", true).build();
            }
            case "ISLAND_CLEAR", "ISLAND_DEMO_CYCLE" -> ok();
            case "SET_BG_VIDEO" -> {
                String path = data.has("path") ? data.get("path").getAsString() : "";
                OverlaySocketServer server = MyiuiClient.getSocketServer();
                if (server != null) {
                    server.enqueue(Protocol.push("bg_video", Protocol.obj().add("path", path).build()));
                }
                yield ok();
            }
            case "JOIN_WORLD" -> {
                McScreens.set(client, new SelectWorldScreen(new TitleScreen()));
                yield Protocol.obj().add("opened", true).build();
            }
            case "CONNECT_SERVER" -> {
                McScreens.set(client, new JoinMultiplayerScreen(new TitleScreen()));
                yield Protocol.obj().add("opened", true).build();
            }
            default -> {
                if (cmd.startsWith("GET_OPTIONS") || cmd.startsWith("SET_OPTION")
                        || cmd.startsWith("SET_KEYBIND") || cmd.startsWith("SET_PACK")) {
                    yield OptionsBridge.dispatch(client, cmd, data);
                }
                MyiuiClient.LOGGER.info("Unknown cmd: {}", cmd);
                yield Protocol.obj().add("unknown", true).add("cmd", cmd).build();
            }
        };
    }

    private static net.minecraft.client.gui.screens.Screen safeParent(Minecraft client) {
        net.minecraft.client.gui.screens.Screen current = McScreens.get(client);
        return current != null ? current : new TitleScreen();
    }

    private static void openOptions(Minecraft client) {
        try {
            // 1.21.4+ package
            Class<?> cls = Class.forName("net.minecraft.client.gui.screens.options.OptionsScreen");
            var ctor = cls.getConstructor(net.minecraft.client.gui.screens.Screen.class, net.minecraft.client.Options.class);
            McScreens.set(client, (net.minecraft.client.gui.screens.Screen) ctor.newInstance(safeParent(client), client.options));
        } catch (ReflectiveOperationException e) {
            try {
                Class<?> cls = Class.forName("net.minecraft.client.gui.screens.OptionsScreen");
                var ctor = cls.getConstructor(net.minecraft.client.gui.screens.Screen.class, net.minecraft.client.Options.class);
                McScreens.set(client, (net.minecraft.client.gui.screens.Screen) ctor.newInstance(safeParent(client), client.options));
            } catch (ReflectiveOperationException e2) {
                throw new RuntimeException("OptionsScreen not found", e2);
            }
        }
    }

    private static JsonObject getPlayer(Minecraft client) {
        JsonObject o = new JsonObject();
        if (client.getUser() != null) {
            o.addProperty("name", client.getUser().getName());
            try {
                o.addProperty("uuid", client.getUser().getProfileId().toString());
            } catch (Throwable t) {
                o.addProperty("uuid", "");
            }
        }
        o.addProperty("minecraft", MyiuiClient.MINECRAFT);
        o.addProperty("modVersion", MyiuiClient.VERSION);
        return o;
    }

    private static JsonObject getWorlds(Minecraft client) {
        JsonArray arr = new JsonArray();
        try {
            Path saves = client.gameDirectory.toPath().resolve("saves");
            if (Files.isDirectory(saves)) {
                try (Stream<Path> stream = Files.list(saves)) {
                    stream.filter(Files::isDirectory).forEach(dir -> {
                        JsonObject w = new JsonObject();
                        w.addProperty("id", dir.getFileName().toString());
                        w.addProperty("name", dir.getFileName().toString());
                        arr.add(w);
                    });
                }
            }
        } catch (Exception e) {
            MyiuiClient.LOGGER.warn("GET_WORLDS failed: {}", e.toString());
        }
        JsonObject root = new JsonObject();
        root.add("worlds", arr);
        return root;
    }

    private static JsonObject getServers(Minecraft client) {
        JsonArray arr = new JsonArray();
        try {
            ServerList servers = new ServerList(client);
            servers.load();
            for (int i = 0; i < servers.size(); i++) {
                ServerData entry = servers.get(i);
                JsonObject s = new JsonObject();
                s.addProperty("id", String.valueOf(i));
                s.addProperty("name", entry.name);
                s.addProperty("address", entry.ip);
                arr.add(s);
            }
        } catch (Exception e) {
            MyiuiClient.LOGGER.warn("GET_SERVERS failed: {}", e.toString());
        }
        JsonObject root = new JsonObject();
        root.add("servers", arr);
        return root;
    }

    private static JsonObject ok() {
        return Protocol.obj().add("ok", true).build();
    }
}
