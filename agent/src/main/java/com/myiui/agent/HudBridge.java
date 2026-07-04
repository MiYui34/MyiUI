package com.myiui.agent;

import com.myiui.agent.mapping.Mappings;

import java.lang.reflect.Method;

/** Collects in-game HUD stats and writes them into shared memory for the overlay DLL. */
public final class HudBridge {
    static final byte HUD_VERSION = 2;
    private static final int HOTBAR_SLOTS = 9;
    private static final byte FLAG_LOW_HEALTH = 1;
    private static final byte FLAG_DAMAGED = 2;
    private static final byte FLAG_APPLESKIN = 4;
    private static final byte FLAG_SHOW_SATURATION = 8;

    private static float lastHealth = -1f;
    private static int damageFlashTicks = 0;
    private static boolean appleSkinLoaded;
    private static float lastTickDelta = 0f;

    private HudBridge() {}

    static void refreshModDetection() {
        appleSkinLoaded = false;
        try {
            ClassLoader loader = Thread.currentThread().getContextClassLoader();
            Class<?> fabricClass = Class.forName("net.fabricmc.loader.api.FabricLoader", true, loader);
            Object fabric = fabricClass.getMethod("getInstance").invoke(null);
            Object optional = fabric.getClass().getMethod("isModLoaded", String.class).invoke(fabric, "appleskin");
            if (optional instanceof Boolean b) {
                appleSkinLoaded = b;
            }
        } catch (Throwable ignored) {
            appleSkinLoaded = false;
        }
    }

    public static void onHudRender(Object inGameHud, Object drawContext, float tickDelta) {
        try {
            Object client = resolveClient(inGameHud);
            if (client == null) {
                return;
            }
            Object world = ReflectUtil.getWorld(client);
            if (world == null) {
                return;
            }
            Object player = getPlayer(client);
            if (player == null) {
                return;
            }
            if (damageFlashTicks > 0) {
                damageFlashTicks--;
            }
            lastTickDelta = tickDelta;
            SharedState.broadcastInGameIfNeeded();
            SharedState.writeHudState(client, player, buildFlags(player));
            InfoHudBridge.onHudRender(client, player);
            PlayerListBridge.onHudRender(client);
        } catch (Throwable t) {
            AgentLog.error("HudBridge.onHudRender failed", t);
        }
    }

    static byte snapshotFlags(Object client, Object player) {
        return buildFlags(player);
    }

    private static byte buildFlags(Object player) {
        float health = readFloat(player, "getHealth", "method_6032");
        float maxHealth = Math.max(1f, readFloat(player, "getMaxHealth", "method_6063"));
        byte flags = 0;
        if (health <= 6f) {
            flags |= FLAG_LOW_HEALTH;
        }
        if (damageFlashTicks > 0) {
            flags |= FLAG_DAMAGED;
        }
        if (appleSkinLoaded) {
            flags |= FLAG_APPLESKIN;
            flags |= FLAG_SHOW_SATURATION;
        }
        if (lastHealth >= 0f && health < lastHealth - 0.01f) {
            damageFlashTicks = 8;
            flags |= FLAG_DAMAGED;
        }
        lastHealth = health;
        return flags;
    }

    static HudSnapshot snapshot(Object client, Object player) {
        HudSnapshot snap = new HudSnapshot();
        snap.health = readFloat(player, "getHealth", "method_6032");
        snap.healthMax = Math.max(1f, readFloat(player, "getMaxHealth", "method_6063"));
        snap.absorption = readFloat(player, "getAbsorptionAmount", "method_6067");

        readFoodStats(player, snap);

        snap.armor = readInt(player, "getArmor", "method_6096");
        snap.air = readInt(player, "getAir", "method_5669");
        snap.maxAir = readInt(player, "getMaxAir", "method_5670");
        snap.underwater = !readBoolean(player, "canBreathe", "method_5677");
        snap.selectedSlot = readSelectedSlot(client, player);
        snap.guiScale = readGuiScale(client);
        snap.creative = readCreative(player);
        snap.slots = readHotbarSlots(client, player);
        snap.offhand = readOffhand(client, player);
        fillHotbarLayout(client, snap);
        snap.xpLevel = readInt(player, "getExperienceLevel", "method_6115");
        if (snap.xpLevel <= 0) {
            snap.xpLevel = readInt(player, "experienceLevel", "field_7520");
        }
        snap.xpProgress = readFloat(player, "getExperienceProgress", "method_6113");
        if (snap.xpProgress <= 0f) {
            snap.xpProgress = readFloat(player, "experienceProgress", "field_7515");
        }
        snap.xpProgress = Math.max(0f, Math.min(1f, snap.xpProgress));
        return snap;
    }

    private static Object resolveClient(Object inGameHud) {
        for (String[] candidate : new String[][]{
                {"client", "field_2035"},
                {"field_2035", "client"},
        }) {
            try {
                Object client = ReflectUtil.getField(inGameHud, candidate[0], candidate[1]);
                if (client != null) {
                    return client;
                }
            } catch (ReflectiveOperationException ignored) {
            }
        }
        return GameActions.resolveClientForBridge();
    }

    private static Object getPlayer(Object client) {
        for (String[] candidate : new String[][]{
                {"player", "field_1724"},
                {"field_1724", "player"},
        }) {
            try {
                Object player = ReflectUtil.getField(client, candidate[0], candidate[1]);
                if (player != null) {
                    return player;
                }
            } catch (ReflectiveOperationException ignored) {
            }
        }
        return null;
    }

    private static Object getHungerManager(Object player) {
        for (String[] candidate : new String[][]{
                {"getHungerManager", "method_7342"},
                {"getFoodData", "method_7342"},
                {"getFoodStats", "method_7342"},
                {"hungerManager", "field_7510"},
                {"field_7510", "hungerManager"},
        }) {
            try {
                if (candidate[0].startsWith("get") || candidate[0].startsWith("method_")) {
                    Method m = ReflectUtil.findInstanceMethod(player.getClass(), candidate[0], candidate[1]);
                    Object hunger = m.invoke(player);
                    if (hunger != null) {
                        return hunger;
                    }
                } else {
                    Object hunger = ReflectUtil.getField(player, candidate[0], candidate[1]);
                    if (hunger != null) {
                        return hunger;
                    }
                }
            } catch (ReflectiveOperationException ignored) {
            }
        }
        return null;
    }

    private static void readFoodStats(Object player, HudSnapshot snap) {
        Object hunger = getHungerManager(player);
        if (hunger != null) {
            snap.food = readFoodLevel(hunger);
            snap.saturation = readFloat(hunger, "getSaturationLevel", "method_7589");
            if (snap.saturation <= 0f) {
                snap.saturation = readFloat(hunger, "getSaturation", "method_7589");
            }
            snap.exhaustion = readFloat(hunger, "getExhaustionLevel", "method_7590");
            if (snap.exhaustion <= 0f) {
                snap.exhaustion = readFloat(hunger, "getExhaustion", "method_7590");
            }
            return;
        }
        snap.food = readInt(player, "getFoodLevel", "method_7586");
        snap.saturation = readFloat(player, "getSaturationLevel", "method_7589");
        snap.exhaustion = readFloat(player, "getExhaustionLevel", "method_7590");
    }

    private static float readFoodLevel(Object hunger) {
        int level = readInt(hunger, "getFoodLevel", "method_7586");
        if (level <= 0) {
            level = readInt(hunger, "foodLevel", "field_3756");
        }
        if (level <= 0) {
            level = readInt(hunger, "field_3756", "foodLevel");
        }
        return level;
    }

    private static int readSelectedSlot(Object client, Object player) {
        Object inventory = getInventory(player);
        if (inventory != null) {
            // Most version-variable member: method_67532 (1.21.5+) vs method_6751/method_5439 (older).
            try {
                Method m = ReflectUtil.findInstanceMethod(inventory.getClass(), Mappings.INV_GET_SELECTED_SLOT);
                Object value = m.invoke(inventory);
                if (value instanceof Number n) {
                    int slot = n.intValue();
                    if (slot >= 0 && slot <= 8) {
                        return slot;
                    }
                }
            } catch (ReflectiveOperationException ignored) {
            }
            try {
                Object value = ReflectUtil.getField(inventory, Mappings.INV_SELECTED_SLOT_FIELD);
                if (value instanceof Number n) {
                    int slot = n.intValue();
                    if (slot >= 0 && slot <= 8) {
                        return slot;
                    }
                }
            } catch (ReflectiveOperationException ignored) {
            }
        }
        return 0;
    }

    private static void fillHotbarLayout(Object client, HudSnapshot snap) {
        try {
            Object window = ReflectUtil.getField(client, "window", "field_1704");
            if (window == null) {
                return;
            }
            int scaledW = readInt(window, "getScaledWidth", "method_4489");
            int scaledH = readInt(window, "getScaledHeight", "method_4502");
            int fbW = readInt(window, "getFramebufferWidth", "method_4486");
            int fbH = readInt(window, "getFramebufferHeight", "method_4507");
            if (scaledW <= 0 || scaledH <= 0 || fbW <= 0 || fbH <= 0) {
                return;
            }
            final float sx = (float) fbW / (float) scaledW;
            final float sy = (float) fbH / (float) scaledH;
            final int left = scaledW / 2 - 91;
            final int top = scaledH - 22;
            snap.hotbarLeftPx = Math.round(left * sx);
            snap.hotbarTopPx = Math.round(top * sy);
            snap.hotbarSlotPx = Math.round(20 * sx);
        } catch (Throwable ignored) {
        }
    }

    private static int readGuiScale(Object client) {
        try {
            Object options = ReflectUtil.getField(client, "options", "field_1690");
            if (options == null) {
                return 2;
            }
            Object guiScale = ReflectUtil.getField(options, "guiScale", "field_1839");
            if (guiScale == null) {
                return 2;
            }
            for (String[] name : new String[][]{{"getValue", "method_41753"}, {"getValue", "getValue"}}) {
                try {
                    Method getValue = ReflectUtil.findInstanceMethod(guiScale.getClass(), name[0], name[1]);
                    Object value = getValue.invoke(guiScale);
                    if (value instanceof Number n) {
                        return Math.max(1, Math.min(4, n.intValue()));
                    }
                } catch (ReflectiveOperationException ignored) {
                }
            }
        } catch (Throwable ignored) {
        }
        return 2;
    }

    private static HotbarSlot[] readHotbarSlots(Object client, Object player) {
        HotbarSlot[] slots = new HotbarSlot[HOTBAR_SLOTS];
        Object inventory = getInventory(player);
        if (inventory == null) {
            return slots;
        }
        for (int i = 0; i < HOTBAR_SLOTS; i++) {
            Object stack = getStackInSlot(inventory, i);
            slots[i] = describeStack(client, stack, lastTickDelta);
        }
        return slots;
    }

    private static HotbarSlot readOffhand(Object client, Object player) {
        try {
            Object inv = getInventory(player);
            if (inv == null) return new HotbarSlot();
            Object stack = invokeObject(inv, "getOffHandStack", "method_6047");
            if (stack == null) {
                stack = invokeObject(inv, "getOffHandStack", "method_5438");
            }
            return describeStack(client, stack, lastTickDelta);
        } catch (Throwable ignored) {
            return new HotbarSlot();
        }
    }

    private static boolean readCreative(Object player) {
        try {
            Object abilities = ReflectUtil.getField(player, "abilities", "field_7510");
            if (abilities == null) return false;
            Object creative = ReflectUtil.getField(abilities, "creativeMode", "field_7477");
            if (creative instanceof Boolean b) return b;
        } catch (Throwable ignored) {
        }
        return false;
    }

    private static Object getInventory(Object player) {
        for (String[] candidate : new String[][]{
                {"getInventory", "method_31548"},
                {"inventory", "field_7514"},
                {"field_7514", "inventory"},
        }) {
            try {
                if (candidate[0].startsWith("get") || candidate[0].startsWith("method_")) {
                    Method m = ReflectUtil.findInstanceMethod(player.getClass(), candidate[0], candidate[1]);
                    Object inv = m.invoke(player);
                    if (inv != null) {
                        return inv;
                    }
                } else {
                    Object inv = ReflectUtil.getField(player, candidate[0], candidate[1]);
                    if (inv != null) {
                        return inv;
                    }
                }
            } catch (ReflectiveOperationException ignored) {
            }
        }
        return null;
    }

    private static Object getStackInSlot(Object inventory, int slot) {
        for (String[] method : new String[][]{
                {"getStack", "method_5438"},
                {"getMainHandStack", "method_6047"},
        }) {
            try {
                Method m = ReflectUtil.findInstanceMethod(inventory.getClass(), method[0], method[1], int.class);
                return m.invoke(inventory, slot);
            } catch (ReflectiveOperationException ignored) {
            }
        }
        return null;
    }

    private static HotbarSlot describeStack(Object client, Object stack, float tickDelta) {
        HotbarSlot slot = new HotbarSlot();
        if (stack == null) {
            return slot;
        }
        if (readBoolean(stack, "isEmpty", "method_7960")) {
            return slot;
        }
        slot.count = (byte) Math.min(127, readInt(stack, "getCount", "method_7947"));
        Object item = invokeObject(stack, "getItem", "method_7909");
        if (item != null) {
            slot.itemId = (short) resolveRegistryItemId(item);
        }
        int maxDamage = readInt(stack, "getMaxDamage", "method_7936");
        if (maxDamage > 0) {
            int damage = readInt(stack, "getDamage", "method_7919");
            int remaining = Math.max(0, maxDamage - damage);
            slot.durabilityPct = (byte) Math.min(100, (remaining * 100) / maxDamage);
        } else {
            slot.durabilityPct = (byte) 255;
        }
        slot.cooldownPct = readCooldownPct(client, stack, tickDelta);
        return slot;
    }

    private static int resolveRegistryItemId(Object item) {
        try {
            Class<?> registries = Class.forName("net.minecraft.registry.Registries", true, item.getClass().getClassLoader());
            Object itemRegistry = ReflectUtil.getField(registries, "ITEM", "field_41178");
            if (itemRegistry == null) {
                itemRegistry = registries.getField("ITEM").get(null);
            }
            Method getRawId = itemRegistry.getClass().getMethod("getRawId", Object.class);
            Object id = getRawId.invoke(itemRegistry, item);
            if (id instanceof Number n) {
                return Math.max(0, Math.min(32767, n.intValue()));
            }
        } catch (Throwable ignored) {
        }
        return Math.max(0, item.hashCode() & 0x7FFF);
    }

    private static byte readCooldownPct(Object client, Object stack, float tickDelta) {
        try {
            Object manager = ReflectUtil.getField(client, "itemCooldownManager", "field_1755");
            if (manager == null) {
                manager = invokeObject(client, "getItemCooldownManager", "method_1568");
            }
            if (manager == null) return 0;
            Method progress = ReflectUtil.findInstanceMethod(manager.getClass(), "getCooldownProgress", "method_7905", stack.getClass(), float.class);
            Object v = progress.invoke(manager, stack, tickDelta);
            if (v instanceof Number n) {
                return (byte) Math.min(100, Math.max(0, Math.round(n.floatValue() * 100f)));
            }
        } catch (Throwable ignored) {
        }
        return 0;
    }

    private static float readFloat(Object target, String named, String intermediary) {
        Object value = invokeObject(target, named, intermediary);
        return value instanceof Number n ? n.floatValue() : 0f;
    }

    private static int readInt(Object target, String named, String intermediary) {
        Object value = invokeObject(target, named, intermediary);
        if (value instanceof Number n) {
            return n.intValue();
        }
        try {
            value = ReflectUtil.getField(target, named, intermediary);
            if (value instanceof Number n) {
                return n.intValue();
            }
        } catch (ReflectiveOperationException ignored) {
        }
        return 0;
    }

    private static boolean readBoolean(Object target, String named, String intermediary) {
        Object value = invokeObject(target, named, intermediary);
        return value instanceof Boolean b && b;
    }

    private static Object invokeObject(Object target, String named, String intermediary) {
        try {
            return ReflectUtil.findInstanceMethod(target.getClass(), named, intermediary).invoke(target);
        } catch (Throwable ignored) {
            return null;
        }
    }

    static final class HudSnapshot {
        float health;
        float healthMax;
        float absorption;
        float food;
        float saturation;
        float exhaustion;
        int armor;
        int air;
        int maxAir;
        boolean underwater;
        int selectedSlot;
        int guiScale;
        int hotbarLeftPx;
        int hotbarTopPx;
        int hotbarSlotPx;
        int xpLevel;
        float xpProgress;
        boolean creative;
        HotbarSlot[] slots = new HotbarSlot[HOTBAR_SLOTS];
        HotbarSlot offhand = new HotbarSlot();
    }

    static final class HotbarSlot {
        short itemId;
        byte count;
        byte durabilityPct;
        byte cooldownPct;
    }
}
