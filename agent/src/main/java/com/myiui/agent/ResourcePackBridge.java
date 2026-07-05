package com.myiui.agent;

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Collection;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

public final class ResourcePackBridge {
    private ResourcePackBridge() {}

    public static String getPacksJson() {
        try {
            PackContext ctx = resolvePackContext();
            if (ctx == null || ctx.allProfiles.isEmpty()) {
                return fallbackJson();
            }

            StringBuilder sb = new StringBuilder("{\"packs\":[");
            boolean first = true;
            List<Object> sortedProfiles = buildDisplayOrder(ctx);
            for (Object profile : sortedProfiles) {
                String id = readProfileId(profile);
                if (id == null || id.isEmpty()) {
                    continue;
                }
                if (!first) {
                    sb.append(',');
                }
                first = false;
                String name = stripFormatting(readProfileName(profile, id));
                boolean enabled = ctx.enabledIds.contains(id);
                boolean locked = isPackLocked(profile, id);
                sb.append("{\"id\":\"").append(escape(id)).append('"');
                sb.append(",\"name\":\"").append(escape(name)).append('"');
                sb.append(",\"enabled\":").append(enabled);
                sb.append(",\"locked\":").append(locked).append('}');
            }
            sb.append("]}");
            return sb.toString();
        } catch (Throwable e) {
            AgentLog.error("GET_RESOURCE_PACKS failed", e);
            return fallbackJson();
        }
    }

    public static boolean setPackOrder(String direction, String packRef) {
        return GameActions.runOnMainThread(client -> {
            try {
                if (packRef != null && packRef.matches("\\d+")) {
                    return setPackOrderByProfileIndex(direction, Integer.parseInt(packRef));
                }
                return setPackOrderById(direction, packRef);
            } catch (Throwable e) {
                AgentLog.error("SET_PACK_ORDER failed: " + direction + " " + packRef, e);
                return false;
            }
        });
    }

    public static boolean openResourcePacksFolder() {
        return GameActions.runOnMainThread(client -> {
            try {
                Object runDir = ReflectUtil.getField(client, "runDirectory", "field_1697");
                if (runDir == null) {
                    AgentLog.error("OPEN_RESOURCE_PACKS_FOLDER: runDirectory missing");
                    return false;
                }
                java.nio.file.Path folder = java.nio.file.Paths.get(String.valueOf(runDir), "resourcepacks");
                java.nio.file.Files.createDirectories(folder);
                if (!java.awt.Desktop.isDesktopSupported()) {
                    AgentLog.error("OPEN_RESOURCE_PACKS_FOLDER: Desktop unsupported");
                    return false;
                }
                java.awt.Desktop.getDesktop().open(folder.toFile());
                // Rescan only after user explicitly opens the folder (not on passive UI reads).
                try {
                    resolvePackContext(true);
                } catch (ReflectiveOperationException ignored) {
                }
                return true;
            } catch (Throwable e) {
                AgentLog.error("OPEN_RESOURCE_PACKS_FOLDER failed", e);
                return false;
            }
        });
    }

    public static boolean togglePack(String packRef) {
        return GameActions.runOnMainThread(client -> {
            try {
                PackContext ctx = resolvePackContext();
                if (ctx == null) {
                    return false;
                }
                Object profile = resolveProfile(ctx, packRef);
                if (profile == null) {
                    AgentLog.error("SET_PACK_TOGGLE profile not found: " + packRef);
                    return false;
                }
                String id = readProfileId(profile);
                if (id == null || id.isEmpty()) {
                    return false;
                }
                if (isPackLocked(profile, id)) {
                    AgentLog.error("SET_PACK_TOGGLE pack is locked: " + id);
                    return false;
                }

                boolean wasEnabled = ctx.enabledIds.contains(id);
                Method toggleMethod = wasEnabled
                        ? ReflectUtil.findInstanceMethod(ctx.manager.getClass(), "disable", "method_49428", String.class)
                        : ReflectUtil.findInstanceMethod(ctx.manager.getClass(), "enable", "method_49427", String.class);
                boolean changed = Boolean.TRUE.equals(toggleMethod.invoke(ctx.manager, id));
                if (!changed) {
                    AgentLog.error("SET_PACK_TOGGLE no change for " + id);
                    return false;
                }
                persistPackChanges(ctx, client, true);
                AgentLog.info("SET_PACK_TOGGLE " + id + " -> " + !wasEnabled);
                return true;
            } catch (Throwable e) {
                AgentLog.error("SET_PACK_TOGGLE failed: " + packRef, e);
                return false;
            }
        });
    }

    private static boolean setPackOrderByProfileIndex(String direction, int profileIndex) {
        try {
            PackContext ctx = resolvePackContext();
            if (ctx == null || profileIndex < 0 || profileIndex >= ctx.allProfiles.size()) {
                AgentLog.error("SET_PACK_ORDER invalid profile index: " + profileIndex);
                return false;
            }

            String packId = readProfileId(ctx.allProfiles.get(profileIndex));
            if (packId == null || !ctx.enabledIds.contains(packId)) {
                AgentLog.error("SET_PACK_ORDER pack not enabled: index=" + profileIndex + " id=" + packId);
                return false;
            }
            return reorderEnabledPack(ctx, direction, packId);
        } catch (ReflectiveOperationException e) {
            AgentLog.error("SET_PACK_ORDER failed for index " + profileIndex, e);
            return false;
        }
    }

    private static boolean setPackOrderById(String direction, String packId) {
        try {
            PackContext ctx = resolvePackContext();
            if (ctx == null || packId == null || packId.isEmpty()) {
                return false;
            }
            if (!ctx.enabledIds.contains(packId)) {
                AgentLog.error("SET_PACK_ORDER pack not enabled: " + packId);
                return false;
            }
            return reorderEnabledPack(ctx, direction, packId);
        } catch (ReflectiveOperationException e) {
            AgentLog.error("SET_PACK_ORDER failed for id " + packId, e);
            return false;
        }
    }

    private static boolean reorderEnabledPack(PackContext ctx, String direction, String packId)
            throws ReflectiveOperationException {
        if (isPackLockedById(ctx, packId)) {
            AgentLog.error("SET_PACK_ORDER pack is locked: " + packId);
            return false;
        }

        List<String> ordered = new ArrayList<>(ctx.enabledOrder);
        int index = ordered.indexOf(packId);
        if (index < 0) {
            AgentLog.error("SET_PACK_ORDER enabled list miss: " + packId);
            return false;
        }

        if ("up".equalsIgnoreCase(direction)) {
            if (index <= 0) {
                return false;
            }
            if (isPackLockedById(ctx, ordered.get(index - 1))) {
                return false;
            }
            String tmp = ordered.get(index - 1);
            ordered.set(index - 1, ordered.get(index));
            ordered.set(index, tmp);
        } else if ("down".equalsIgnoreCase(direction)) {
            if (index >= ordered.size() - 1) {
                return false;
            }
            if (isPackLockedById(ctx, ordered.get(index + 1))) {
                return false;
            }
            String tmp = ordered.get(index + 1);
            ordered.set(index + 1, ordered.get(index));
            ordered.set(index, tmp);
        } else {
            return false;
        }

        Method setEnabled = ReflectUtil.findInstanceMethod(ctx.manager.getClass(), "setEnabledProfiles", "method_14447",
                Collection.class);
        setEnabled.invoke(ctx.manager, ordered);
        persistPackChanges(ctx, GameActions.resolveClientForBridge(), false);
        AgentLog.info("SET_PACK_ORDER " + direction + " " + packId);
        return true;
    }

    private static List<Object> buildDisplayOrder(PackContext ctx) {
        List<Object> sorted = new ArrayList<>();
        Set<String> added = new LinkedHashSet<>();

        for (String id : ctx.enabledOrder) {
            for (Object profile : ctx.allProfiles) {
                String profileId = readProfileId(profile);
                if (id.equals(profileId)) {
                    sorted.add(profile);
                    added.add(id);
                    break;
                }
            }
        }

        for (Object profile : ctx.allProfiles) {
            String id = readProfileId(profile);
            if (id != null && !id.isEmpty() && !added.contains(id)) {
                sorted.add(profile);
                added.add(id);
            }
        }
        return sorted;
    }

    private static Object resolveProfile(PackContext ctx, String packRef) {
        if (packRef == null) {
            return null;
        }
        if (packRef.matches("\\d+")) {
            int index = Integer.parseInt(packRef);
            if (index < 0 || index >= ctx.allProfiles.size()) {
                return null;
            }
            return ctx.allProfiles.get(index);
        }
        for (Object profile : ctx.allProfiles) {
            String id = readProfileId(profile);
            if (packRef.equals(id)) {
                return profile;
            }
        }
        return null;
    }

    private static void persistPackChanges(PackContext ctx, Object client, boolean reloadResources)
            throws ReflectiveOperationException {
        if (ctx.options != null) {
            try {
                Method refresh = ReflectUtil.findInstanceMethod(ctx.options.getClass(), "refreshResourcePacks",
                        "method_49598", ctx.manager.getClass());
                refresh.invoke(ctx.options, ctx.manager);
            } catch (ReflectiveOperationException ignored) {
            }
            Method write = ReflectUtil.findInstanceMethod(ctx.options.getClass(), "write", "method_1640");
            write.invoke(ctx.options);
        }
        if (reloadResources && client != null) {
            try {
                Method reload = ReflectUtil.findInstanceMethod(client.getClass(), "reloadResources", "method_1521");
                reload.invoke(client);
            } catch (ReflectiveOperationException ignored) {
            }
        }
    }

    private static PackContext resolvePackContext() throws ReflectiveOperationException {
        return resolvePackContext(false);
    }

    /** @param rescan when true, rescan disk and sync from options (mutates manager; use only on explicit user actions) */
    private static PackContext resolvePackContext(boolean rescan) throws ReflectiveOperationException {
        GameActions.ensureReady();
        Object client = GameActions.resolveClientForBridge();
        if (client == null) {
            return null;
        }

        Method getManager = ReflectUtil.findInstanceMethod(client.getClass(), "getResourcePackManager",
                "method_1520");
        Object manager = getManager.invoke(client);
        if (manager == null) {
            return null;
        }

        if (rescan) {
            ReflectUtil.findInstanceMethod(manager.getClass(), "scanPacks", "method_14445").invoke(manager);
            Object options = GameActions.getGameOptions(client);
            if (options != null) {
                try {
                    Method addProfiles = ReflectUtil.findInstanceMethod(options.getClass(),
                            "addResourcePackProfilesToManager", "method_1627", manager.getClass());
                    addProfiles.invoke(options, manager);
                } catch (ReflectiveOperationException ignored) {
                }
            }
        }

        PackContext ctx = new PackContext();
        ctx.manager = manager;
        ctx.options = GameActions.getGameOptions(client);

        Collection<?> enabledProfiles = invokeProfileCollection(manager, "getEnabledProfiles", "method_14444");
        if (enabledProfiles != null) {
            for (Object profile : enabledProfiles) {
                String id = readProfileId(profile);
                if (id != null) {
                    ctx.enabledIds.add(id);
                    ctx.enabledOrder.add(id);
                }
            }
        }

        Collection<?> allProfiles = invokeProfileCollection(manager, "getProfiles", "method_14441");
        if (allProfiles != null) {
            ctx.allProfiles.addAll(allProfiles);
        }
        return ctx;
    }

    @SuppressWarnings("unchecked")
    private static Collection<?> invokeProfileCollection(Object manager, String named, String intermediary)
            throws ReflectiveOperationException {
        Method method = ReflectUtil.findInstanceMethod(manager.getClass(), named, intermediary);
        Object result = method.invoke(manager);
        return result instanceof Collection<?> collection ? collection : null;
    }

    private static String readProfileId(Object profile) {
        try {
            Method getId = ReflectUtil.findInstanceMethod(profile.getClass(), "getId", "method_14463");
            Object id = getId.invoke(profile);
            return id != null ? String.valueOf(id) : null;
        } catch (ReflectiveOperationException e) {
            return null;
        }
    }

    private static String readProfileName(Object profile, String fallback) {
        try {
            Method getDisplayName = ReflectUtil.findInstanceMethod(profile.getClass(), "getDisplayName", "method_14457");
            Object text = getDisplayName.invoke(profile);
            if (text != null) {
                for (String[] method : new String[][]{
                        {"getString", "method_10851"},
                        {"asTruncatedString", "method_27662"},
                }) {
                    try {
                        Method getString = ReflectUtil.findInstanceMethod(text.getClass(), method[0], method[1]);
                        Object value = getString.invoke(text);
                        if (value != null) {
                            String name = String.valueOf(value);
                            if (!name.isEmpty()) {
                                return name;
                            }
                        }
                    } catch (ReflectiveOperationException ignored) {
                    }
                }
                String raw = String.valueOf(text);
                if (!raw.isEmpty() && !raw.contains("@")) {
                    return raw;
                }
            }
        } catch (ReflectiveOperationException ignored) {
        }
        return fallback;
    }

    private static boolean isPackLocked(Object profile, String id) {
        if (id == null || id.isEmpty()) {
            return false;
        }
        if ("vanilla".equals(id) || "minecraft".equals(id)) {
            return true;
        }
        try {
            Method isRequired = ReflectUtil.findInstanceMethod(profile.getClass(), "isRequired", "method_14464");
            return Boolean.TRUE.equals(isRequired.invoke(profile));
        } catch (ReflectiveOperationException ignored) {
            return false;
        }
    }

    private static boolean isPackLockedById(PackContext ctx, String id) {
        if (id == null || id.isEmpty()) {
            return false;
        }
        if ("vanilla".equals(id) || "minecraft".equals(id)) {
            return true;
        }
        for (Object profile : ctx.allProfiles) {
            String profileId = readProfileId(profile);
            if (id.equals(profileId)) {
                return isPackLocked(profile, id);
            }
        }
        return false;
    }

    private static String stripFormatting(String value) {
        if (value == null || value.isEmpty()) {
            return value;
        }
        StringBuilder sb = new StringBuilder(value.length());
        for (int i = 0; i < value.length(); i++) {
            char ch = value.charAt(i);
            if (ch == '\u00A7' && i + 1 < value.length()) {
                i++;
                continue;
            }
            sb.append(ch);
        }
        return sb.toString().trim();
    }

    private static String fallbackJson() {
        return "{\"packs\":[{\"id\":\"vanilla\",\"name\":\"Default\",\"enabled\":true,\"locked\":true}]}";
    }

    private static String escape(String s) {
        return s.replace("\\", "\\\\").replace("\"", "\\\"");
    }

    private static final class PackContext {
        Object manager;
        Object options;
        final List<Object> allProfiles = new ArrayList<>();
        final List<String> enabledOrder = new ArrayList<>();
        final Set<String> enabledIds = new LinkedHashSet<>();
    }
}
