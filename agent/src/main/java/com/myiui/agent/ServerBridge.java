package com.myiui.agent;

import com.google.gson.Gson;
import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import java.util.UUID;

public final class ServerBridge {
    private ServerBridge() {}

    public static final class ServerEntry {
        public final String id;
        public final String name;
        public final String address;

        public ServerEntry(String id, String name, String address) {
            this.id = id;
            this.name = name;
            this.address = address;
        }
    }

    public static String getServersJson() {
        try {
            List<ServerEntry> entries = readServerEntries();
            StringBuilder sb = new StringBuilder("{\"servers\":[");
            for (int i = 0; i < entries.size(); i++) {
                if (i > 0) sb.append(',');
                ServerEntry entry = entries.get(i);
                sb.append("{\"id\":\"").append(escape(entry.id))
                        .append("\",\"name\":\"").append(escape(entry.name))
                        .append("\",\"address\":\"").append(escape(entry.address)).append("\"}");
            }
            sb.append("]}");
            return sb.toString();
        } catch (IOException e) {
            AgentLog.error("GET_SERVERS failed", e);
            return null;
        }
    }

    public static ServerEntry findServer(String id) {
        if (id == null || id.isEmpty()) return null;
        try {
            for (ServerEntry entry : readServerEntries()) {
                if (id.equals(entry.id) || id.equals(entry.address) || id.equals(entry.name)) {
                    return entry;
                }
            }
        } catch (IOException e) {
            AgentLog.error("findServer failed", e);
        }
        return null;
    }

    public static boolean connectServer(String id) {
        return GameActions.connectServer(id);
    }

    public static boolean addServer() {
        return GameActions.openAddServer();
    }

    public static boolean addServerEntry(String name, String address) {
        try {
            List<ServerEntry> entries = readServerEntries();
            entries.add(new ServerEntry(stableId(address), name, address));
            writeServerEntries(entries);
            AgentLog.info("Added server: " + name + " -> " + address);
            return true;
        } catch (IOException e) {
            AgentLog.error("ADD_SERVER_SUBMIT failed", e);
            return false;
        }
    }

    public static boolean editServerEntry(String id, String name, String address) {
        try {
            List<ServerEntry> entries = readServerEntries();
            boolean updated = false;
            for (int i = 0; i < entries.size(); i++) {
                ServerEntry entry = entries.get(i);
                if (id.equals(entry.id) || id.equals(entry.address) || id.equals(entry.name)) {
                    entries.set(i, new ServerEntry(stableId(address), name, address));
                    updated = true;
                    break;
                }
            }
            if (!updated) return false;
            writeServerEntries(entries);
            AgentLog.info("Edited server: " + name + " -> " + address);
            return true;
        } catch (IOException e) {
            AgentLog.error("EDIT_SERVER_SUBMIT failed", e);
            return false;
        }
    }

    public static boolean deleteServerEntry(String id) {
        try {
            List<ServerEntry> entries = readServerEntries();
            boolean removed = entries.removeIf(entry -> id.equals(entry.id) || id.equals(entry.address) || id.equals(entry.name));
            if (!removed) return false;
            writeServerEntries(entries);
            AgentLog.info("Deleted server: " + id);
            return true;
        } catch (IOException e) {
            AgentLog.error("DELETE_SERVER failed", e);
            return false;
        }
    }

    private static List<ServerEntry> readServerEntries() throws IOException {
        Path json = resolveServersJson();
        if (!Files.isRegularFile(json)) {
            return new ArrayList<>();
        }
        String raw = Files.readString(json, StandardCharsets.UTF_8).trim();
        if (raw.isEmpty()) return new ArrayList<>();

        List<ServerEntry> entries = new ArrayList<>();
        JsonElement root = JsonParser.parseString(raw);
        JsonArray array;
        if (root.isJsonArray()) {
            array = root.getAsJsonArray();
        } else if (root.isJsonObject() && root.getAsJsonObject().has("servers")) {
            array = root.getAsJsonObject().getAsJsonArray("servers");
        } else {
            return entries;
        }

        int index = 0;
        for (JsonElement element : array) {
            if (!element.isJsonObject()) continue;
            JsonObject obj = element.getAsJsonObject();
            String name = readString(obj, "name");
            String address = readString(obj, "address");
            if (address == null || address.isEmpty()) {
                address = readString(obj, "ip");
            }
            if (name == null) name = "";
            if (address == null) address = "";
            String id = readString(obj, "id");
            if (id == null || id.isEmpty()) {
                id = stableId(address.isEmpty() ? String.valueOf(index) : address);
            }
            entries.add(new ServerEntry(id, name, address));
            index++;
        }
        return entries;
    }

    private static void writeServerEntries(List<ServerEntry> entries) throws IOException {
        Path json = resolveServersJson();
        JsonArray array = new JsonArray();
        for (ServerEntry entry : entries) {
            JsonObject obj = new JsonObject();
            obj.addProperty("name", entry.name);
            obj.addProperty("ip", entry.address);
            array.add(obj);
        }
        Files.createDirectories(json.getParent());
        Files.writeString(json, new Gson().toJson(array), StandardCharsets.UTF_8);
    }

    private static String readString(JsonObject obj, String key) {
        if (!obj.has(key) || obj.get(key).isJsonNull()) return null;
        return obj.get(key).getAsString();
    }

    private static String stableId(String address) {
        if (address == null || address.isEmpty()) {
            return UUID.randomUUID().toString();
        }
        return address;
    }

    private static Path resolveServersJson() {
        String appData = System.getenv("APPDATA");
        if (appData == null) return Path.of("servers.json");
        return Path.of(appData, ".minecraft", "servers.json");
    }

    static String escape(String s) {
        return s.replace("\\", "\\\\").replace("\"", "\\\"");
    }
}
