package com.myiui.agent;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Comparator;
import java.util.List;

/** Collects Tab player-list data for the in-game Dynamic Island overlay. */
public final class PlayerListBridge {
    private static final int TAB_HEADER_LEN = 48;
    private static final int TAB_PLAYER_NAME_LEN = 20;
    private static final int TAB_MAX_PLAYERS = 32;

    private static boolean lastTabVisible;
    private static int tabVisibleHold;

    private PlayerListBridge() {}

    public static void onHudRender(Object client) {
        try {
            if (client == null) {
                return;
            }
            TabSnapshot snap = buildSnapshot(client);
            if (snap.tabVisible != lastTabVisible) {
                lastTabVisible = snap.tabVisible;
                AgentLog.info("Tab list visible=" + snap.tabVisible + " players=" + snap.playerCount);
            }
            SharedState.writeTabListState(snap);
        } catch (Throwable t) {
            AgentLog.error("PlayerListBridge.onHudRender failed", t);
        }
    }

    static TabSnapshot buildSnapshot(Object client) {
        TabSnapshot snap = new TabSnapshot();
        snap.tabVisible = isTabVisible(client);
        if (!snap.tabVisible) {
            snap.playerCount = 0;
            snap.header = "";
            return snap;
        }

        Object handler = getNetworkHandler(client);
        List<TabPlayerRow> rows = new ArrayList<>();
        if (handler == null) {
            appendLocalPlayer(client, rows);
            snap.playerCount = Math.min(TAB_MAX_PLAYERS, rows.size());
            snap.header = truncate("Players (" + rows.size() + ")", TAB_HEADER_LEN);
            fillRows(snap, rows);
            return snap;
        }

        Collection<?> entries = getPlayerList(handler);
        if (entries == null || entries.isEmpty()) {
            appendLocalPlayer(client, rows);
            snap.playerCount = Math.min(TAB_MAX_PLAYERS, rows.size());
            snap.header = truncate("Players (" + rows.size() + ")", TAB_HEADER_LEN);
            fillRows(snap, rows);
            return snap;
        }

        for (Object entry : entries) {
            if (entry == null) {
                continue;
            }
            String name = readPlayerName(entry);
            if (name == null || name.isEmpty()) {
                continue;
            }
            int ping = readLatency(entry);
            rows.add(new TabPlayerRow(name, ping));
        }

        rows.sort(Comparator.comparing(row -> row.name.toLowerCase()));
        snap.playerCount = Math.min(TAB_MAX_PLAYERS, rows.size());
        snap.header = truncate("Players (" + rows.size() + ")", TAB_HEADER_LEN);
        fillRows(snap, rows);
        return snap;
    }

    private static void fillRows(TabSnapshot snap, List<TabPlayerRow> rows) {
        for (int i = 0; i < snap.playerCount; i++) {
            TabPlayerRow row = rows.get(i);
            snap.names[i] = truncate(row.name, TAB_PLAYER_NAME_LEN);
            snap.pings[i] = (short) Math.max(-1, Math.min(32767, row.ping));
        }
    }

    private static void appendLocalPlayer(Object client, List<TabPlayerRow> rows) {
        for (String[] field : new String[][]{
                {"player", "field_1724"},
                {"field_1724", "player"},
        }) {
            try {
                Object player = ReflectUtil.getField(client, field[0], field[1]);
                if (player == null) {
                    continue;
                }
                String name = readEntityName(player);
                if (name != null && !name.isEmpty()) {
                    rows.add(new TabPlayerRow(name, 0));
                }
                return;
            } catch (ReflectiveOperationException ignored) {
            }
        }
    }

    private static String readEntityName(Object entity) {
        Object name = invokeObject(entity, "getName", "method_5477");
        if (name != null) {
            Object text = invokeObject(name, "getString", "method_10851");
            if (text instanceof String s && !s.isEmpty()) {
                return s;
            }
        }
        Object profile = invokeObject(entity, "getGameProfile", "method_7334");
        if (profile != null) {
            Object profileName = invokeObject(profile, "getName", "method_1676");
            if (profileName instanceof String s && !s.isEmpty()) {
                return s;
            }
        }
        return null;
    }

    private static boolean isTabVisible(Object client) {
        boolean raw = isTabVisibleRaw(client);
        if (raw) {
            tabVisibleHold = Math.min(tabVisibleHold + 2, 8);
        } else {
            tabVisibleHold = Math.max(tabVisibleHold - 1, 0);
        }
        return tabVisibleHold > 0;
    }

    private static boolean isTabVisibleRaw(Object client) {
        Object key = getPlayerListKey(client);
        if (key != null) {
            if (isKeyBindingHeld(key)) {
                return true;
            }
            return isBoundKeyHeld(client, key);
        }
        return isDefaultTabKeyHeld(client);
    }

    private static boolean isKeyBindingHeld(Object key) {
        for (String[] method : new String[][]{
                {"isPressed", "method_1434"},
        }) {
            try {
                Method m = ReflectUtil.findInstanceMethod(key.getClass(), method[0], method[1]);
                Object result = m.invoke(key);
                if (result instanceof Boolean b && b) {
                    return true;
                }
            } catch (ReflectiveOperationException ignored) {
            }
        }
        return false;
    }

    private static boolean isBoundKeyHeld(Object client, Object keyBinding) {
        try {
            Object boundKey = ReflectUtil.getField(keyBinding, "boundKey", "field_1655");
            if (boundKey == null) {
                return false;
            }
            Object codeObj = invokeObject(boundKey, "getCode", "method_1444");
            if (!(codeObj instanceof Number codeNum)) {
                return false;
            }
            int code = codeNum.intValue();
            Object window = ReflectUtil.getField(client, "window", "field_1704");
            if (window == null) {
                return false;
            }
            Method getHandle = ReflectUtil.findInstanceMethod(window.getClass(), "getHandle", "method_4490");
            Object handleObj = getHandle.invoke(window);
            if (!(handleObj instanceof Number handleNum)) {
                return false;
            }
            long handle = handleNum.longValue();
            Class<?> inputUtil = GameActions.findClassForBridge(
                    "net.minecraft.client.util.InputUtil", "net.minecraft.class_3675");
            if (inputUtil == null) {
                return false;
            }
            Method isKeyPressed = ReflectUtil.findStaticMethod(
                    inputUtil, "isKeyPressed", "method_15987", long.class, int.class);
            Object pressed = isKeyPressed.invoke(null, handle, code);
            return pressed instanceof Boolean b && b;
        } catch (ReflectiveOperationException ignored) {
            return false;
        }
    }

    private static boolean isDefaultTabKeyHeld(Object client) {
        try {
            Object window = ReflectUtil.getField(client, "window", "field_1704");
            if (window == null) {
                return false;
            }
            Method getHandle = ReflectUtil.findInstanceMethod(window.getClass(), "getHandle", "method_4490");
            Object handleObj = getHandle.invoke(window);
            if (!(handleObj instanceof Number handleNum)) {
                return false;
            }
            Class<?> inputUtil = GameActions.findClassForBridge(
                    "net.minecraft.client.util.InputUtil", "net.minecraft.class_3675");
            if (inputUtil == null) {
                return false;
            }
            Method isKeyPressed = ReflectUtil.findStaticMethod(
                    inputUtil, "isKeyPressed", "method_15987", long.class, int.class);
            Object pressed = isKeyPressed.invoke(null, handleNum.longValue(), 258);
            return pressed instanceof Boolean b && b;
        } catch (ReflectiveOperationException ignored) {
            return false;
        }
    }

    private static Object getPlayerListKey(Object client) {
        Object options = getOptions(client);
        if (options == null) {
            return null;
        }
        for (String[] field : new String[][]{
                {"playerListKey", "field_1907"},
                {"keyPlayerList", "field_1907"},
                {"field_1907", "playerListKey"},
                {"playerListKey", "field_1837"},
                {"field_1837", "playerListKey"},
        }) {
            try {
                Object key = ReflectUtil.getField(options, field[0], field[1]);
                if (key != null && isKeyBinding(key)) {
                    return key;
                }
            } catch (ReflectiveOperationException ignored) {
            }
        }
        return scanKeyBinding(options, "player", "list", "tab");
    }

    private static boolean isKeyBinding(Object value) {
        if (value == null) {
            return false;
        }
        String typeName = value.getClass().getName();
        return typeName.contains("KeyBinding") || typeName.contains("class_304");
    }

    private static Object scanKeyBinding(Object holder, String... hints) {
        if (holder == null) {
            return null;
        }
        for (Field field : holder.getClass().getDeclaredFields()) {
            try {
                field.setAccessible(true);
                Object value = field.get(holder);
                if (value == null) {
                    continue;
                }
                String typeName = value.getClass().getName();
                if (!typeName.contains("KeyBinding") && !typeName.contains("class_304")) {
                    continue;
                }
                String fieldName = field.getName().toLowerCase();
                for (String hint : hints) {
                    if (fieldName.contains(hint)) {
                        return value;
                    }
                }
            } catch (ReflectiveOperationException ignored) {
            }
        }
        return null;
    }

    private static Object getOptions(Object client) {
        for (String[] field : new String[][]{
                {"options", "field_1690"},
                {"field_1690", "options"},
        }) {
            try {
                Object options = ReflectUtil.getField(client, field[0], field[1]);
                if (options != null) {
                    return options;
                }
            } catch (ReflectiveOperationException ignored) {
            }
        }
        return null;
    }

    private static Object getNetworkHandler(Object client) {
        for (String[] method : new String[][]{
                {"getNetworkHandler", "method_1562"},
                {"getConnection", "method_1562"},
        }) {
            try {
                Method m = ReflectUtil.findInstanceMethod(client.getClass(), method[0], method[1]);
                Object handler = m.invoke(client);
                if (handler != null) {
                    return handler;
                }
            } catch (ReflectiveOperationException ignored) {
            }
        }
        return null;
    }

    @SuppressWarnings("unchecked")
    private static Collection<?> getPlayerList(Object handler) {
        for (String[] method : new String[][]{
                {"getListedPlayerListEntries", "method_45732"},
                {"getPlayerList", "method_2880"},
                {"getListedPlayers", "method_45732"},
        }) {
            try {
                Method m = ReflectUtil.findInstanceMethod(handler.getClass(), method[0], method[1]);
                Object result = m.invoke(handler);
                if (result instanceof Collection<?> collection) {
                    return collection;
                }
            } catch (ReflectiveOperationException ignored) {
            }
        }
        for (Field field : handler.getClass().getDeclaredFields()) {
            try {
                field.setAccessible(true);
                Object value = field.get(handler);
                if (value instanceof Collection<?> collection && !collection.isEmpty()) {
                    Object first = collection.iterator().next();
                    if (first != null) {
                        String type = first.getClass().getName();
                        if (type.contains("PlayerListEntry") || type.contains("class_640")) {
                            return collection;
                        }
                    }
                }
            } catch (ReflectiveOperationException ignored) {
            }
        }
        return null;
    }

    private static String readPlayerName(Object entry) {
        Object profile = invokeObject(entry, "getProfile", "method_2966");
        if (profile == null) {
            profile = invokeObject(entry, "getGameProfile", "method_2966");
        }
        if (profile != null) {
            Object name = invokeObject(profile, "getName", "method_1676");
            if (name instanceof String s && !s.isEmpty()) {
                return s;
            }
        }
        Object display = invokeObject(entry, "getDisplayName", "method_2971");
        if (display != null) {
            String text = String.valueOf(display);
            if (!text.isEmpty() && !text.contains("@")) {
                return text;
            }
        }
        return null;
    }

    private static int readLatency(Object entry) {
        Object value = invokeObject(entry, "getLatency", "method_2959");
        if (value instanceof Number n) {
            return n.intValue();
        }
        return -1;
    }

    private static Object invokeObject(Object target, String named, String intermediary) {
        try {
            return ReflectUtil.findInstanceMethod(target.getClass(), named, intermediary).invoke(target);
        } catch (Throwable ignored) {
            return null;
        }
    }

    private static String truncate(String s, int maxLen) {
        if (s == null) {
            return "";
        }
        return s.length() <= maxLen ? s : s.substring(0, maxLen);
    }

    static final class TabSnapshot {
        boolean tabVisible;
        int playerCount;
        String header = "";
        final String[] names = new String[TAB_MAX_PLAYERS];
        final short[] pings = new short[TAB_MAX_PLAYERS];
    }

    private static final class TabPlayerRow {
        final String name;
        final int ping;

        TabPlayerRow(String name, int ping) {
            this.name = name;
            this.ping = ping;
        }
    }
}
