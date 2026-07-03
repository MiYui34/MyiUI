package com.myiui.agent;

import com.google.gson.JsonObject;
import com.google.gson.JsonParser;

import java.nio.file.Files;
import java.nio.file.Path;

final class MediaLimits {
    private static long maxImageBytes = 15L * 1024 * 1024;
    private static long maxVideoBytes = 300L * 1024 * 1024;

    private MediaLimits() {}

    static void loadFromConfig() {
        try {
            Path cfg = ConfigPaths.backgroundJson();
            if (!Files.exists(cfg)) return;
            JsonObject root = JsonParser.parseString(Files.readString(cfg)).getAsJsonObject();
            if (root.has("max_image_mb")) {
                maxImageBytes = (long) (root.get("max_image_mb").getAsDouble() * 1024 * 1024);
            }
            if (root.has("max_video_mb")) {
                maxVideoBytes = (long) (root.get("max_video_mb").getAsDouble() * 1024 * 1024);
            }
        } catch (Exception ignored) {
        }
    }

    static boolean isWithinLimit(Path path) {
        try {
            if (!Files.isRegularFile(path)) return false;
            long size = Files.size(path);
            String lower = path.toString().toLowerCase();
            if (lower.endsWith(".png") || lower.endsWith(".jpg") || lower.endsWith(".jpeg")) {
                return size <= maxImageBytes;
            }
            if (lower.endsWith(".mp4") || lower.endsWith(".webm")) {
                return size <= maxVideoBytes;
            }
            return false;
        } catch (Exception e) {
            return false;
        }
    }

    static String limitMessage(Path path) {
        String lower = path.toString().toLowerCase();
        if (lower.endsWith(".mp4") || lower.endsWith(".webm")) {
            return "视频超过 " + (maxVideoBytes / (1024 * 1024)) + " MB 限制";
        }
        return "图片超过 " + (maxImageBytes / (1024 * 1024)) + " MB 限制";
    }
}
