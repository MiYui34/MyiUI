package com.myiui.agent;

import java.io.File;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.net.URL;
import java.nio.file.Path;

public final class AgentExposure {
    private AgentExposure() {}

    public static void exposeToGame(JvmtiInstrumentation inst, ClassLoader gameLoader) {
        try {
            if (gameLoader == null) {
                gameLoader = resolveGameLoader(inst);
            }
            if (gameLoader == null) {
                AgentLog.error("Game ClassLoader not found — injected hooks will crash on Fabric.");
                return;
            }

            URL agentUrl = Main.class.getProtectionDomain().getCodeSource().getLocation();
            File agentJar = new File(agentUrl.toURI());
            AgentLog.info("Game ClassLoader: " + gameLoader.getClass().getName());

            if (tryAddUrl(gameLoader, agentUrl)) {
                AgentLog.info("Agent JAR exposed to game ClassLoader via addURL.");
                verifyLoad(gameLoader, "com.myiui.agent.SharedState");
                return;
            }

            if (tryKnotDelegate(gameLoader, agentJar, agentUrl)) {
                AgentLog.info("Agent JAR exposed via Knot delegate.");
                verifyLoad(gameLoader, "com.myiui.agent.SharedState");
                return;
            }

            AgentLog.info("Agent JAR loaded via URLClassLoader parent chain.");
            verifyLoad(gameLoader, "com.myiui.agent.SharedState");
        } catch (Throwable t) {
            AgentLog.error("exposeToGame failed", t);
        }
    }

    public static ClassLoader resolveGameLoader(JvmtiInstrumentation inst) {
        for (String name : new String[] {"net.minecraft.class_310", "net.minecraft.client.MinecraftClient"}) {
            try {
                Class<?> c = Class.forName(name, false, Thread.currentThread().getContextClassLoader());
                LoadedClassesCache.track(c);
                return c.getClassLoader();
            } catch (Throwable ignored) {
            }
        }
        if (inst == null) {
            return null;
        }
        for (Class<?> c : inst.getAllLoadedClasses()) {
            if ("net.minecraft.class_310".equals(c.getName())
                    || "net.minecraft.client.MinecraftClient".equals(c.getName())) {
                return c.getClassLoader();
            }
        }
        return null;
    }

    private static boolean tryAddUrl(ClassLoader loader, URL url) {
        ClassLoader current = loader;
        while (current != null) {
            try {
                Method addURL = current.getClass().getDeclaredMethod("addURL", URL.class);
                addURL.setAccessible(true);
                addURL.invoke(current, url);
                return true;
            } catch (NoSuchMethodException ignored) {
            } catch (Throwable t) {
                AgentLog.error("addURL failed on " + current.getClass().getName() + ": " + t.getMessage());
            }
            current = current.getParent();
        }
        return false;
    }

    private static boolean tryKnotDelegate(ClassLoader loader, File jar, URL url) {
        try {
            Object delegate = readField(loader, "delegate");
            if (delegate == null) {
                return false;
            }

            for (Method method : delegate.getClass().getDeclaredMethods()) {
                String name = method.getName();
                if (!name.equals("addJar") && !name.equals("addToClasspath") && !name.equals("addURL")
                        && !name.equals("addToURLs")) {
                    continue;
                }
                method.setAccessible(true);
                Class<?>[] params = method.getParameterTypes();
                if (params.length != 1) {
                    continue;
                }
                if (params[0] == File.class) {
                    method.invoke(delegate, jar);
                    return true;
                }
                if (params[0] == URL.class) {
                    method.invoke(delegate, url);
                    return true;
                }
                if (params[0] == Path.class) {
                    method.invoke(delegate, jar.toPath());
                    return true;
                }
            }
        } catch (Throwable t) {
            AgentLog.error("tryKnotDelegate: " + t.getMessage());
        }
        return false;
    }

    private static Object readField(Object target, String name) {
        try {
            Class<?> clazz = target.getClass();
            while (clazz != null) {
                try {
                    Field field = clazz.getDeclaredField(name);
                    field.setAccessible(true);
                    return field.get(target);
                } catch (NoSuchFieldException ignored) {
                }
                clazz = clazz.getSuperclass();
            }
        } catch (Throwable ignored) {
        }
        return null;
    }

    public static void verifyLoad(ClassLoader loader, String className) {
        try {
            Class.forName(className, true, loader);
            AgentLog.info("Verified game loader can load " + className);
        } catch (Throwable t) {
            AgentLog.error("Game loader still cannot load " + className + ": " + t.getMessage());
        }
    }
}
