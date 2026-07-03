package com.myiui.agent.netease;

import com.myiui.agent.AgentLog;
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;

/**
 * 网易云播放器配置：API base URL、音质、超时、音量。
 * 读取顺序：LOCALAPPDATA\MyiUI\runtime\config\menu\netease.json → 项目根 config/menu/netease.json。
 */
public final class NetEaseConfig {
    private static volatile String baseUrl = "http://localhost:3000";
    private static volatile String quality = "exhigh";
    private static volatile boolean enableFlac = false;
    private static volatile int requestTimeoutMs = 8000;
    private static volatile boolean lyricsSync = true;
    private static volatile int volume = 80;

    private NetEaseConfig() {}

    public static void load() {
        try {
            Path cfg = resolveConfigPath();
            if (cfg == null || !Files.isRegularFile(cfg)) {
                AgentLog.info("NetEaseConfig: no config file, using defaults (base=" + baseUrl + ")");
                return;
            }
            JsonObject root = JsonParser.parseString(Files.readString(cfg, StandardCharsets.UTF_8)).getAsJsonObject();
            if (root.has("base_url")) baseUrl = root.get("base_url").getAsString().replaceAll("/+$", "");
            if (root.has("quality")) quality = root.get("quality").getAsString();
            if (root.has("enable_flac")) enableFlac = root.get("enable_flac").getAsBoolean();
            if (root.has("request_timeout_ms")) requestTimeoutMs = root.get("request_timeout_ms").getAsInt();
            if (root.has("lyrics_sync")) lyricsSync = root.get("lyrics_sync").getAsBoolean();
            if (root.has("volume")) volume = root.get("volume").getAsInt();
            AgentLog.info("NetEaseConfig loaded: base=" + baseUrl + " quality=" + quality + " flac=" + enableFlac);
        } catch (Throwable t) {
            AgentLog.error("NetEaseConfig load failed", t);
        }
    }

    public static String baseUrl() { return baseUrl; }
    public static String quality() { return quality; }
    public static boolean enableFlac() { return enableFlac; }
    public static int requestTimeoutMs() { return requestTimeoutMs; }
    public static boolean lyricsSync() { return lyricsSync; }
    public static int volume() { return volume; }

    public static void setVolume(int v) {
        volume = Math.max(0, Math.min(100, v));
    }

    private static Path resolveConfigPath() {
        String local = System.getenv("LOCALAPPDATA");
        if (local != null) {
            Path runtime = Paths.get(local, "MyiUI", "runtime", "config", "menu", "netease.json");
            if (Files.isRegularFile(runtime)) return runtime;
        }
        String env = System.getenv("MYIUI_ROOT");
        if (env != null && !env.isBlank()) {
            Path inRoot = Paths.get(env, "config", "menu", "netease.json");
            if (Files.isRegularFile(inRoot)) return inRoot;
        }
        if (local != null) {
            Path marker = Paths.get(local, "MyiUI", "project_root.txt");
            if (Files.isRegularFile(marker)) {
                try {
                    String root = Files.readString(marker, StandardCharsets.UTF_8).trim();
                    if (!root.isBlank()) {
                        Path inRoot = Paths.get(root, "config", "menu", "netease.json");
                        if (Files.isRegularFile(inRoot)) return inRoot;
                    }
                } catch (IOException ignored) {}
            }
        }
        return null;
    }

    /** 网易云数据目录：%LOCALAPPDATA%\MyiUI\netease\ */
    public static Path dataDir() {
        String local = System.getenv("LOCALAPPDATA");
        if (local == null) local = System.getProperty("java.io.tmpdir");
        return Paths.get(local, "MyiUI", "netease");
    }
}
