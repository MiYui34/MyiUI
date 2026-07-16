package com.myiui.bridge;

import com.google.gson.JsonObject;
import com.myiui.MyiuiClient;
import com.myiui.ws.OverlaySocketServer;
import com.myiui.ws.Protocol;
import net.minecraft.client.Minecraft;
import net.minecraft.client.player.LocalPlayer;

/**
 * Samples player HUD vitals each tick and pushes dirty deltas to the overlay.
 */
public final class HudBridge {
    private static float lastHealth = Float.NaN;
    private static float lastMaxHealth = Float.NaN;
    private static float lastAbsorption = Float.NaN;
    private static float lastFood = Float.NaN;
    private static float lastSaturation = Float.NaN;
    private static int lastArmor = Integer.MIN_VALUE;
    private static int lastAir = Integer.MIN_VALUE;
    private static int lastSelected = Integer.MIN_VALUE;
    private static int lastXpLevel = Integer.MIN_VALUE;
    private static float lastXpProgress = Float.NaN;
    private static boolean lastLowHealth;
    private static boolean lastDamaged;
    private static int damageFlashTicks;

    private HudBridge() {}

    public static void tick(Minecraft client) {
        LocalPlayer player = client.player;
        if (player == null) {
            return;
        }

        float health = player.getHealth();
        float maxHealth = player.getMaxHealth();
        float absorption = player.getAbsorptionAmount();
        float food = player.getFoodData().getFoodLevel();
        float saturation = player.getFoodData().getSaturationLevel();
        int armor = player.getArmorValue();
        int air = player.getAirSupply();
        int maxAir = player.getMaxAirSupply();
        int selected = selectedSlot(player);
        int xpLevel = player.experienceLevel;
        float xpProgress = player.experienceProgress;

        if (health < lastHealth) {
            damageFlashTicks = 8;
        }
        if (damageFlashTicks > 0) {
            damageFlashTicks--;
        }
        boolean damaged = damageFlashTicks > 0;
        boolean lowHealth = health <= 6.0f;

        boolean dirty = !eq(health, lastHealth)
                || !eq(maxHealth, lastMaxHealth)
                || !eq(absorption, lastAbsorption)
                || !eq(food, lastFood)
                || !eq(saturation, lastSaturation)
                || armor != lastArmor
                || air != lastAir
                || selected != lastSelected
                || xpLevel != lastXpLevel
                || !eq(xpProgress, lastXpProgress)
                || lowHealth != lastLowHealth
                || damaged != lastDamaged;

        if (!dirty) {
            return;
        }

        lastHealth = health;
        lastMaxHealth = maxHealth;
        lastAbsorption = absorption;
        lastFood = food;
        lastSaturation = saturation;
        lastArmor = armor;
        lastAir = air;
        lastSelected = selected;
        lastXpLevel = xpLevel;
        lastXpProgress = xpProgress;
        lastLowHealth = lowHealth;
        lastDamaged = damaged;

        float healthPct = maxHealth <= 0 ? 0 : Math.min(1f, health / maxHealth);
        float absorptionPct = maxHealth <= 0 ? 0 : Math.min(1f, absorption / maxHealth);
        float hungerPct = Math.min(1f, food / 20f);
        float saturationPct = Math.min(1f, saturation / 20f);

        JsonObject data = Protocol.obj()
                .add("health", health)
                .add("healthMax", maxHealth)
                .add("healthPct", healthPct)
                .add("absorption", absorption)
                .add("absorptionPct", absorptionPct)
                .add("food", food)
                .add("hungerPct", hungerPct)
                .add("saturation", saturation)
                .add("saturationPct", saturationPct)
                .add("armor", armor)
                .add("air", air)
                .add("airMax", maxAir)
                .add("selectedSlot", selected)
                .add("xpLevel", xpLevel)
                .add("xpProgress", xpProgress)
                .add("lowHealth", lowHealth)
                .add("damaged", damaged)
                .add("creative", player.isCreative())
                .build();

        OverlaySocketServer server = MyiuiClient.getSocketServer();
        if (server != null) {
            server.enqueue(Protocol.push("hud", data));
        }
    }

    public static void reset() {
        lastHealth = Float.NaN;
        lastMaxHealth = Float.NaN;
        lastAbsorption = Float.NaN;
        lastFood = Float.NaN;
        lastSaturation = Float.NaN;
        lastArmor = Integer.MIN_VALUE;
        lastAir = Integer.MIN_VALUE;
        lastSelected = Integer.MIN_VALUE;
        lastXpLevel = Integer.MIN_VALUE;
        lastXpProgress = Float.NaN;
    }

    private static boolean eq(float a, float b) {
        if (Float.isNaN(a) && Float.isNaN(b)) {
            return true;
        }
        return Math.abs(a - b) < 0.001f;
    }

    /** Inventory.selected is private on Mojang mappings — prefer getSelectedSlot(), else reflect. */
    private static int selectedSlot(LocalPlayer player) {
        var inv = player.getInventory();
        try {
            return (int) inv.getClass().getMethod("getSelectedSlot").invoke(inv);
        } catch (ReflectiveOperationException ignored) {
            try {
                var field = inv.getClass().getDeclaredField("selected");
                field.setAccessible(true);
                return field.getInt(inv);
            } catch (ReflectiveOperationException e) {
                return 0;
            }
        }
    }
}
