package com.myiui.agent;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.List;

final class BridgePacker {
    static final int HUD_STATE_SIZE = 114;
    static final int INFO_HUD_STATE_SIZE = 64;
    static final int MUSIC_HUD_STATE_SIZE = 596;
    private static final int TAB_LIST_STATE_SIZE = 824;

    private BridgePacker() {}

    static byte[] packHud(HudBridge.HudSnapshot snap, byte flags, int hudSeq) {
        ByteBuffer buf = ByteBuffer.allocate(HUD_STATE_SIZE).order(ByteOrder.LITTLE_ENDIAN);
        buf.put((byte) 1);
        buf.put(flags);
        buf.put((byte) snap.selectedSlot);
        buf.put((byte) Math.max(1, Math.min(4, snap.guiScale)));
        buf.putShort((short) (hudSeq & 0xFFFF));
        buf.put(HudBridge.HUD_VERSION);
        buf.put(snap.creative ? (byte) 1 : (byte) 0);
        buf.putFloat(snap.health);
        buf.putFloat(snap.healthMax);
        buf.putFloat(snap.absorption);
        buf.putFloat(snap.food);
        buf.putFloat(snap.saturation);
        buf.putFloat(snap.exhaustion);
        buf.putShort((short) snap.armor);
        buf.putShort((short) snap.air);
        buf.putShort((short) Math.max(1, snap.maxAir));
        buf.put(snap.underwater ? (byte) 1 : (byte) 0);
        buf.put((byte) 0);
        buf.putShort((short) snap.hotbarLeftPx);
        buf.putShort((short) snap.hotbarTopPx);
        buf.putShort((short) snap.hotbarSlotPx);
        buf.putShort((short) snap.xpLevel);
        buf.putFloat(snap.xpProgress);
        buf.putShort((short) 0);
        for (int i = 0; i < 9; i++) {
            putHotbarSlot(buf, snap.slots[i]);
        }
        putHotbarSlot(buf, snap.offhand);
        return buf.array();
    }

    private static void putHotbarSlot(ByteBuffer buf, HudBridge.HotbarSlot slot) {
        HudBridge.HotbarSlot s = slot == null ? new HudBridge.HotbarSlot() : slot;
        buf.putShort(s.itemId);
        buf.put(s.count);
        buf.put(s.durabilityPct);
        buf.put(s.cooldownPct);
        buf.put((byte) 0);
    }

    static byte[] packInfo(InfoHudBridge.InfoSnapshot snap, int infoSeq) {
        ByteBuffer buf = ByteBuffer.allocate(INFO_HUD_STATE_SIZE).order(ByteOrder.LITTLE_ENDIAN);
        buf.put((byte) 1);
        buf.put((byte) 0);
        buf.putShort((short) (infoSeq & 0xFFFF));
        buf.putInt(snap.blockX);
        buf.putInt(snap.blockY);
        buf.putInt(snap.blockZ);
        buf.putShort((short) snap.pingMs);
        buf.putShort((short) snap.fps);
        buf.putFloat(snap.speedBps);
        buf.putFloat(snap.yaw);
        buf.putFloat(snap.pitch);
        writeFixed(buf, snap.biome, 16);
        writeFixed(buf, snap.direction, 4);
        while (buf.position() < INFO_HUD_STATE_SIZE) {
            buf.put((byte) 0);
        }
        return buf.array();
    }

    static byte[] packMusic(MusicHudBridge.MusicSnapshot snap, int musicSeq) {
        ByteBuffer buf = ByteBuffer.allocate(MUSIC_HUD_STATE_SIZE).order(ByteOrder.LITTLE_ENDIAN);
        buf.put((byte) ((snap.playing || snap.paused) ? 1 : 0));
        buf.put(snap.playing ? (byte) 1 : (byte) 0);
        buf.put(snap.paused ? (byte) 1 : (byte) 0);
        buf.put((byte) 0);
        buf.putShort((short) (musicSeq & 0xFFFF));
        buf.putShort((short) 0);
        buf.putInt((int) (snap.positionMs & 0xFFFFFFFFL));
        buf.putInt((int) (snap.durationMs & 0xFFFFFFFFL));
        writeFixed(buf, snap.title, 48);
        writeFixed(buf, snap.artist, 48);
        writeFixed(buf, snap.coverUrl, 96);
        for (int i = 0; i < MusicHudBridge.WAVEFORM_BINS; i++) {
            float v = i < snap.waveform.length ? snap.waveform[i] : 0f;
            buf.putFloat(v);
        }
        writeFixed(buf, snap.lyricCurrent, 128);
        writeFixed(buf, snap.lyricNext, 128);
        buf.putFloat(snap.lyricProgress);
        while (buf.position() < MUSIC_HUD_STATE_SIZE) {
            buf.put((byte) 0);
        }
        return buf.array();
    }

    static byte[] packTabList(PlayerListBridge.TabSnapshot snap, int tabSeq) {
        ByteBuffer buf = ByteBuffer.allocate(TAB_LIST_STATE_SIZE).order(ByteOrder.LITTLE_ENDIAN);
        buf.put((byte) 1);
        buf.put(snap.tabVisible ? (byte) 1 : (byte) 0);
        buf.put((byte) Math.min(255, snap.playerCount));
        buf.put((byte) 0);
        buf.putShort((short) (tabSeq & 0xFFFF));
        buf.putShort((short) 0);
        writeFixed(buf, snap.header, 48);
        for (int i = 0; i < 32; i++) {
            String name = i < snap.names.length && snap.names[i] != null ? snap.names[i] : "";
            writeFixed(buf, name, 20);
            short ping = i < snap.pings.length ? snap.pings[i] : (short) 0;
            buf.putShort(ping);
            buf.putShort((short) 0);
        }
        return buf.array();
    }

    static byte[] islandSlots(IslandManager.IslandSnapshot snap) {
        byte[] out = new byte[16];
        for (int i = 0; i < 4; i++) {
            out[i * 4] = snap.slots[i][0];
            out[i * 4 + 1] = snap.slots[i][1];
            out[i * 4 + 2] = snap.slots[i][2];
        }
        return out;
    }

    private static void writeFixed(ByteBuffer buf, String text, int maxLen) {
        byte[] bytes = text == null ? new byte[0] : text.getBytes(StandardCharsets.UTF_8);
        for (int i = 0; i < maxLen; i++) {
            buf.put(i < bytes.length ? bytes[i] : (byte) 0);
        }
    }
}
