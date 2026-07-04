package com.myiui.agent;

/** Overlay-pushed UI flags consumed by InGameHud transformers. */
public final class OverlayUiBridge {
    private static volatile boolean chatVisible = true;

    private OverlayUiBridge() {}

    public static void setChatVisible(boolean visible) {
        chatVisible = visible;
    }

    public static boolean isChatVisible() {
        return chatVisible;
    }

    /** Parses {@code chat=1} fragments from pipe command payload. */
    public static boolean applyFlags(String payload) {
        if (payload == null || payload.isBlank()) {
            return false;
        }
        String[] parts = payload.split(";");
        for (String part : parts) {
            int eq = part.indexOf('=');
            if (eq <= 0) continue;
            String key = part.substring(0, eq).trim();
            String val = part.substring(eq + 1).trim();
            boolean on = "1".equals(val) || "true".equalsIgnoreCase(val);
            if ("chat".equals(key)) {
                setChatVisible(on);
            }
        }
        return true;
    }
}
