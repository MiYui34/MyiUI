package com.myiui.agent;

import java.lang.reflect.Field;
import java.util.List;

/** Reads chat messages from the game via reflection (no bytecode hook needed). */
public final class ChatReader {
    private static Field inGameHudField;
    private static Field chatHudField;
    private static Field messagesField;
    private static Field txtFieldInLine;
    private static int lastReadCount = -1;

    private ChatReader() {}

    public static void readMessages(Object client) {
        if (client == null) {
            return;
        }
        try {
            Object inGameHud = getInGameHud(client);
            if (inGameHud == null) return;
            Object chatHud = getChatHud(inGameHud);
            if (chatHud == null) return;
            List<?> messages = getMessages(chatHud);
            if (messages == null) return;

            int count = messages.size();
            if (count == lastReadCount) {
                return;
            }
            lastReadCount = count;

            int currentStored = ChatManager.snapshot().size();
            if (count > currentStored) {
                for (int i = Math.max(0, count - 16); i < count; i++) {
                    addLineToManager(messages.get(i));
                }
            } else if (count < currentStored) {
                ChatManager.clear();
                for (int i = 0; i < count; i++) {
                    addLineToManager(messages.get(i));
                }
            }
        } catch (Throwable t) {
            AgentLog.error("ChatReader: " + t.getMessage());
        }
    }

    private static void addLineToManager(Object line) {
        String text = extractText(line);
        if (text == null || text.isEmpty()) return;
        String user = ChatManager.extractUserPublic(text);
        String msg = ChatManager.extractTextPublic(text);
        ChatManager.addMessage(user, msg);
    }

    private static Field findFieldRecursive(Class<?> clazz, String... names) {
        while (clazz != null && clazz != Object.class) {
            for (String name : names) {
                try {
                    Field f = clazz.getDeclaredField(name);
                    f.setAccessible(true);
                    return f;
                } catch (NoSuchFieldException ignored) {
                }
            }
            clazz = clazz.getSuperclass();
        }
        return null;
    }

    private static Object getInGameHud(Object client) {
        if (inGameHudField == null) {
            inGameHudField = findFieldRecursive(client.getClass(), "inGameHud", "field_1842", "field_1738");
            if (inGameHudField != null) {
                AgentLog.info("ChatReader: inGameHud field = " + inGameHudField.getName());
            }
        }
        if (inGameHudField == null) return null;
        try {
            return inGameHudField.get(client);
        } catch (Exception e) {
            return null;
        }
    }

    private static Object getChatHud(Object inGameHud) {
        if (chatHudField == null) {
            chatHudField = findFieldRecursive(inGameHud.getClass(), "chatHud", "field_2055");
            if (chatHudField == null) {
                for (Field f : inGameHud.getClass().getDeclaredFields()) {
                    String type = f.getType().getName();
                    if (type.contains("ChatHud") || type.contains("class_334")) {
                        chatHudField = f;
                        chatHudField.setAccessible(true);
                        AgentLog.info("ChatReader: chatHud found by type = " + f.getName() + " type=" + type);
                        break;
                    }
                }
            } else {
                AgentLog.info("ChatReader: chatHud field = " + chatHudField.getName());
            }
        }
        if (chatHudField == null) return null;
        try {
            return chatHudField.get(inGameHud);
        } catch (Exception e) {
            return null;
        }
    }

    @SuppressWarnings("unchecked")
    private static List<?> getMessages(Object chatHud) {
        if (messagesField == null) {
            messagesField = findFieldRecursive(chatHud.getClass(), "messages", "field_2063", "visibleMessages");
            if (messagesField == null) {
                for (Field f : chatHud.getClass().getDeclaredFields()) {
                    if (List.class.isAssignableFrom(f.getType())) {
                        messagesField = f;
                        messagesField.setAccessible(true);
                        AgentLog.info("ChatReader: messages found by type = " + f.getName());
                        break;
                    }
                }
            } else {
                AgentLog.info("ChatReader: messages field = " + messagesField.getName());
            }
        }
        if (messagesField == null) return null;
        try {
            return (List<?>) messagesField.get(chatHud);
        } catch (Exception e) {
            return null;
        }
    }

    private static String extractText(Object chatLine) {
        if (chatLine == null) return null;
        if (txtFieldInLine == null) {
            txtFieldInLine = findFieldRecursive(chatLine.getClass(), "text", "field_2064", "content", "message");
            if (txtFieldInLine == null) {
                Class<?> clazz = chatLine.getClass();
                while (clazz != null && clazz != Object.class) {
                    for (Field f : clazz.getDeclaredFields()) {
                        String type = f.getType().getName();
                        if (type.contains("Text") || type.contains("class_2561") || type.contains("Component")) {
                            txtFieldInLine = f;
                            txtFieldInLine.setAccessible(true);
                            break;
                        }
                    }
                    if (txtFieldInLine != null) break;
                    clazz = clazz.getSuperclass();
                }
            }
        }
        if (txtFieldInLine == null) return null;
        try {
            Object textObj = txtFieldInLine.get(chatLine);
            if (textObj == null) return null;
            try {
                java.lang.reflect.Method m = textObj.getClass().getMethod("getString");
                return (String) m.invoke(textObj);
            } catch (Exception ignored) {
            }
            return String.valueOf(textObj);
        } catch (Exception e) {
            return null;
        }
    }
}
