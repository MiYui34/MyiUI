package com.myiui.ws;

import com.google.gson.JsonObject;
import com.myiui.MyiuiClient;
import com.myiui.bridge.GameDataBridge;
import com.myiui.state.OverlayConnection;
import org.java_websocket.WebSocket;
import org.java_websocket.handshake.ClientHandshake;
import org.java_websocket.server.WebSocketServer;

import java.net.InetSocketAddress;
import java.util.Collections;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Localhost WebSocket server. Minecraft main thread only {@link #enqueue};
 * network IO runs on the Java-WebSocket selector thread.
 */
public final class OverlaySocketServer extends WebSocketServer {
    private final Set<WebSocket> clients = Collections.newSetFromMap(new ConcurrentHashMap<>());
    private final ConcurrentLinkedQueue<JsonObject> outbound = new ConcurrentLinkedQueue<>();
    private final ExecutorService starter = Executors.newSingleThreadExecutor(r -> {
        Thread t = new Thread(r, "myiui-ws-starter");
        t.setDaemon(true);
        return t;
    });
    private final AtomicBoolean started = new AtomicBoolean(false);

    public OverlaySocketServer(int port) {
        super(new InetSocketAddress("127.0.0.1", port));
        setReuseAddr(true);
    }

    public void startAsync() {
        if (!started.compareAndSet(false, true)) {
            return;
        }
        starter.execute(() -> {
            try {
                start();
            } catch (Exception e) {
                MyiuiClient.LOGGER.error("Failed to start WebSocket server", e);
            }
        });
    }

    public boolean hasClients() {
        return !clients.isEmpty();
    }

    /** Thread-safe: called from Minecraft client tick. */
    public void enqueue(JsonObject message) {
        if (message == null || clients.isEmpty()) {
            return;
        }
        outbound.offer(message);
    }

    public void enqueue(Protocol.ObjBuilder builder) {
        enqueue(builder.build());
    }

    /** Drain outbound queue on client tick (still non-blocking send). */
    public void flushOutbound() {
        JsonObject msg;
        while ((msg = outbound.poll()) != null) {
            broadcastJson(msg);
        }
    }

    public void broadcastJson(JsonObject message) {
        String text = Protocol.GSON.toJson(message);
        for (WebSocket client : clients) {
            if (client.isOpen()) {
                client.send(text);
            }
        }
    }

    public void send(WebSocket client, JsonObject message) {
        if (client != null && client.isOpen()) {
            client.send(Protocol.GSON.toJson(message));
        }
    }

    @Override
    public void onOpen(WebSocket conn, ClientHandshake handshake) {
        clients.add(conn);
        MyiuiClient.LOGGER.info("Overlay connected: {}", conn.getRemoteSocketAddress());
        JsonObject hello = Protocol.push("hello", Protocol.obj()
                .add("modVersion", MyiuiClient.VERSION)
                .add("minecraft", MyiuiClient.MINECRAFT)
                .add("port", MyiuiClient.WS_PORT)
                .build());
        send(conn, hello);
    }

    @Override
    public void onClose(WebSocket conn, int code, String reason, boolean remote) {
        clients.remove(conn);
        MyiuiClient.LOGGER.info("Overlay disconnected: {} ({})", reason, code);
        OverlayConnection.onClientDisconnected();
    }

    @Override
    public void onMessage(WebSocket conn, String message) {
        try {
            JsonObject root = Protocol.parse(message);
            String id = root.has("id") && !root.get("id").isJsonNull()
                    ? root.get("id").getAsString()
                    : null;
            String type = root.has("type") ? root.get("type").getAsString() : "";
            String cmd = root.has("cmd") ? root.get("cmd").getAsString() : "";
            JsonObject data = root.has("data") && root.get("data").isJsonObject()
                    ? root.getAsJsonObject("data")
                    : new JsonObject();

            if ("action".equals(type) || "query".equals(type) || !cmd.isEmpty()) {
                GameDataBridge.enqueueIncoming(conn, id, type.isEmpty() ? "action" : type, cmd, data);
            } else if ("ping".equals(type)) {
                send(conn, Protocol.push("pong", Protocol.obj().build()));
            }
        } catch (Exception e) {
            MyiuiClient.LOGGER.warn("Bad WS message: {}", e.toString());
        }
    }

    @Override
    public void onError(WebSocket conn, Exception ex) {
        MyiuiClient.LOGGER.warn("WebSocket error: {}", ex.toString());
    }

    @Override
    public void onStart() {
        MyiuiClient.LOGGER.info("WebSocket server started on {}", getAddress());
    }
}
