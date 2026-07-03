package com.myiui.agent;

import java.io.IOException;
import java.nio.file.DirectoryStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.List;

public final class WorldBridge {
    private WorldBridge() {}

    public static String getWorldsJson() {
        try {
            Path saves = resolveSavesDir();
            if (saves == null || !Files.isDirectory(saves)) {
                return "{\"worlds\":[]}";
            }
            StringBuilder sb = new StringBuilder("{\"worlds\":[");
            boolean first = true;
            try (DirectoryStream<Path> stream = Files.newDirectoryStream(saves)) {
                for (Path entry : stream) {
                    if (!Files.isDirectory(entry)) continue;
                    String name = entry.getFileName().toString();
                    if (!first) sb.append(',');
                    first = false;
                    sb.append("{\"name\":\"").append(escape(name)).append("\",\"mode\":\"survival\",\"last_played\":\"\"}");
                }
            }
            sb.append("]}");
            return sb.toString();
        } catch (IOException e) {
            AgentLog.error("GET_WORLDS failed", e);
            return null;
        }
    }

    public static boolean joinWorld(String worldName) {
        return GameActions.joinWorld(worldName);
    }

    public static boolean deleteWorld(String worldName) {
        try {
            Path saves = resolveSavesDir();
            if (saves == null) return false;
            Path world = saves.resolve(worldName);
            if (!Files.exists(world)) return false;
            deleteRecursive(world);
            return true;
        } catch (IOException e) {
            AgentLog.error("DELETE_WORLD failed", e);
            return false;
        }
    }

    public static boolean createWorld() {
        return GameActions.openCreateWorld();
    }

    public static boolean submitCreateWorld(String name, String mode, String seed) {
        return GameActions.submitCreateWorld(name, mode, seed);
    }

    private static Path resolveSavesDir() {
        Path saves = GameActions.getSavesDirectory();
        if (saves != null) {
            return saves;
        }
        String appData = System.getenv("APPDATA");
        if (appData == null) return null;
        return Paths.get(appData, ".minecraft", "saves");
    }

    private static void deleteRecursive(Path path) throws IOException {
        if (Files.isDirectory(path)) {
            try (DirectoryStream<Path> stream = Files.newDirectoryStream(path)) {
                for (Path child : stream) {
                    deleteRecursive(child);
                }
            }
        }
        Files.deleteIfExists(path);
    }

    static String escape(String s) {
        return s.replace("\\", "\\\\").replace("\"", "\\\"");
    }
}
