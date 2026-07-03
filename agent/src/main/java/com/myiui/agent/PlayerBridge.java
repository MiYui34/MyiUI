package com.myiui.agent;

import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;

import java.io.InputStream;
import java.lang.reflect.Method;
import java.net.HttpURLConnection;
import java.net.URI;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.StandardCopyOption;
import java.util.UUID;
import java.util.stream.Stream;

public final class PlayerBridge {
    private PlayerBridge() {}

    public static String getPlayerJson() {
        try {
            GameActions.ensureReady();
            Object client = GameActions.resolveClientForBridge();
            if (client != null) {
                String live = readFromClient(client);
                if (live != null) {
                    return live;
                }
            }
        } catch (Throwable e) {
            AgentLog.error("GET_PLAYER live read failed", e);
        }
        return fallbackFromDisk();
    }

    private static String readFromClient(Object client) {
        try {
            Object session = readSession(client);
            if (session == null) {
                return null;
            }

            String name = invokeString(session, "getUsername", "method_1676");
            if (name == null || name.isEmpty()) {
                return null;
            }

            UUID uuid = invokeUuid(session, "getUuidOrNull", "method_44717");
            if (uuid == null) {
                uuid = offlineUuid(name);
            }

            String accountType = resolveAccountType(session);
            String skinUrl = buildSkinUrl(name, uuid, accountType);
            String skinPath = resolveSkinPath(uuid, name, accountType, skinUrl);
            String mcVersion = normalizeMcVersion(readMcVersion(client));
            String loader = readFabricLoaderVersion();

            return buildJson(name, uuid, accountType, skinUrl, skinPath, mcVersion, loader);
        } catch (Throwable e) {
            AgentLog.error("readFromClient failed", e);
            return null;
        }
    }

    private static Object readSession(Object client) {
        for (String[] method : new String[][]{
                {"getSession", "method_1548"},
                {"method_1548", "getSession"},
        }) {
            try {
                Method m = ReflectUtil.findInstanceMethod(client.getClass(), method[0], method[1]);
                Object session = m.invoke(client);
                if (session != null) {
                    return session;
                }
            } catch (ReflectiveOperationException ignored) {
            }
        }
        for (String[] candidate : new String[][]{
                {"session", "field_1724"},
                {"field_1724", "session"},
        }) {
            try {
                Object session = ReflectUtil.getField(client, candidate[0], candidate[1]);
                if (session != null) {
                    return session;
                }
            } catch (ReflectiveOperationException ignored) {
            }
        }
        return null;
    }

    private static String resolveAccountType(Object session) {
        try {
            Object accountType = invokeObject(session, "getAccountType", "method_35718");
            if (accountType != null) {
                String raw = accountType.toString().toLowerCase();
                if (raw.contains("msa") || raw.contains("microsoft") || raw.contains("mojang")) {
                    return "premium";
                }
                if (raw.contains("legacy") || raw.contains("offline")) {
                    return "offline";
                }
            }
        } catch (Throwable ignored) {
        }
        return hasAccessToken(session) ? "premium" : "offline";
    }

    private static boolean hasAccessToken(Object session) {
        try {
            String token = invokeString(session, "getAccessToken", "method_1674");
            return token != null && !token.isEmpty();
        } catch (Throwable ignored) {
            return false;
        }
    }

    private static String readMcVersion(Object client) {
        for (String[] method : new String[][]{
                {"getGameVersion", "method_1515"},
                {"method_1515", "getGameVersion"},
        }) {
            try {
                Method m = ReflectUtil.findInstanceMethod(client.getClass(), method[0], method[1]);
                Object value = m.invoke(client);
                if (value instanceof String s && !s.isEmpty()) {
                    return s;
                }
            } catch (ReflectiveOperationException ignored) {
            }
        }
        return readVersionFromDisk();
    }

    private static String readFabricLoaderVersion() {
        try {
            Class<?> loaderClass = GameActions.findClassForBridge(
                    "net.fabricmc.loader.impl.FabricLoaderImpl",
                    "net.fabricmc.loader.impl.FabricLoaderImpl");
            Method getInstance = ReflectUtil.findStaticMethod(loaderClass, "getInstance", "getInstance");
            Object loader = getInstance.invoke(null);
            Method getAllMods = ReflectUtil.findInstanceMethod(loader.getClass(), "getAllMods", "getAllMods");
            Object mods = getAllMods.invoke(loader);
            if (mods instanceof Iterable<?> iterable) {
                for (Object mod : iterable) {
                    Object metadata = mod.getClass().getMethod("getMetadata").invoke(mod);
                    String id = String.valueOf(metadata.getClass().getMethod("getId").invoke(metadata));
                    if ("fabricloader".equals(id)) {
                        Object version = metadata.getClass().getMethod("getVersion").invoke(metadata);
                        String ver = String.valueOf(version.getClass().getMethod("getFriendlyString").invoke(version));
                        return "Fabric " + ver;
                    }
                }
            }
        } catch (Throwable ignored) {
        }
        return "Fabric";
    }

    private static String buildSkinUrl(String name, UUID uuid, String accountType) {
        String flatUuid = uuid.toString().replace("-", "");
        if ("premium".equals(accountType)) {
            return "https://mc-heads.net/avatar/" + flatUuid + "/80";
        }
        return "https://mc-heads.net/avatar/" + urlEncode(name) + "/80";
    }

    private static String resolveSkinPath(UUID uuid, String name, String accountType, String skinUrl) {
        String local = resolveLocalSkinPath(uuid);
        if (local != null) {
            return local;
        }
        return downloadSkinToCache(uuid, name, accountType, skinUrl);
    }

    private static String downloadSkinToCache(UUID uuid, String name, String accountType, String skinUrl) {
        try {
            Path cacheDir = resolveSkinCacheDir();
            if (cacheDir == null) {
                return null;
            }
            Files.createDirectories(cacheDir);
            String flatUuid = uuid.toString().replace("-", "");
            Path cacheFile = cacheDir.resolve(flatUuid + ".png");
            if (Files.isRegularFile(cacheFile) && Files.size(cacheFile) > 64) {
                return cacheFile.toAbsolutePath().toString();
            }

            String[] urls = buildSkinUrlCandidates(name, uuid, accountType, skinUrl);
            for (String url : urls) {
                if (url == null || url.isEmpty()) {
                    continue;
                }
                if (downloadToFile(url, cacheFile)) {
                    AgentLog.info("Cached player skin: " + cacheFile);
                    return cacheFile.toAbsolutePath().toString();
                }
            }
            Files.deleteIfExists(cacheFile);
        } catch (Throwable e) {
            AgentLog.error("downloadSkinToCache failed", e);
        }
        return null;
    }

    private static String[] buildSkinUrlCandidates(String name, UUID uuid, String accountType, String primaryUrl) {
        String flatUuid = uuid.toString().replace("-", "");
        String encodedName = urlEncode(name);
        if ("premium".equals(accountType)) {
            return new String[]{
                    primaryUrl,
                    "https://mc-heads.net/avatar/" + flatUuid + "/80",
                    "https://crafatar.com/avatars/" + flatUuid + "?size=80&overlay=true",
                    "https://minotar.net/avatar/" + flatUuid + "/80",
            };
        }
        return new String[]{
                primaryUrl,
                "https://mc-heads.net/avatar/" + encodedName + "/80",
                "https://minotar.net/avatar/" + encodedName + "/80",
        };
    }

    private static Path resolveSkinCacheDir() {
        String localAppData = System.getenv("LOCALAPPDATA");
        if (localAppData == null || localAppData.isEmpty()) {
            return null;
        }
        return Paths.get(localAppData, "MyiUI", "skin-cache");
    }

    private static boolean downloadToFile(String url, Path target) {
        try {
            HttpURLConnection connection = (HttpURLConnection) URI.create(url).toURL().openConnection();
            connection.setConnectTimeout(8000);
            connection.setReadTimeout(8000);
            connection.setRequestProperty("User-Agent", "MyiUI/1.0");
            connection.setInstanceFollowRedirects(true);
            int code = connection.getResponseCode();
            if (code < 200 || code >= 300) {
                connection.disconnect();
                return false;
            }
            try (InputStream in = connection.getInputStream()) {
                Files.copy(in, target, StandardCopyOption.REPLACE_EXISTING);
            } finally {
                connection.disconnect();
            }
            return Files.isRegularFile(target) && Files.size(target) > 32;
        } catch (Throwable e) {
            AgentLog.error("downloadToFile failed: " + url, e);
            try {
                Files.deleteIfExists(target);
            } catch (Throwable ignored) {
            }
            return false;
        }
    }

    private static String resolveLocalSkinPath(UUID uuid) {
        try {
            Path minecraft = resolveMinecraftDir();
            if (minecraft == null) {
                return null;
            }
            String flatUuid = uuid.toString().replace("-", "");
            Path customSkin = minecraft.resolve("skins").resolve(flatUuid + ".png");
            if (Files.isRegularFile(customSkin)) {
                return customSkin.toAbsolutePath().toString();
            }
            Path assetsSkins = minecraft.resolve("assets").resolve("skins");
            if (Files.isDirectory(assetsSkins)) {
                try (var stream = Files.list(assetsSkins)) {
                    Path match = stream.filter(Files::isRegularFile)
                            .filter(path -> path.getFileName().toString().startsWith(flatUuid.substring(0, 2)))
                            .filter(path -> path.getFileName().toString().contains(flatUuid))
                            .findFirst()
                            .orElse(null);
                    if (match != null) {
                        return match.toAbsolutePath().toString();
                    }
                }
            }
        } catch (Throwable ignored) {
        }
        return null;
    }

    private static String normalizeMcVersion(String version) {
        if (version == null || version.isEmpty()) {
            return "Unknown";
        }
        int fabricIdx = version.indexOf("-Fabric");
        if (fabricIdx > 0) {
            return version.substring(0, fabricIdx);
        }
        int dashIdx = version.indexOf('-');
        if (dashIdx > 0 && Character.isDigit(version.charAt(0))) {
            return version.substring(0, dashIdx);
        }
        return version;
    }

    private static String fallbackFromDisk() {
        DiskProfile disk = readDiskProfile();
        if (disk.name == null || disk.name.isEmpty()) {
            disk.name = "Player";
        }
        if (disk.uuid == null) {
            disk.uuid = offlineUuid(disk.name);
        }
        if (disk.accountType == null || disk.accountType.isEmpty()) {
            disk.accountType = "offline";
        }
        if (disk.mcVersion == null || disk.mcVersion.isEmpty()) {
            disk.mcVersion = readVersionFromDisk();
        }
        disk.mcVersion = normalizeMcVersion(disk.mcVersion);
        if (disk.mcVersion == null || disk.mcVersion.isEmpty()) {
            disk.mcVersion = "Unknown";
        }
        if (disk.loader == null || disk.loader.isEmpty()) {
            disk.loader = "Fabric";
        }
        String skinUrl = buildSkinUrl(disk.name, disk.uuid, disk.accountType);
        String skinPath = resolveSkinPath(disk.uuid, disk.name, disk.accountType, skinUrl);
        return buildJson(disk.name, disk.uuid, disk.accountType, skinUrl, skinPath, disk.mcVersion, disk.loader);
    }

    private static DiskProfile readDiskProfile() {
        DiskProfile profile = new DiskProfile();
        try {
            Path minecraft = resolveMinecraftDir();
            if (minecraft == null) {
                return profile;
            }

            Path accounts = minecraft.resolve("launcher_accounts.json");
            if (Files.isRegularFile(accounts)) {
                parseLauncherAccounts(Files.readString(accounts, StandardCharsets.UTF_8), profile);
            }

            if (profile.name == null || profile.name.isEmpty()) {
                Path legacy = minecraft.resolve("launcher_profiles.json");
                if (Files.isRegularFile(legacy)) {
                    parseLauncherProfiles(Files.readString(legacy, StandardCharsets.UTF_8), profile);
                }
            }

            if (profile.name == null || profile.name.isEmpty()) {
                parseUserCache(minecraft.resolve("usercache.json"), profile);
            }

            if (profile.mcVersion == null || profile.mcVersion.isEmpty()) {
                profile.mcVersion = readVersionFromDisk();
            }
        } catch (Throwable e) {
            AgentLog.error("readDiskProfile failed", e);
        }
        return profile;
    }

    private static void parseLauncherAccounts(String json, DiskProfile profile) {
        JsonObject root = JsonParser.parseString(json).getAsJsonObject();
        String active = root.has("activeAccountLocalId") ? root.get("activeAccountLocalId").getAsString() : null;
        JsonObject accounts = root.has("accounts") ? root.getAsJsonObject("accounts") : null;
        if (accounts == null) {
            return;
        }

        JsonObject selected = null;
        if (active != null && accounts.has(active)) {
            selected = accounts.getAsJsonObject(active);
        } else if (!accounts.entrySet().isEmpty()) {
            selected = accounts.entrySet().iterator().next().getValue().getAsJsonObject();
        }
        if (selected == null) {
            return;
        }

        if (selected.has("minecraftProfile")) {
            JsonObject mc = selected.getAsJsonObject("minecraftProfile");
            if (mc.has("name")) {
                profile.name = mc.get("name").getAsString();
            }
            if (mc.has("id")) {
                try {
                    profile.uuid = UUID.fromString(mc.get("id").getAsString().replace("-", "").length() == 32
                            ? insertUuidHyphens(mc.get("id").getAsString())
                            : mc.get("id").getAsString());
                } catch (Throwable ignored) {
                }
            }
        }

        String type = selected.has("type") ? selected.get("type").getAsString().toLowerCase() : "";
        profile.accountType = type.contains("microsoft") || type.contains("mojang") ? "premium" : "offline";
    }

    private static void parseLauncherProfiles(String json, DiskProfile profile) {
        String selectedId = extractJsonString(json, "selectedProfile");
        if (selectedId != null) {
            int profiles = json.indexOf("\"profiles\"");
            if (profiles >= 0) {
                int idPos = json.indexOf("\"" + selectedId + "\"", profiles);
                if (idPos >= 0) {
                    int nameKey = json.indexOf("\"name\"", idPos);
                    if (nameKey >= 0 && nameKey < idPos + 500) {
                        profile.name = extractJsonString(json.substring(nameKey), "name");
                    }
                }
            }
        }
        String version = extractJsonString(json, "lastVersion");
        if (version != null && !version.isEmpty()) {
            profile.mcVersion = version;
        }
        profile.accountType = "offline";
    }

    private static void parseUserCache(Path path, DiskProfile profile) {
        try {
            if (!Files.isRegularFile(path)) {
                return;
            }
            JsonElement root = JsonParser.parseString(Files.readString(path, StandardCharsets.UTF_8));
            if (!root.isJsonArray() || root.getAsJsonArray().isEmpty()) {
                return;
            }
            JsonObject first = root.getAsJsonArray().get(0).getAsJsonObject();
            if (first.has("name")) {
                profile.name = first.get("name").getAsString();
            }
            if (first.has("uuid")) {
                profile.uuid = UUID.fromString(first.get("uuid").getAsString());
            }
        } catch (Throwable ignored) {
        }
    }

    private static String readVersionFromDisk() {
        try {
            Path minecraft = resolveMinecraftDir();
            if (minecraft == null) {
                return "Unknown";
            }
            Path versions = minecraft.resolve("versions");
            if (!Files.isDirectory(versions)) {
                return "Unknown";
            }
            Path latest = null;
            try (var stream = Files.list(versions)) {
                latest = stream.filter(Files::isDirectory)
                        .filter(p -> Files.exists(p.resolve(p.getFileName().toString() + ".json")))
                        .sorted((a, b) -> Long.compare(getVersionFolderTime(b), getVersionFolderTime(a)))
                        .findFirst()
                        .orElse(null);
            }
            if (latest == null) {
                return "Unknown";
            }
            return latest.getFileName().toString();
        } catch (Throwable ignored) {
            return "Unknown";
        }
    }

    private static long getVersionFolderTime(Path versionDir) {
        try {
            return Files.getLastModifiedTime(versionDir.resolve(versionDir.getFileName().toString() + ".json")).toMillis();
        } catch (Throwable ignored) {
            return 0L;
        }
    }

    private static String buildJson(String name, UUID uuid, String accountType, String skinUrl, String skinPath,
                                    String mcVersion, String loader) {
        StringBuilder sb = new StringBuilder(420);
        sb.append("{\"name\":\"").append(escape(name)).append('"');
        sb.append(",\"uuid\":\"").append(uuid).append('"');
        sb.append(",\"account_type\":\"").append(accountType).append('"');
        sb.append(",\"skin_url\":\"").append(escape(skinUrl)).append('"');
        if (skinPath != null && !skinPath.isEmpty()) {
            sb.append(",\"skin_path\":\"").append(escape(skinPath)).append('"');
        }
        sb.append(",\"mc_version\":\"").append(escape(mcVersion)).append('"');
        sb.append(",\"loader\":\"").append(escape(loader)).append('"');
        sb.append('}');
        return sb.toString();
    }

    private static Path resolveMinecraftDir() {
        String appData = System.getenv("APPDATA");
        if (appData == null) {
            return null;
        }
        return Paths.get(appData, ".minecraft");
    }

    private static String extractJsonString(String json, String key) {
        String needle = "\"" + key + "\"";
        int pos = json.indexOf(needle);
        if (pos < 0) {
            return null;
        }
        int colon = json.indexOf(':', pos + needle.length());
        if (colon < 0) {
            return null;
        }
        int quoteStart = json.indexOf('"', colon + 1);
        if (quoteStart < 0) {
            return null;
        }
        int quoteEnd = json.indexOf('"', quoteStart + 1);
        if (quoteEnd < 0) {
            return null;
        }
        return json.substring(quoteStart + 1, quoteEnd);
    }

    private static String insertUuidHyphens(String raw) {
        if (raw.length() != 32) {
            return raw;
        }
        return raw.substring(0, 8) + "-" + raw.substring(8, 12) + "-" + raw.substring(12, 16) + "-"
                + raw.substring(16, 20) + "-" + raw.substring(20);
    }

    private static UUID offlineUuid(String username) {
        return UUID.nameUUIDFromBytes(("OfflinePlayer:" + username).getBytes(StandardCharsets.UTF_8));
    }

    private static Object invokeObject(Object target, String named, String intermediary) {
        try {
            return ReflectUtil.findInstanceMethod(target.getClass(), named, intermediary).invoke(target);
        } catch (Throwable ignored) {
            return null;
        }
    }

    private static String invokeString(Object target, String named, String intermediary) {
        Object value = invokeObject(target, named, intermediary);
        return value != null ? String.valueOf(value) : null;
    }

    private static UUID invokeUuid(Object target, String named, String intermediary) {
        Object value = invokeObject(target, named, intermediary);
        return value instanceof UUID uuid ? uuid : null;
    }

    private static String urlEncode(String value) {
        return value.replace(" ", "%20");
    }

    static String escape(String s) {
        return s.replace("\\", "\\\\").replace("\"", "\\\"");
    }

    private static final class DiskProfile {
        String name;
        UUID uuid;
        String accountType;
        String mcVersion;
        String loader;
    }
}
