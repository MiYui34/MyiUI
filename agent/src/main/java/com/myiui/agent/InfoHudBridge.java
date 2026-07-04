package com.myiui.agent;

/** Coords / ping / speed / FPS info for overlay HUD widgets. */
public final class InfoHudBridge {
    private static double lastX = Double.NaN;
    private static double lastY = Double.NaN;
    private static double lastZ = Double.NaN;
    private static long lastSampleMs = 0L;

    private InfoHudBridge() {}

    public static void onHudRender(Object client, Object player) {
        try {
            InfoSnapshot snap = snapshot(client, player);
            SharedState.writeInfoHudState(snap);
        } catch (Throwable t) {
            AgentLog.error("InfoHudBridge.onHudRender failed", t);
        }
    }

    static InfoSnapshot snapshot(Object client, Object player) {
        InfoSnapshot snap = new InfoSnapshot();
        snap.blockX = (int) Math.floor(readDouble(player, "getX", "method_23317"));
        snap.blockY = (int) Math.floor(readDouble(player, "getY", "method_23318"));
        snap.blockZ = (int) Math.floor(readDouble(player, "getZ", "method_23321"));
        snap.yaw = readFloat(player, "getYaw", "method_36454");
        snap.pitch = readFloat(player, "getPitch", "method_36455");
        snap.direction = yawToDirection(snap.yaw);
        snap.biome = readBiome(client, player);
        snap.fps = (short) Math.max(0, Math.min(32767, readFps(client)));
        snap.pingMs = readPing(client, player);
        snap.speedBps = readSpeed(player);
        return snap;
    }

    private static float readSpeed(Object player) {
        double x = readDouble(player, "getX", "method_23317");
        double y = readDouble(player, "getY", "method_23318");
        double z = readDouble(player, "getZ", "method_23321");
        long now = System.currentTimeMillis();
        float speed = 0f;
        if (!Double.isNaN(lastX) && lastSampleMs > 0) {
            double dt = (now - lastSampleMs) / 1000.0;
            if (dt > 0.01) {
                double dx = x - lastX;
                double dy = y - lastY;
                double dz = z - lastZ;
                speed = (float) (Math.sqrt(dx * dx + dy * dy + dz * dz) / dt);
            }
        }
        lastX = x;
        lastY = y;
        lastZ = z;
        lastSampleMs = now;
        return speed;
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

    private static short readPing(Object client, Object player) {
        try {
            Object network = ReflectUtil.getField(client, "getNetworkHandler", "field_3944");
            if (network == null) {
                network = invokeObject(client, "getNetworkHandler", "method_1562");
            }
            if (network == null) return 0;
            Object uuid = invokeObject(player, "getUuid", "method_5667");
            if (uuid == null) return 0;
            Object entry = ReflectUtil.findInstanceMethod(network.getClass(), "getPlayerListEntry", "method_2874", uuid.getClass())
                    .invoke(network, uuid);
            if (entry == null) return 0;
            Object latency = invokeObject(entry, "getLatency", "method_2959");
            if (latency instanceof Number n) {
                return (short) Math.max(0, Math.min(32767, n.intValue()));
            }
        } catch (Throwable ignored) {
        }
        return 0;
    }

    private static String readBiome(Object client, Object player) {
        try {
            Object world = ReflectUtil.getWorld(client);
            if (world == null) return "";
            Object blockPos = Class.forName("net.minecraft.util.math.BlockPos", true, player.getClass().getClassLoader())
                    .getConstructor(int.class, int.class, int.class)
                    .newInstance(
                            (int) Math.floor(readDouble(player, "getX", "method_23317")),
                            (int) Math.floor(readDouble(player, "getY", "method_23318")),
                            (int) Math.floor(readDouble(player, "getZ", "method_23321")));
            Object biome = invokeObject(world, "getBiome", "method_23753", blockPos.getClass());
            if (biome == null) return "";
            Object key = invokeObject(biome, "getKey", "comp_349");
            if (key == null) key = invokeObject(biome, "getKey", "method_40230");
            if (key == null) return "";
            Object path = invokeObject(key, "getValue", "method_29177");
            if (path == null) path = key.toString();
            String s = path.toString();
            int slash = s.lastIndexOf('/');
            return slash >= 0 ? s.substring(slash + 1) : s;
        } catch (Throwable ignored) {
            return "";
        }
    }

    private static String yawToDirection(float yaw) {
        float n = ((yaw % 360f) + 360f) % 360f;
        if (n >= 315 || n < 45) return "S";
        if (n < 135) return "W";
        if (n < 225) return "N";
        return "E";
    }

    private static double readDouble(Object target, String named, String intermediary) {
        Object value = invokeObject(target, named, intermediary);
        return value instanceof Number n ? n.doubleValue() : 0.0;
    }

    private static float readFloat(Object target, String named, String intermediary) {
        Object value = invokeObject(target, named, intermediary);
        return value instanceof Number n ? n.floatValue() : 0f;
    }

    private static Object invokeObject(Object target, String named, String intermediary, Class<?>... params) {
        try {
            return ReflectUtil.findInstanceMethod(target.getClass(), named, intermediary, params).invoke(target, (Object[]) null);
        } catch (Throwable ignored) {
            try {
                return ReflectUtil.findInstanceMethod(target.getClass(), named, intermediary).invoke(target);
            } catch (Throwable ignored2) {
                return null;
            }
        }
    }

    static final class InfoSnapshot {
        int blockX;
        int blockY;
        int blockZ;
        short pingMs;
        short fps;
        float speedBps;
        float yaw;
        float pitch;
        String biome = "";
        String direction = "";
    }
}
