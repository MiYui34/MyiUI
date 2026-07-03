package com.myiui.agent;

import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassWriter;

final class SafeClassWriter extends ClassWriter {
    private final ClassLoader gameLoader;

    SafeClassWriter(ClassReader reader, int flags, ClassLoader gameLoader) {
        super(reader, flags);
        this.gameLoader = gameLoader;
    }

    @Override
    protected String getCommonSuperClass(String type1, String type2) {
        ClassLoader loader = gameLoader != null ? gameLoader : Thread.currentThread().getContextClassLoader();
        try {
            Class<?> c1 = Class.forName(type1.replace('/', '.'), false, loader);
            Class<?> c2 = Class.forName(type2.replace('/', '.'), false, loader);
            if (c1.isAssignableFrom(c2)) return type1;
            if (c2.isAssignableFrom(c1)) return type2;
            if (c1.isInterface() || c2.isInterface()) {
                return "java/lang/Object";
            }
            do {
                c1 = c1.getSuperclass();
            } while (!c1.isAssignableFrom(c2));
            return c1.getName().replace('.', '/');
        } catch (Throwable t) {
            return "java/lang/Object";
        }
    }
}

final class ReflectUtil {
    private ReflectUtil() {}

    static Object getScreenClient(Object screen) {
        if (screen == null) {
            return null;
        }
        for (String[] candidate : new String[][]{
                {"client", "field_22787"},
                {"minecraft", "field_22787"},
                {"client", "field_1705"},
        }) {
            try {
                Object client = getField(screen, candidate[0], candidate[1]);
                if (client != null) {
                    return client;
                }
            } catch (ReflectiveOperationException ignored) {
            }
        }
        return findFieldByType(screen, "net.minecraft.client.MinecraftClient", "net.minecraft.class_310");
    }

    static Object getCurrentScreen(Object client) throws ReflectiveOperationException {
        for (String[] candidate : new String[][]{
                {"currentScreen", "field_1755"},
                {"field_1755", "currentScreen"},
                {"currentScreen", "field_40896"},
        }) {
            try {
                Object screen = getField(client, candidate[0], candidate[1]);
                if (screen != null) {
                    return screen;
                }
            } catch (ReflectiveOperationException ignored) {
            }
        }
        Object screen = findFieldByType(client, "net.minecraft.client.gui.screen.Screen", "net.minecraft.class_437");
        if (screen != null) {
            return screen;
        }
        throw new NoSuchFieldException("currentScreen");
    }

    static Object getRunDirectory(Object client) throws ReflectiveOperationException {
        for (String[] candidate : new String[][]{
                {"runDirectory", "field_1697"},
                {"runDirectory", "field_16973"},
                {"runDirectory", "field_54518"},
                {"gameDirectory", "field_1697"},
                {"gameDir", "field_1697"},
        }) {
            try {
                Object value = getField(client, candidate[0], candidate[1]);
                if (value != null) {
                    return value;
                }
            } catch (ReflectiveOperationException ignored) {
            }
        }
        Object dir = findGameDirectory(client);
        if (dir != null) {
            return dir;
        }
        throw new NoSuchFieldException("runDirectory");
    }

    static Object getWorld(Object client) {
        for (String[] candidate : new String[][]{
                {"world", "field_1687"},
                {"world", "field_1688"},
                {"clientWorld", "field_1687"},
        }) {
            try {
                return getField(client, candidate[0], candidate[1]);
            } catch (ReflectiveOperationException ignored) {
            }
        }
        return findFieldByType(client, "net.minecraft.client.world.ClientWorld", "net.minecraft.class_638");
    }

    static Object findFieldByType(Object target, String... typeNames) {
        Class<?> clazz = target.getClass();
        while (clazz != null && clazz != Object.class) {
            for (java.lang.reflect.Field field : clazz.getDeclaredFields()) {
                String typeName = field.getType().getName();
                boolean match = false;
                for (String wanted : typeNames) {
                    if (wanted.equals(typeName)) {
                        match = true;
                        break;
                    }
                }
                if (!match) {
                    continue;
                }
                try {
                    field.setAccessible(true);
                    return field.get(target);
                } catch (ReflectiveOperationException ignored) {
                }
            }
            clazz = clazz.getSuperclass();
        }
        return null;
    }

    private static Object findGameDirectory(Object client) {
        java.io.File best = null;
        Class<?> clazz = client.getClass();
        while (clazz != null && clazz != Object.class) {
            for (java.lang.reflect.Field field : clazz.getDeclaredFields()) {
                if (!java.io.File.class.isAssignableFrom(field.getType())) {
                    continue;
                }
                try {
                    field.setAccessible(true);
                    Object value = field.get(client);
                    if (!(value instanceof java.io.File file) || !file.isDirectory()) {
                        continue;
                    }
                    if (new java.io.File(file, "saves").isDirectory()
                            || new java.io.File(file, "options.txt").isFile()) {
                        return file;
                    }
                    if (best == null) {
                        best = file;
                    }
                } catch (ReflectiveOperationException ignored) {
                }
            }
            clazz = clazz.getSuperclass();
        }
        return best;
    }

    private static Object findFileField(Object client) {
        return findGameDirectory(client);
    }

    static Object getStaticField(Class<?> clazz, String named, String intermediary) throws ReflectiveOperationException {
        for (String name : new String[]{named, intermediary}) {
            try {
                var field = clazz.getDeclaredField(name);
                field.setAccessible(true);
                return field.get(null);
            } catch (NoSuchFieldException ignored) {
            }
        }
        throw new NoSuchFieldException(named + "/" + intermediary);
    }

    static Object getField(Object target, String named, String intermediary) throws ReflectiveOperationException {
        Class<?> clazz = target.getClass();
        while (clazz != null && clazz != Object.class) {
            try {
                var field = clazz.getDeclaredField(named);
                field.setAccessible(true);
                return field.get(target);
            } catch (NoSuchFieldException ignored) {
            }
            try {
                var field = clazz.getDeclaredField(intermediary);
                field.setAccessible(true);
                return field.get(target);
            } catch (NoSuchFieldException ignored) {
            }
            clazz = clazz.getSuperclass();
        }
        throw new NoSuchFieldException(named + "/" + intermediary);
    }

    static java.lang.reflect.Method findStaticMethod(Class<?> clazz, String named, String intermediary)
            throws NoSuchMethodException {
        try {
            return clazz.getMethod(named);
        } catch (NoSuchMethodException e) {
            return clazz.getMethod(intermediary);
        }
    }

    static java.lang.reflect.Method findStaticMethod(Class<?> clazz, String named, String intermediary,
                                                     Class<?>... paramTypes) throws NoSuchMethodException {
        for (String name : new String[]{named, intermediary}) {
            try {
                java.lang.reflect.Method method = clazz.getDeclaredMethod(name, paramTypes);
                method.setAccessible(true);
                return method;
            } catch (NoSuchMethodException ignored) {
            }
            try {
                return clazz.getMethod(name, paramTypes);
            } catch (NoSuchMethodException ignored) {
            }
        }
        throw new NoSuchMethodException(named + "/" + intermediary);
    }

    static java.lang.reflect.Method findInstanceMethod(Class<?> clazz, String named, String intermediary,
                                                       Class<?>... paramTypes) throws NoSuchMethodException {
        Class<?> current = clazz;
        while (current != null && current != Object.class) {
            for (String name : new String[]{named, intermediary}) {
                try {
                    java.lang.reflect.Method method = current.getDeclaredMethod(name, paramTypes);
                    method.setAccessible(true);
                    return method;
                } catch (NoSuchMethodException ignored) {
                }
                try {
                    return current.getMethod(name, paramTypes);
                } catch (NoSuchMethodException ignored) {
                }
            }
            current = current.getSuperclass();
        }
        throw new NoSuchMethodException(named + "/" + intermediary);
    }
}
