package com.myiui.preload;

import java.io.File;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.net.URL;
import java.nio.file.Path;

/** Embedded bootstrap class (defineClass into game ClassLoader). */
public final class AgentBootstrap {
    /** Bump when bootstrap bytecode changes; preloader refuses stale in-JVM copies. */
    public static final int BOOTSTRAP_VERSION = 2;

    private static ClassLoader agentLoader;

    private AgentBootstrap() {}

    public static native void log(String message);

    public static native ClassLoader getClassLoader();

    /** Expose agent.jar to the game ClassLoader via addURL, then preload key classes. */
    public static void prepareAgent(String agentJarPath) {
        try {
            log("AgentBootstrap.prepareAgent: " + agentJarPath);
            ClassLoader gameLoader = getClassLoader();
            if (gameLoader == null) {
                gameLoader = Thread.currentThread().getContextClassLoader();
            }
            log("AgentBootstrap: gameLoader=" + gameLoader);

            File jar = new File(agentJarPath);
            URL jarUrl = jar.toURI().toURL();

            // Expose agent.jar to the game ClassLoader. KnotClassLoader ignores parent
            // addURL — must also try its delegate classpath APIs (see AgentExposure).
            addUrlToLoader(gameLoader, jarUrl);
            if (!canLoad(gameLoader, "com.myiui.agent.NativeBridge")) {
                log("AgentBootstrap: addURL did not expose jar — trying Knot delegate");
                if (!tryKnotDelegate(gameLoader, jar, jarUrl)) {
                    throw new RuntimeException("could not add agent jar to " + gameLoader);
                }
            }
            if (!canLoad(gameLoader, "com.myiui.agent.NativeBridge")) {
                throw new ClassNotFoundException("com.myiui.agent.NativeBridge (jar=" + jar + ")");
            }

            // Preload NativeBridge so RegisterNatives targets the correct class object.
            Class<?> nativeBridge = Class.forName("com.myiui.agent.NativeBridge", true, gameLoader);
            log("AgentBootstrap: NativeBridge loaded by " + nativeBridge.getClassLoader());

            agentLoader = gameLoader;
            Thread.currentThread().setContextClassLoader(gameLoader);
            log("AgentBootstrap.prepareAgent ok");
        } catch (Throwable t) {
            log("AgentBootstrap.prepareAgent failed: " + t);
            throw new RuntimeException(t);
        }
    }

    public static Class<?> nativeBridgeClass() throws ClassNotFoundException {
        if (agentLoader == null) {
            throw new IllegalStateException("agent not prepared");
        }
        return Class.forName("com.myiui.agent.NativeBridge", true, agentLoader);
    }

    public static void startAgent() {
        try {
            if (agentLoader == null) {
                throw new IllegalStateException("agent not prepared");
            }
            Class<?> mainClass = Class.forName("com.myiui.agent.Main", true, agentLoader);
            log("AgentBootstrap: Main loaded by " + mainClass.getClassLoader());
            mainClass.getMethod("main", String[].class).invoke(null, (Object) new String[0]);
            log("AgentBootstrap.startAgent ok");
        } catch (Throwable t) {
            log("AgentBootstrap.startAgent failed: " + t);
            throw new RuntimeException(t);
        }
    }

    private static void addUrlToLoader(ClassLoader loader, URL url) {
        ClassLoader current = loader;
        while (current != null) {
            try {
                Method addURL = current.getClass().getDeclaredMethod("addURL", URL.class);
                addURL.setAccessible(true);
                addURL.invoke(current, url);
                log("AgentBootstrap: addURL ok on " + current.getClass().getName());
                return;
            } catch (NoSuchMethodException ignored) {
            } catch (Throwable t) {
                log("AgentBootstrap: addURL failed on " + current.getClass().getName() + ": " + t.getMessage());
            }
            current = current.getParent();
        }
    }

    private static boolean canLoad(ClassLoader loader, String className) {
        try {
            Class.forName(className, false, loader);
            return true;
        } catch (Throwable ignored) {
            return false;
        }
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
                    log("AgentBootstrap: Knot delegate." + name + "(File) ok");
                    return true;
                }
                if (params[0] == URL.class) {
                    method.invoke(delegate, url);
                    log("AgentBootstrap: Knot delegate." + name + "(URL) ok");
                    return true;
                }
                if (params[0] == Path.class) {
                    method.invoke(delegate, jar.toPath());
                    log("AgentBootstrap: Knot delegate." + name + "(Path) ok");
                    return true;
                }
            }
        } catch (Throwable t) {
            log("AgentBootstrap: tryKnotDelegate failed: " + t.getMessage());
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
}
