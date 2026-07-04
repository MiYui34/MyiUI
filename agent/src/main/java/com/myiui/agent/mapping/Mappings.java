package com.myiui.agent.mapping;

/**
 * Central registry of version-sensitive Minecraft names (single source of truth).
 *
 * <p>Every entry is an ordered candidate list: yarn (named) first, then Fabric intermediary,
 * then any known cross-version variants. Fabric intermediary names are stable across versions,
 * so most members need only two candidates; members whose descriptor or numbering changed
 * between 1.21.x releases carry extra candidates.
 *
 * <p>Reflective lookups feed these arrays to {@code ReflectUtil}'s N-candidate resolvers.
 * ASM transformers (which match on raw bytecode names, not reflection) use the {@code *_METHOD}
 * name sets and the {@code *_CLASS_IDS} class-id sets via {@code matchesAny}/{@code isClass}.
 */
public final class Mappings {
    private Mappings() {}

    // ---- Internal names ('/'-form) for class-level matching (ASM + instance classification) ----

    public static final String[] TITLE_SCREEN_CLASS = {
            "net/minecraft/client/gui/screen/TitleScreen", "net/minecraft/class_442"};
    public static final String[] SCREEN_CLASS = {
            "net/minecraft/client/gui/screen/Screen", "net/minecraft/class_437"};
    public static final String[] MINECRAFT_CLIENT_CLASS = {
            "net/minecraft/client/MinecraftClient", "net/minecraft/class_310"};
    public static final String[] IN_GAME_HUD_CLASS = {
            "net/minecraft/client/gui/hud/InGameHud", "net/minecraft/class_329"};
    public static final String[] PLAYER_LIST_HUD_CLASS = {
            "net/minecraft/client/gui/hud/PlayerListHud", "net/minecraft/class_355"};

    /** DrawContext type markers used inside method descriptors (yarn + intermediary). */
    public static final String[] DRAW_CONTEXT_DESC_MARKERS = {
            "Lnet/minecraft/client/gui/DrawContext;", "Lnet/minecraft/class_332;"};

    /** RenderTickCounter type markers used inside InGameHud.render descriptors. */
    public static final String[] RENDER_TICK_COUNTER_MARKERS = {
            "RenderTickCounter", "class_9779"};

    // ---- ASM method-name candidate sets (bytecode-level, no descriptor) ----

    // MinecraftClient (class_310)
    public static final String[] MC_SET_SCREEN_METHOD = {"setScreen", "method_1507"};
    public static final String[] MC_TICK_METHOD = {"tick", "method_1574"};
    public static final String[] MC_RENDER_METHOD = {"render", "method_1523"};
    // disconnect gained/renamed overloads across versions.
    public static final String[] MC_DISCONNECT_METHOD = {
            "disconnect", "method_18099", "method_56134", "method_18096"};

    // TitleScreen (class_442)
    public static final String[] TITLE_INIT_METHOD = {"init", "method_25426"};
    public static final String[] TITLE_ON_DISPLAYED_METHOD = {"onDisplayed", "method_49589"};
    public static final String[] TITLE_RENDER_METHOD = {"render", "method_25394"};
    public static final String[] SCREEN_RENDER_BACKGROUND_METHOD = {"renderBackground", "method_25420"};

    // InGameHud (class_329)
    public static final String[] HUD_RENDER_METHOD = {"render", "method_1753"};
    public static final String[] HUD_RENDER_STATUS_EFFECTS_METHOD = {"renderStatusEffectOverlay", "method_1769"};
    public static final String[] HUD_RENDER_PLAYER_LIST_METHOD = {"renderPlayerList", "method_55804"};
    public static final String[] HUD_RENDER_CHAT_METHOD = {"renderChat", "method_1803"};

    // PlayerListHud (class_355)
    public static final String[] PLAYER_LIST_RENDER_METHOD = {"render", "method_1919"};

    // ---- Reflective member candidate sets (highest-variance points) ----

    // PlayerInventory selected-slot: method renamed/renumbered across 1.21.x, and field variants.
    public static final String[] INV_GET_SELECTED_SLOT = {
            "getSelectedSlot", "method_67532", "method_6751", "method_5439"};
    public static final String[] INV_SELECTED_SLOT_FIELD = {
            "selectedSlot", "field_7545", "field_7544"};

    // ClientPlayNetworkHandler tab list.
    public static final String[] NET_LISTED_ENTRIES = {
            "getListedPlayerListEntries", "getPlayerList", "getListedPlayers", "method_45732", "method_2880"};
    public static final String[] MC_GET_NETWORK_HANDLER = {
            "getNetworkHandler", "getConnection", "method_1562"};

    // HungerManager accessors + field.
    public static final String[] PLAYER_GET_HUNGER_MANAGER = {
            "getHungerManager", "getFoodData", "getFoodStats", "method_7342"};
    public static final String[] HUNGER_MANAGER_FIELD = {"hungerManager", "field_7510"};

    /** True if {@code name} (either '/'-internal or '.'-binary form) equals any candidate. */
    public static boolean isClass(String[] candidates, String name) {
        if (name == null) {
            return false;
        }
        String slashed = name.replace('.', '/');
        for (String c : candidates) {
            if (c.equals(slashed)) {
                return true;
            }
        }
        return false;
    }

    /** True if {@code value} equals any candidate (exact match). */
    public static boolean matchesAny(String[] candidates, String value) {
        if (value == null) {
            return false;
        }
        for (String c : candidates) {
            if (c.equals(value)) {
                return true;
            }
        }
        return false;
    }

    /** True if {@code desc} contains any of the given type markers. */
    public static boolean descContainsAny(String[] markers, String desc) {
        if (desc == null) {
            return false;
        }
        for (String m : markers) {
            if (desc.contains(m)) {
                return true;
            }
        }
        return false;
    }
}
