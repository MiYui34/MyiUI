package com.myiui.agent;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;

public final class ControlsBridge {
    private ControlsBridge() {}

    private static final List<String> DISPLAY_KEYBIND_ORDER = List.of(
            "forwardKey", "backKey", "leftKey", "rightKey", "jumpKey", "sneakKey", "sprintKey",
            "inventoryKey", "dropKey", "chatKey");

    public static String getKeybindsJson() {
        try {
            Object client = GameActions.resolveClientForBridge();
            if (client == null) return "{\"keybinds\":[]}";
            Object options = GameActions.getGameOptions(client);
            java.util.Map<String, Object> bindings = new java.util.LinkedHashMap<>();
            for (Field field : options.getClass().getDeclaredFields()) {
                field.setAccessible(true);
                Object val = field.get(options);
                if (val == null) continue;
                String typeName = val.getClass().getName();
                if (!typeName.contains("KeyBinding")) continue;
                bindings.put(field.getName(), val);
            }
            List<String> items = new ArrayList<>();
            for (String id : DISPLAY_KEYBIND_ORDER) {
                Object val = bindings.remove(id);
                if (val == null) continue;
                String label = keyLabel(id);
                String key = formatBoundKey(val);
                items.add("{\"id\":\"" + WorldBridge.escape(id) + "\",\"label\":\"" + WorldBridge.escape(label)
                        + "\",\"key\":\"" + WorldBridge.escape(key) + "\"}");
            }
            return "{\"keybinds\":[" + String.join(",", items) + "]}";
        } catch (Throwable e) {
            AgentLog.error("GET_KEYBINDS failed", e);
            return null;
        }
    }

    public static boolean setKeybind(String id, String keyCode) {
        try {
            int glfwCode = Integer.parseInt(keyCode.trim());
            final Object inputKey = resolveInputKey(glfwCode);
            if (inputKey == null) {
                AgentLog.error("无法解析按键码: " + keyCode);
                return false;
            }
            return GameActions.writeOnGameOptions(options -> {
                try {
                    Object binding = findKeyBinding(options, id);
                    if (binding == null) {
                        AgentLog.error("未知按键绑定: " + id);
                        return false;
                    }
                    Method setBoundKey = ReflectUtil.findInstanceMethod(binding.getClass(), "setBoundKey", "method_1422",
                            inputKey.getClass());
                    setBoundKey.invoke(binding, inputKey);
                    return true;
                } catch (ReflectiveOperationException e) {
                    AgentLog.error("setKeybind 写入失败: " + id, e);
                    return false;
                }
            }).success();
        } catch (NumberFormatException e) {
            AgentLog.error("按键码无效: " + keyCode);
            return false;
        } catch (Throwable e) {
            AgentLog.error("SET_KEYBIND failed: " + id, e);
            return false;
        }
    }

    private static Object resolveInputKey(int glfwCode) throws ReflectiveOperationException {
        Object client = GameActions.resolveClientForBridge();
        if (client == null) return null;
        ClassLoader loader = client.getClass().getClassLoader();
        Class<?> inputUtilClass = Class.forName("net.minecraft.client.util.InputUtil", true, loader);
        Method fromKeyCode = inputUtilClass.getMethod("fromKeyCode", int.class, int.class);
        return fromKeyCode.invoke(null, glfwCode, glfwCode);
    }

    private static Object findKeyBinding(Object options, String id) throws ReflectiveOperationException {
        for (Field field : options.getClass().getDeclaredFields()) {
            if (!field.getName().equals(id)) continue;
            field.setAccessible(true);
            Object val = field.get(options);
            if (val != null && val.getClass().getName().contains("KeyBinding")) {
                return val;
            }
        }
        return null;
    }

    private static String formatBoundKey(Object binding) throws ReflectiveOperationException {
        Object boundKey = ReflectUtil.getField(binding, "boundKey", "field_1655");
        if (boundKey == null) return "?";
        try {
            Method getTranslationKey = ReflectUtil.findInstanceMethod(boundKey.getClass(), "getTranslationKey",
                    "method_1444");
            Object translation = getTranslationKey.invoke(boundKey);
            if (translation != null) {
                String s = String.valueOf(translation);
                if (!s.isEmpty()) return s;
            }
        } catch (ReflectiveOperationException ignored) {
        }
        try {
            Method getCode = ReflectUtil.findInstanceMethod(boundKey.getClass(), "getCode", "method_1442");
            return String.valueOf(getCode.invoke(boundKey));
        } catch (ReflectiveOperationException e) {
            return String.valueOf(boundKey);
        }
    }

    private static String keyLabel(String fieldName) {
        return switch (fieldName) {
            case "forwardKey" -> "向前移动";
            case "backKey" -> "向后移动";
            case "leftKey" -> "向左移动";
            case "rightKey" -> "向右移动";
            case "jumpKey" -> "跳跃";
            case "sneakKey" -> "潜行";
            case "sprintKey" -> "冲刺";
            case "inventoryKey" -> "物品栏";
            case "dropKey" -> "丢弃物品";
            case "chatKey" -> "打开聊天";
            case "useKey" -> "使用";
            case "attackKey" -> "攻击";
            default -> fieldName;
        };
    }
}
