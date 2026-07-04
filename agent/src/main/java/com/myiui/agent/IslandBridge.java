package com.myiui.agent;

/** Writes Inspiration Island state to SHM each client tick while in-world. */
public final class IslandBridge {
    private static int islandSeq = 0;

    private IslandBridge() {}

    public static void onClientTick(Object client) {
        try {
            if (client == null) {
                return;
            }
            if (ReflectUtil.getWorld(client) == null) {
                return;
            }
            SharedState.broadcastInGameIfNeeded();
            IslandManager.removeExpired();
            int fps = readFps(client);
            SharedState.writeIslandState(IslandManager.buildSnapshot(fps));
            MusicHudBridge.onClientTick();
            pushHudIfWorldVisible(client);
        } catch (Throwable t) {
            AgentLog.error("IslandBridge.onClientTick failed", t);
        }
    }

    private static int readFps(Object client) {
        for (String[] field : new String[][]{
                {"currentFps", "field_1738"},
                {"field_1738", "currentFps"},
        }) {
            try {
                Object v = ReflectUtil.getField(client, field[0], field[1]);
                if (v instanceof Number n) {
                    return n.intValue();
                }
            } catch (ReflectiveOperationException ignored) {
            }
        }
        return 0;
    }

    private static void pushHudIfWorldVisible(Object client) {
        try {
            if (ReflectUtil.getCurrentScreen(client) != null) {
                return;
            }
            Object player = ReflectUtil.getField(client, "player", "field_1724");
            if (player == null) {
                player = ReflectUtil.getField(client, "field_1724", "player");
            }
            if (player == null) {
                return;
            }
            SharedState.writeHudState(client, player, HudBridge.snapshotFlags(client, player));
        } catch (Throwable ignored) {
        }
    }
}
