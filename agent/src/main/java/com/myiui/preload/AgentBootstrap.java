package com.myiui.preload;

import java.io.File;
import java.lang.reflect.Method;
import java.net.URL;
import java.net.URLClassLoader;

/** Embedded bootstrap class (defineClass into game ClassLoader). */
public final class AgentBootstrap {
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

            // Add agent jar to game ClassLoader so ALL agent classes are loaded
            // by the same loader as game classes — no dual-classloader conflicts.
            if (!addUrlToLoader(gameLoader, jarUrl)) {
                throw new RuntimeException("addURL failed on " + gameLoader);
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

    private static boolean addUrlToLoader(ClassLoader loader, URL url) {
        ClassLoader current = loader;
        while (current != null) {
            try {
                Method addURL = current.getClass().getDeclaredMethod("addURL", URL.class);
                addURL.setAccessible(true);
                addURL.invoke(current, url);
                return true;
            } catch (NoSuchMethodException ignored) {
            } catch (Throwable t) {
                AgentBootstrap.log("addURL failed on " + current.getClass().getName() + ": " + t.getMessage());
            }
            current = current.getParent();
        }
        return false;
    }
}
