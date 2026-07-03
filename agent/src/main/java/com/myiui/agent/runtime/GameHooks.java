package com.myiui.agent.runtime;

/**
 * Loaded into the Minecraft (Knot) ClassLoader. Game bytecode may only call this class.
 */
public final class GameHooks {
    private static volatile ClassLoader agentLoader;

    private GameHooks() {}

    public static void bindAgentLoader(ClassLoader loader) {
        agentLoader = loader;
    }

    public static void clearScreenChildren(Object screen) {
        invokeStatic("com.myiui.agent.ScreenHelper", "clearScreenChildren", screen);
    }

    public static void setMenuActive(boolean active) {
        invokeStatic("com.myiui.agent.SharedState", "setMenuActive", active);
    }

    public static void onTitleScreenRender() {
        invokeStatic("com.myiui.agent.SharedState", "onTitleScreenRender");
    }

    public static void onSetScreen(Object screen) {
        invokeStatic("com.myiui.agent.SharedState", "onSetScreen", screen);
    }

    private static void invokeStatic(String className, String methodName, Object... args) {
        try {
            ClassLoader loader = agentLoader;
            if (loader == null) return;
            Class<?> clazz = Class.forName(className, true, loader);
            Class<?>[] paramTypes = new Class<?>[args.length];
            for (int i = 0; i < args.length; i++) {
                paramTypes[i] = args[i] == null ? Object.class : args[i].getClass();
                if (paramTypes[i] == Boolean.class) paramTypes[i] = boolean.class;
            }
            var method = clazz.getMethod(methodName, paramTypes);
            method.invoke(null, args);
        } catch (Throwable t) {
            System.err.println("[MyiUI] GameHooks." + methodName + " failed: " + t.getMessage());
        }
    }
}
