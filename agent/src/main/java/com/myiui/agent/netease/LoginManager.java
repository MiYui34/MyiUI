package com.myiui.agent.netease;

import com.myiui.agent.AgentLog;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.LinkedHashMap;
import java.util.Map;

/**
 * 网易云登录态管理：cookie 持久化到 %LOCALAPPDATA%\MyiUI\netease\cookie.txt。
 * cookie 明文存储（与 api-enhanced 一致），文档需提示风险。
 */
public final class LoginManager {
    private static final Object LOCK = new Object();
    private static volatile Map<String, String> cookies = new LinkedHashMap<>();
    private static volatile long userId = 0;
    private static volatile String nickname = "";
    private static volatile String avatarUrl = "";
    private static volatile boolean loaded = false;

    private LoginManager() {}

    public static void load() {
        synchronized (LOCK) {
            if (loaded) return;
            loaded = true;
            try {
                Path file = NetEaseConfig.dataDir().resolve("cookie.txt");
                if (!Files.isRegularFile(file)) {
                    AgentLog.info("NetEase: no saved cookie");
                    return;
                }
                String raw = Files.readString(file, StandardCharsets.UTF_8).trim();
                if (raw.isEmpty()) return;
                // 格式：MUSIC_U=xxx; __csrf=yyy; ... 或每行一个 name=value
                cookies = parseCookieString(raw);
                AgentLog.info("NetEase: loaded cookie with " + cookies.size() + " fields");
            } catch (Throwable t) {
                AgentLog.error("NetEase cookie load failed", t);
            }
        }
    }

    public static void saveCookie(String cookieString) {
        synchronized (LOCK) {
            cookies = parseCookieString(cookieString);
            try {
                Files.createDirectories(NetEaseConfig.dataDir());
                Files.writeString(NetEaseConfig.dataDir().resolve("cookie.txt"), cookieString,
                        StandardCharsets.UTF_8);
                AgentLog.info("NetEase: cookie saved (" + cookies.size() + " fields)");
            } catch (IOException e) {
                AgentLog.error("NetEase cookie save failed", e);
            }
        }
    }

    public static void saveCookieFromHeaders(java.net.HttpURLConnection conn) {
        // 从 Set-Cookie 头累积 cookie，合并进现有 cookie
        synchronized (LOCK) {
            Map<String, String> merged = new LinkedHashMap<>(cookies);
            boolean changed = false;
            for (Map.Entry<String, java.util.List<String>> e : conn.getHeaderFields().entrySet()) {
                if (e.getKey() == null) continue;
                if (!e.getKey().equalsIgnoreCase("Set-Cookie")) continue;
                for (String sc : e.getValue()) {
                    int semi = sc.indexOf(';');
                    String pair = semi > 0 ? sc.substring(0, semi) : sc;
                    int eq = pair.indexOf('=');
                    if (eq > 0) {
                        String name = pair.substring(0, eq).trim();
                        String value = pair.substring(eq + 1).trim();
                        String prev = merged.put(name, value);
                        if (!value.equals(prev)) changed = true;
                    }
                }
            }
            cookies = merged;
            // 仅在 cookie 实际变化时写盘，避免轮询请求频繁 I/O
            if (changed) {
                try {
                    Files.createDirectories(NetEaseConfig.dataDir());
                    Files.writeString(NetEaseConfig.dataDir().resolve("cookie.txt"), toCookieString(cookies),
                            StandardCharsets.UTF_8);
                } catch (IOException ignored) {}
            }
        }
    }

    public static String cookieHeader() {
        synchronized (LOCK) {
            return toCookieString(cookies);
        }
    }

    public static boolean isLoggedIn() {
        synchronized (LOCK) {
            return cookies.containsKey("MUSIC_U");
        }
    }

    public static void setProfile(long uid, String name, String avatar) {
        synchronized (LOCK) {
            userId = uid;
            nickname = name == null ? "" : name;
            avatarUrl = avatar == null ? "" : avatar;
        }
    }

    public static long userId() { return userId; }
    public static String nickname() { return nickname; }
    public static String avatarUrl() { return avatarUrl; }

    public static void logout() {
        synchronized (LOCK) {
            cookies.clear();
            userId = 0;
            nickname = "";
            avatarUrl = "";
            try {
                Files.deleteIfExists(NetEaseConfig.dataDir().resolve("cookie.txt"));
            } catch (IOException ignored) {}
            AgentLog.info("NetEase: logged out");
        }
    }

    private static Map<String, String> parseCookieString(String raw) {
        Map<String, String> map = new LinkedHashMap<>();
        if (raw == null || raw.isEmpty()) return map;
        // 同时支持 "a=1; b=2" 和 "a=1\nb=2"
        String[] parts = raw.contains("\n") ? raw.split("\\R") : raw.split(";");
        for (String p : parts) {
            int eq = p.indexOf('=');
            if (eq > 0) {
                map.put(p.substring(0, eq).trim(), p.substring(eq + 1).trim());
            }
        }
        return map;
    }

    private static String toCookieString(Map<String, String> map) {
        StringBuilder sb = new StringBuilder();
        boolean first = true;
        for (Map.Entry<String, String> e : map.entrySet()) {
            if (!first) sb.append("; ");
            sb.append(e.getKey()).append('=').append(e.getValue());
            first = false;
        }
        return sb.toString();
    }
}
