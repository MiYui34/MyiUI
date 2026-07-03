package com.myiui.agent;

import java.io.InputStream;
import java.lang.reflect.Field;
import java.lang.reflect.Method;

final class GameClassInjector {
    private static final String HOOKS_CLASS = "com.myiui.agent.runtime.GameHooks";
    private static final String HOOKS_INTERNAL = "com/myiui/agent/runtime/GameHooks";

    private GameClassInjector() {}

    static ClassLoader findGameClassLoader(JvmtiInstrumentation inst) {
        for (Class<?> c : inst.getAllLoadedClasses()) {
            String name = c.getName();
            if ("net.minecraft.client.MinecraftClient".equals(name) || "net.minecraft.class_310".equals(name)) {
                return c.getClassLoader();
            }
        }
        return null;
    }

    static void injectHooks(JvmtiInstrumentation inst) {
        ClassLoader gameLoader = findGameClassLoader(inst);
        if (gameLoader == null) {
            AgentLog.error("Game ClassLoader not found — cannot inject GameHooks.");
            return;
        }

        try {
            byte[] bytes = readClassBytes(HOOKS_INTERNAL + ".class");
            Class<?> hooks = defineClass(gameLoader, HOOKS_CLASS, bytes);
            Method bind = hooks.getDeclaredMethod("bindAgentLoader", ClassLoader.class);
            bind.invoke(null, GameClassInjector.class.getClassLoader());
            AgentLog.info("GameHooks defined in game ClassLoader: " + hooks.getClassLoader().getClass().getName());
        } catch (Throwable t) {
            AgentLog.error("GameHooks injection failed", t);
        }
    }

    private static byte[] readClassBytes(String resourcePath) throws java.io.IOException {
        ClassLoader loader = GameClassInjector.class.getClassLoader();
        try (InputStream in = loader.getResourceAsStream(resourcePath)) {
            if (in == null) throw new java.io.FileNotFoundException(resourcePath);
            return in.readAllBytes();
        }
    }

    private static Class<?> defineClass(ClassLoader target, String binaryName, byte[] bytes) throws Exception {
        try {
            Method define = ClassLoader.class.getDeclaredMethod(
                    "defineClass", String.class, byte[].class, int.class, int.class);
            define.setAccessible(true);
            return (Class<?>) define.invoke(target, binaryName, bytes, 0, bytes.length);
        } catch (NoSuchMethodException e) {
            // Java 9+ module ClassLoader path
            Class<?> unsafeClass = Class.forName("sun.misc.Unsafe");
            Field theUnsafe = unsafeClass.getDeclaredField("theUnsafe");
            theUnsafe.setAccessible(true);
            Object unsafe = theUnsafe.get(null);
            Method defineClass = unsafeClass.getMethod(
                    "defineClass", String.class, byte[].class, int.class, int.class, ClassLoader.class, java.security.ProtectionDomain.class);
            return (Class<?>) defineClass.invoke(unsafe, binaryName, bytes, 0, bytes.length, target, null);
        }
    }
}
