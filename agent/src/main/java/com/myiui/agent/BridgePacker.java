package com.myiui.agent;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.List;

final class BridgePacker {
    private static final int HUD_STATE_SIZE = 108;
    private static final int CHAT_USER_LEN = 32;
    private static final int CHAT_TEXT_LEN = 192;
    private static final int CHAT_MAX_MESSAGES = 16;
    private static final int CHAT_STATE_SIZE = 6 + CHAT_MAX_MESSAGES * (CHAT_USER_LEN + CHAT_TEXT_LEN);

    private BridgePacker() {}

    static byte[] packHud(HudBridge.HudSnapshot snap, byte flags, int hudSeq) {
        ByteBuffer buf = ByteBuffer.allocate(HUD_STATE_SIZE).order(ByteOrder.LITTLE_ENDIAN);
        buf.put((byte) 1);
        buf.put(flags);
        buf.put((byte) snap.selectedSlot);
        buf.put((byte) Math.max(1, Math.min(4, snap.guiScale)));
        buf.putShort((short) (hudSeq & 0xFFFF));
        buf.putShort((short) 0);
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
            HudBridge.HotbarSlot slot = snap.slots[i] == null ? new HudBridge.HotbarSlot() : snap.slots[i];
            buf.putShort(slot.itemId);
            buf.put(slot.count);
            buf.put(slot.durabilityPct);
            buf.put(slot.cooldownPct);
            buf.put((byte) 0);
        }
        return buf.array();
    }

    static byte[] packChat(List<String[]> messages, boolean visible, int chatSeq) {
        ByteBuffer buf = ByteBuffer.allocate(CHAT_STATE_SIZE).order(ByteOrder.LITTLE_ENDIAN);
        buf.put((byte) 1);
        buf.put((byte) (visible ? 1 : 0));
        int count = Math.min(CHAT_MAX_MESSAGES, messages == null ? 0 : messages.size());
        buf.put((byte) count);
        buf.put((byte) 0);
        buf.putShort((short) (chatSeq & 0xFFFF));
        for (int i = 0; i < count; i++) {
            String[] msg = messages.get(i);
            writeFixed(buf, msg.length > 0 ? msg[0] : "", CHAT_USER_LEN);
            writeFixed(buf, msg.length > 1 ? msg[1] : "", CHAT_TEXT_LEN);
        }
        for (int i = count; i < CHAT_MAX_MESSAGES; i++) {
            writeFixed(buf, "", CHAT_USER_LEN);
            writeFixed(buf, "", CHAT_TEXT_LEN);
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
