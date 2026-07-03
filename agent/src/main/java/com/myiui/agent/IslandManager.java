package com.myiui.agent;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;

/** Notification queue for the in-game Inspiration Island (Dynamic Island). */
public final class IslandManager {
    public static final int SLOT_MUSIC = 0;
    public static final int SLOT_QUICK_MENU = 1;
    public static final int SLOT_SYSTEM = 2;
    public static final int SLOT_CUSTOM = 3;

    private static final int ISLAND_TITLE_LEN = 48;
    private static final int ISLAND_SUBTITLE_LEN = 96;
    private static final int ISLAND_LYRICS_LEN = 128;

    private static final long PERMANENT_MS = -1L;
    private static final List<IslandNotification> QUEUE = new ArrayList<>();
    private static volatile IslandNotification active;
    private static final AtomicInteger DEMO_STEP = new AtomicInteger(-1);
    private static volatile long demoNextMs = 0L;

    private IslandManager() {}

    public static synchronized void addNotification(String title, String subtitle, long durationMs, int slotId) {
        if (title == null) title = "";
        if (subtitle == null) subtitle = "";
        slotId = Math.max(0, Math.min(3, slotId));
        IslandNotification n = new IslandNotification(title.trim(), subtitle.trim(), durationMs, slotId);
        QUEUE.add(n);
        if (active == null) {
            active = n;
        }
        AgentLog.info("Island notify: " + title + " slot=" + slotId + " ms=" + durationMs);
    }

    public static synchronized void clear() {
        QUEUE.clear();
        active = null;
        DEMO_STEP.set(-1);
        demoNextMs = 0L;
    }

    public static synchronized void startDemoCycle() {
        clear();
        DEMO_STEP.set(0);
        demoNextMs = System.currentTimeMillis();
        AgentLog.info("Island demo cycle started");
    }

    public static synchronized void removeExpired() {
        long now = System.currentTimeMillis();
        Iterator<IslandNotification> it = QUEUE.iterator();
        while (it.hasNext()) {
            IslandNotification n = it.next();
            if (n.isExpired(now)) {
                it.remove();
            }
        }
        if (active != null && active.isExpired(now)) {
            active = null;
        }
        if (active == null && !QUEUE.isEmpty()) {
            active = QUEUE.get(0);
        }
        tickDemo(now);
    }

    private static void tickDemo(long now) {
        int step = DEMO_STEP.get();
        if (step < 0) {
            return;
        }
        if (now < demoNextMs) {
            return;
        }
        switch (step) {
            case 0 -> {
                addNotification("灵感岛", "弹性展开动画", 2800, SLOT_SYSTEM);
                DEMO_STEP.set(1);
                demoNextMs = now + 3200;
            }
            case 1 -> {
                addNotification("槽位 0", "网易云播放器（预留）", 2800, SLOT_MUSIC);
                DEMO_STEP.set(2);
                demoNextMs = now + 3200;
            }
            case 2 -> {
                addNotification("槽位 1", "游戏内 UI 编辑器（预留）", 2800, SLOT_QUICK_MENU);
                DEMO_STEP.set(3);
                demoNextMs = now + 3200;
            }
            case 3 -> {
                clear();
                DEMO_STEP.set(-1);
                AgentLog.info("Island demo cycle finished");
            }
            default -> DEMO_STEP.set(-1);
        }
    }

    public static synchronized IslandSnapshot buildSnapshot(int fps) {
        IslandSnapshot snap = new IslandSnapshot();
        snap.fps = fps;
        snap.notifyCount = QUEUE.size();
        // 优先检查网易云播放器状态 — 播放中时填入曲名/歌手/歌词
        if (com.myiui.agent.netease.MusicPlayer.isPlaying() || com.myiui.agent.netease.MusicPlayer.isPaused()) {
            com.myiui.agent.netease.NetEaseModels.Song song = com.myiui.agent.netease.MusicPlayer.currentSong();
            if (song != null) {
                snap.hasNotification = false;
                snap.notifyCount = 0;  // 音乐播放时不显示通知展开
                snap.activeSlot = SLOT_MUSIC;
                snap.title = truncate(song.name, ISLAND_TITLE_LEN);
                snap.subtitle = truncate(song.artist, ISLAND_SUBTITLE_LEN);
                snap.lyrics = truncate(com.myiui.agent.netease.LyricManager.currentLine(com.myiui.agent.netease.MusicPlayer.positionMs()),
                        ISLAND_LYRICS_LEN);
                snap.expireMs = 0xFFFFFFFFL;  // 永久显示直到切歌
                snap.mode = com.myiui.agent.netease.LyricManager.hasLyrics() ? (byte) 4 : (byte) 0;  // Lyrics 模式
                snap.slots = defaultSlots();
                return snap;
            }
        }
        if (active != null) {
            snap.hasNotification = true;
            snap.activeSlot = active.slotId;
            snap.title = active.title;
            snap.subtitle = active.subtitle;
            snap.expireMs = active.durationMs < 0 ? 0xFFFFFFFFL : active.remainingMs(System.currentTimeMillis());
        } else {
            snap.hasNotification = false;
            snap.activeSlot = 255;
            snap.title = defaultTitle(fps);
            snap.subtitle = "";
            snap.expireMs = 0;
        }
        snap.lyrics = "";
        snap.slots = defaultSlots();
        return snap;
    }

    private static String truncate(String s, int maxLen) {
        if (s == null) return "";
        return s.length() <= maxLen ? s : s.substring(0, maxLen);
    }

    public static String defaultTitle(int fps) {
        return fps > 0 ? fps + " FPS" : "MyiUI";
    }

    private static byte[][] defaultSlots() {
        byte[][] slots = new byte[4][4];
        slots[0][0] = SLOT_MUSIC;
        slots[0][1] = 1;
        slots[0][2] = 1;
        slots[1][0] = SLOT_QUICK_MENU;
        slots[1][1] = 1;
        slots[1][2] = 2;
        slots[2][0] = SLOT_SYSTEM;
        slots[2][1] = 1;
        slots[2][2] = 3;
        slots[3][0] = SLOT_CUSTOM;
        slots[3][1] = 1;
        slots[3][2] = 4;
        return slots;
    }

    public static String slotsConfigJson() {
        return "{\"slots\":["
                + "{\"id\":\"music\",\"enabled\":true,\"icon\":\"music\"},"
                + "{\"id\":\"quick_menu\",\"enabled\":true,\"icon\":\"menu\"},"
                + "{\"id\":\"system\",\"enabled\":true,\"icon\":\"bell\"},"
                + "{\"id\":\"custom\",\"enabled\":true,\"icon\":\"star\"}"
                + "]}";
    }

    static final class IslandNotification {
        final String title;
        final String subtitle;
        final long durationMs;
        final int slotId;
        final long startMs;

        IslandNotification(String title, String subtitle, long durationMs, int slotId) {
            this.title = title;
            this.subtitle = subtitle;
            this.durationMs = durationMs;
            this.slotId = slotId;
            this.startMs = System.currentTimeMillis();
        }

        boolean isExpired(long now) {
            if (durationMs < 0) {
                return false;
            }
            return now - startMs > durationMs;
        }

        long remainingMs(long now) {
            if (durationMs < 0) {
                return 0xFFFFFFFFL;
            }
            return Math.max(0, durationMs - (now - startMs));
        }
    }

    static final class IslandSnapshot {
        int fps;
        int notifyCount;
        boolean hasNotification;
        int activeSlot;
        String title;
        String subtitle;
        String lyrics = "";
        long expireMs;
        byte mode;
        byte[][] slots;
    }
}
