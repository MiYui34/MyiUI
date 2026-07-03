package com.myiui.agent;

import java.util.ArrayList;
import java.util.List;

final class LoadedClassesCache {
    private static final List<Class<?>> loaded = new ArrayList<>();

    private LoadedClassesCache() {}

    static void track(Class<?> clazz) {
        if (clazz == null) {
            return;
        }
        synchronized (loaded) {
            if (!loaded.contains(clazz)) {
                loaded.add(clazz);
            }
        }
    }

    static Class<?>[] snapshot() {
        synchronized (loaded) {
            return loaded.toArray(Class[]::new);
        }
    }
}
