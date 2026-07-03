package com.myiui.agent;

public final class SharedState {
    private static final int HUD_STATE_SIZE = 108;
    private static final int CHAT_USER_LEN = 32;
    private static final int CHAT_TEXT_LEN = 192;
    private static final int CHAT_MAX_MESSAGES = 16;
    private static final int CHAT_MESSAGE_SIZE = CHAT_USER_LEN + CHAT_TEXT_LEN;
    private static final int CHAT_STATE_SIZE = 6 + CHAT_MAX_MESSAGES * CHAT_MESSAGE_SIZE;
    private static final int MAX_FRAME_BYTES = 1920 * 1080 * 4;

    private static volatile boolean menuActive;
    private static volatile boolean overlayAck;
    private static volatile byte currentScreenKind = ScreenKind.NONE;
    private static int screenSeq = 0;
    private static int hudSeq = 0;
    private static int islandSeq = 0;
    private static int chatSeq = 0;
    private static int writeBufferIndex = 0;
    private static int frameSequence = 0;

    private SharedState() {}

    public static void init() {
        HudBridge.refreshModDetection();
        AgentLog.info("SharedState v2 JNI bridge ready");
    }

    /** Authoritative UI state broadcast — called synchronously from MinecraftClient.setScreen. */
    public static synchronized void broadcastScreen(Object screen) {
        ensureMapped();
        if (screen == null) {
            Object client = GameActions.resolveClientForBridge();
            if (client != null) {
                try {
                    if (ReflectUtil.getWorld(client) != null) {
                        broadcastInGame();
                        return;
                    }
                } catch (Throwable ignored) {
                }
            }
        }
        byte kind = ScreenKind.classify(screen);
        boolean active = kind == ScreenKind.MAIN_MENU;
        if (kind == currentScreenKind && active == menuActive) {
            return;
        }
        menuActive = active;
        if (active) {
            overlayAck = false;
        }
        currentScreenKind = kind;
        screenSeq++;
        writeUiState(kind, active, screenSeq);
        if (kind != ScreenKind.IN_GAME) {
            clearHudState();
            clearIslandState();
        }
        AgentLog.info("broadcast screen seq=" + screenSeq + " kind=" + ScreenKind.label(kind)
                + (screen == null ? "" : " class=" + screen.getClass().getName()));
    }

    public static synchronized void broadcastInGame() {
        ensureMapped();
        if (currentScreenKind == ScreenKind.IN_GAME) {
            return;
        }
        menuActive = false;
        currentScreenKind = ScreenKind.IN_GAME;
        screenSeq++;
        writeUiState(ScreenKind.IN_GAME, false, screenSeq);
        clearHudState();
    }

    public static synchronized void writeIslandState(IslandManager.IslandSnapshot snap) {
        if (snap == null) {
            return;
        }
        byte mode = snap.hasNotification ? (byte) 2 : snap.mode;
        NativeBridge.pushIslandState(
                (byte) 1,
                mode,
                (byte) (snap.activeSlot & 0xFF),
                (byte) Math.min(255, snap.notifyCount),
                (short) (++islandSeq & 0xFFFF),
                (short) snap.fps,
                snap.title,
                snap.subtitle,
                snap.lyrics == null ? "" : snap.lyrics,
                BridgePacker.islandSlots(snap),
                (int) (snap.expireMs & 0xFFFFFFFFL));
    }

    private static final int ISLAND_TITLE_LEN = 48;

    private static void clearIslandState() {
        NativeBridge.pushIslandState((byte) 0, (byte) 0, (byte) 0, (byte) 0, (short) 0, (short) 0,
                "", "", "", new byte[16], 0);
    }

    public static synchronized void writeChatState(java.util.List<String[]> messages, boolean visible) {
        if (messages == null) {
            return;
        }
        NativeBridge.pushChatState(BridgePacker.packChat(messages, visible, ++chatSeq));
    }

    private static void clearChatState() {
        NativeBridge.pushChatState(new byte[CHAT_STATE_SIZE]);
    }

    public static synchronized void broadcastInGameIfNeeded() {
        if (currentScreenKind != ScreenKind.IN_GAME) {
            broadcastInGame();
        }
    }

    public static synchronized void writeHudState(Object client, Object player, byte flags) {
        HudBridge.HudSnapshot snap = HudBridge.snapshot(client, player);
        NativeBridge.pushHudState(BridgePacker.packHud(snap, flags, ++hudSeq));
    }

    private static void clearHudState() {
        NativeBridge.pushHudState(new byte[HUD_STATE_SIZE]);
    }

    public static synchronized void setMenuActive(boolean active) {
        menuActive = active;
        byte kind = active ? ScreenKind.MAIN_MENU : currentScreenKind;
        if (active) {
            kind = ScreenKind.MAIN_MENU;
            currentScreenKind = kind;
        }
        screenSeq++;
        writeUiState(kind, active, screenSeq);
    }

    public static boolean isMenuActive() {
        return menuActive;
    }

    /** TitleScreen vanilla render is skipped only after overlay confirms it can draw. */
    public static boolean shouldSkipVanillaTitleRender() {
        return menuActive && overlayAck;
    }

    public static void setOverlayAck(boolean ack) {
        overlayAck = ack;
        if (ack) {
            AgentLog.info("Overlay 已就绪 — 跳过原版主菜单绘制");
        }
    }

    public static void onTitleScreenOpened(Object screen) {
        if (!ClassUtil.isTitleScreenInstance(screen)) {
            return;
        }
        boolean alreadyActive = menuActive && currentScreenKind == ScreenKind.MAIN_MENU;
        ScreenHelper.clearScreenChildren(screen);
        if (!alreadyActive) {
            broadcastScreen(screen);
            VideoBackground.resumeIfNeeded();
            AgentLog.info("主菜单已就绪 — 启用 MyiUI 覆盖层");
        }
    }

    public static void onTitleScreenRender() {
        // TitleScreen.render only runs when MyiUI owns the menu; keep overlay active.
    }

    public static void onScreenRender(Object screen) {
        if (!ClassUtil.isTitleScreenInstance(screen)) {
            return;
        }
        if (!menuActive) {
            onTitleScreenOpened(screen);
        }
    }

    public static void syncMenuWithClient(Object client) {
        try {
            Object world = ReflectUtil.getWorld(client);
            if (world != null) {
                Object screen;
                try {
                    screen = ReflectUtil.getCurrentScreen(client);
                } catch (ReflectiveOperationException e) {
                    broadcastInGameIfNeeded();
                    return;
                }
                if (screen == null) {
                    broadcastInGameIfNeeded();
                    return;
                }
                if (menuActive || currentScreenKind == ScreenKind.MAIN_MENU) {
                    byte expected = ScreenKind.classify(screen);
                    if (expected != currentScreenKind) {
                        broadcastScreen(screen);
                    }
                }
                return;
            }

            Object screen;
            try {
                screen = ReflectUtil.getCurrentScreen(client);
            } catch (ReflectiveOperationException e) {
                return;
            }
            if (screen == null) {
                return;
            }
            if (ClassUtil.isTitleScreenInstance(screen)) {
                if (currentScreenKind != ScreenKind.MAIN_MENU || !menuActive) {
                    AgentLog.info("syncMenu: recovering main menu broadcast");
                    onTitleScreenOpened(screen);
                }
                return;
            }

            byte expected = ScreenKind.classify(screen);
            if (currentScreenKind != expected) {
                AgentLog.info("syncMenu: rebroadcast " + ScreenKind.label(expected));
                broadcastScreen(screen);
            }
        } catch (Throwable t) {
            AgentLog.error("syncMenuWithClient failed", t);
        }
    }

    public static void scheduleMenuSync(Object client) {
        if (client == null) {
            return;
        }
        syncMenuWithClient(client);
    }

    public static void scheduleMenuSyncDeferred(Object client) {
        if (client == null) {
            return;
        }
        try {
            java.lang.reflect.Method execute = ReflectUtil.findInstanceMethod(
                    client.getClass(), "execute", "method_1514", Runnable.class);
            execute.invoke(client, (Runnable) () -> syncMenuWithClient(client));
        } catch (Throwable t) {
            AgentLog.error("scheduleMenuSyncDeferred failed", t);
        }
    }

    public static void onAfterDisconnect(Object client) {
        try {
            Object screen = ReflectUtil.getCurrentScreen(client);
            if (screen != null && isTransientNetworkScreen(screen)) {
                return;
            }
        } catch (ReflectiveOperationException ignored) {
        }
        AgentLog.info("disconnect completed — syncing menu state");
        scheduleMenuSync(client);
    }

    private static boolean isTransientNetworkScreen(Object screen) {
        String name = screen.getClass().getName();
        return name.contains("ConnectScreen") || name.contains("class_412")
                || name.contains("ProgressScreen") || name.contains("class_435")
                || name.contains("DownloadingTerrainScreen") || name.contains("class_434");
    }

    public static void onScreenLifecycleEvent() {
        Object client = GameActions.resolveClientForBridge();
        if (client != null) {
            scheduleMenuSyncDeferred(client);
        }
    }

    public static void onSubScreenClosedFromScreen(Object closedScreen) {
        if (menuActive && currentScreenKind == ScreenKind.MAIN_MENU) {
            AgentLog.info("Sub-screen closed: already on main menu (skip recovery)");
            return;
        }
        Object client = GameActions.resolveClientForBridge();
        if (client == null) {
            client = ReflectUtil.getScreenClient(closedScreen);
        }
        if (client == null) {
            onScreenLifecycleEvent();
            return;
        }
        if (ReflectUtil.getWorld(client) != null) {
            AgentLog.info("Sub-screen closed: in-game, skip menu recovery");
            return;
        }
        AgentLog.info("Sub-screen closed: " + closedScreen.getClass().getSimpleName());
        final Object clientRef = client;
        try {
            java.lang.reflect.Method execute = ReflectUtil.findInstanceMethod(
                    clientRef.getClass(), "execute", "method_1514", Runnable.class);
            execute.invoke(clientRef, (Runnable) () -> finishReturnToMainMenu(clientRef));
        } catch (Throwable t) {
            AgentLog.error("onSubScreenClosed schedule failed", t);
            finishReturnToMainMenu(clientRef);
        }
    }

    private static void finishReturnToMainMenu(Object client) {
        try {
            Object current = ReflectUtil.getCurrentScreen(client);
            if (ClassUtil.isTitleScreenInstance(current)) {
                onTitleScreenOpened(current);
                return;
            }
            AgentLog.info("Return to menu: current="
                    + (current == null ? "null" : current.getClass().getName()));
            GameActions.ensureTitleScreenAndActivateMenu(client);
        } catch (Throwable t) {
            AgentLog.error("finishReturnToMainMenu failed", t);
        }
    }

    private static volatile String pendingDisconnectReason;

    public static synchronized String consumeDisconnectReason() {
        String reason = pendingDisconnectReason;
        pendingDisconnectReason = null;
        return reason;
    }

    private static void captureDisconnectReason(Object screen) {
        if (screen == null) {
            return;
        }
        String name = screen.getClass().getName();
        if (!name.contains("DisconnectedScreen") && !name.contains("class_419")) {
            return;
        }
        pendingDisconnectReason = "\u8fde\u63a5\u670d\u52a1\u5668\u5931\u8d25\uff0c\u8bf7\u68c0\u67e5\u5730\u5740\u6216\u7f51\u7edc";
        try {
            Object title = ReflectUtil.getField(screen, "reason", "field_2475");
            if (title == null) {
                title = ReflectUtil.getField(screen, "message", "field_2476");
            }
            if (title != null) {
                String text = String.valueOf(title);
                if (!text.isEmpty() && !text.contains("@")) {
                    pendingDisconnectReason = text;
                }
            }
        } catch (ReflectiveOperationException ignored) {
        }
    }

    public static void onSetScreen(Object screen) {
        broadcastScreen(screen);
        captureDisconnectReason(screen);
        if (screen == null) {
            VideoBackground.suspendForGameScreen();
            return;
        }
        if (ClassUtil.isTitleScreenInstance(screen)) {
            onTitleScreenOpened(screen);
            return;
        }
        VideoBackground.suspendForGameScreen();
    }

    public static void onSetScreenTail(Object client) {
        if (client == null) {
            return;
        }
        try {
            Object current;
            try {
                current = ReflectUtil.getCurrentScreen(client);
            } catch (ReflectiveOperationException e) {
                return;
            }
            byte expected = ScreenKind.classify(current);
            if (expected != currentScreenKind) {
                AgentLog.info("setScreen tail mismatch — rebroadcast " + ScreenKind.label(expected));
                broadcastScreen(current);
                if (expected == ScreenKind.MAIN_MENU && current != null) {
                    onTitleScreenOpened(current);
                } else if (expected != ScreenKind.MAIN_MENU) {
                    VideoBackground.suspendForGameScreen();
                }
            }
        } catch (Throwable t) {
            AgentLog.error("onSetScreenTail failed", t);
        }
    }

    public static synchronized void writeFrame(int width, int height, int stride, byte[] rgba) {
        if (rgba == null || width <= 0 || height <= 0) {
            return;
        }
        final int expectedBytes = width * height * 4;
        if (rgba.length < expectedBytes || expectedBytes > MAX_FRAME_BYTES) {
            return;
        }
        NativeBridge.pushVideoFrame(rgba, width, height, ++frameSequence);
    }

    public static synchronized void clearVideoFrame() {
        NativeBridge.pushVideoFrame(new byte[0], 0, 0, ++frameSequence);
    }

    public static synchronized void writeHeader(int w, int h, int stride, int frameIndex, byte ready, int bufferIndex,
            String path) {
        // v2: video frame path metadata is owned by VideoBackground; no SHM header.
    }

    private static void ensureMapped() {
        // v2: JNI bridge, no mmap.
    }

    private static void writeUiState(byte kind, boolean active, int seq) {
        boolean islandActive = kind == ScreenKind.IN_GAME;
        NativeBridge.pushScreenState(kind, seq, active, islandActive, overlayAck);
    }
}
