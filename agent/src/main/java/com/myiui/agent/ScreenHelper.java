package com.myiui.agent;

import java.lang.reflect.Field;
import java.util.List;

public final class ScreenHelper {
    private ScreenHelper() {}

    public static void clearScreenChildren(Object screen) {
        try {
            Class<?> c = screen.getClass();
            while (c != null && c != Object.class) {
                for (Field field : c.getDeclaredFields()) {
                    String fieldName = field.getName();
                    if (List.class.isAssignableFrom(field.getType())
                            || fieldName.equals("children")
                            || fieldName.equals("drawables")
                            || fieldName.startsWith("field_")) {
                        field.setAccessible(true);
                        Object value = field.get(screen);
                        if (value instanceof List<?> list) {
                            list.clear();
                        }
                    }
                }
                c = c.getSuperclass();
            }
        } catch (ReflectiveOperationException e) {
            AgentLog.error("clearScreenChildren failed: " + e.getMessage());
        }
    }
}
