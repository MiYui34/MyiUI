package com.myiui.agent;

import java.util.ArrayList;
import java.util.List;

/** Stores recent chat messages for overlay rendering. */
public final class ChatManager {
    private static final int MAX_MESSAGES = 16;
    private static final long HIDE_TIMEOUT_MS = 12000;
    private static final List<String[]> MESSAGES = new ArrayList<>();
    private static volatile boolean visible = false;
    private static volatile long lastMessageTime = 0;

    private ChatManager() {}

    public static void captureMessage(Object textComponent) {
        if (textComponent == null) {
            return;
        }
        String parsed = parseChatRaw(textComponent);
        String user = extractUser(parsed);
        String text = extractText(parsed);
        addMessage(user, text);
    }

    private static String parseChatRaw(Object textComponent) {
        // Try getString() with no args first (most common)
        try {
            java.lang.reflect.Method m = textComponent.getClass().getMethod("getString");
            return (String) m.invoke(textComponent);
        } catch (Exception ignored) {
        }
        // Try getLiteralString or getPlainString
        for (String methodName : new String[]{"getLiteralString", "getPlainString", "asString"}) {
            try {
                java.lang.reflect.Method m = textComponent.getClass().getMethod(methodName);
                Object result = m.invoke(textComponent);
                if (result instanceof String s) {
                    return s;
                }
            } catch (Exception ignored) {
            }
        }
        return String.valueOf(textComponent);
    }

    private static String extractUser(String line) {
        int lt = line.indexOf('<');
        int gt = line.indexOf('>');
        if (lt >= 0 && gt > lt) {
            return line.substring(lt + 1, gt).trim();
        }
        // Also try ":" separator
        int colon = line.indexOf(':');
        if (colon > 0 && colon < line.length() / 2) {
            return line.substring(0, colon).trim();
        }
        return "";
    }

    private static String extractText(String line) {
        int gt = line.indexOf('>');
        if (gt >= 0) {
            return line.substring(gt + 1).trim();
        }
        int colon = line.indexOf(':');
        if (colon > 0 && colon < line.length() / 2) {
            return line.substring(colon + 1).trim();
        }
        return line;
    }

    public static String extractUserPublic(String line) {
        return extractUser(line);
    }

    public static String extractTextPublic(String line) {
        return extractText(line);
    }

    public static synchronized void addMessage(String user, String text) {
        if (user == null) user = "";
        if (text == null) text = "";
        user = user.trim();
        text = text.trim();
        if (user.isEmpty() && text.isEmpty()) {
            return;
        }
        MESSAGES.add(new String[]{user, text});
        while (MESSAGES.size() > MAX_MESSAGES) {
            MESSAGES.remove(0);
        }
        lastMessageTime = System.currentTimeMillis();
        visible = true;
        AgentLog.info("Chat: <" + user + "> " + text);
    }

    public static synchronized List<String[]> snapshot() {
        return new ArrayList<>(MESSAGES);
    }

    public static synchronized void clear() {
        MESSAGES.clear();
    }

    public static void setVisible(boolean v) {
        visible = v;
    }

    public static boolean isVisible() {
        return visible;
    }

    /** Called each tick — auto-hide after timeout. */
    public static void tickVisibility() {
        if (visible && lastMessageTime > 0) {
            if (System.currentTimeMillis() - lastMessageTime > HIDE_TIMEOUT_MS) {
                visible = false;
            }
        }
    }

    public static synchronized void writeState() {
        tickVisibility();
        SharedState.writeChatState(snapshot(), visible);
    }
}
