package com.myiui.agent;


public final class AttachBootstrap {
    private AttachBootstrap() {}

    public static void onAttached(JvmtiInstrumentation inst) {
        AgentLog.info("AttachBootstrap.onAttached enter");
        try {
            Class<?> mcClass = findLoadedMinecraftClient(inst);
            if (mcClass == null) {
                AgentLog.info("MinecraftClient not loaded yet — TitleScreen hooks apply on next screen visit.");
                return;
            }
            AgentLog.info("AttachBootstrap: mcClass=" + mcClass.getName());

            var getInstance = ReflectUtil.findStaticMethod(mcClass, "getInstance", "method_1551");
            Object client = getInstance.invoke(null);
            if (client == null) {
                AgentLog.info("MinecraftClient instance is null.");
                return;
            }

            Object screen = ReflectUtil.getCurrentScreen(client);
            AgentLog.info("Attach bootstrap screen: " + (screen == null ? "null" : screen.getClass().getName()));
            SharedState.broadcastScreen(screen);
            if (ClassUtil.isTitleScreenInstance(screen)) {
                AgentLog.info("已进入主菜单 — 隐藏原版控件并启用 MyiUI 覆盖层");
                SharedState.onTitleScreenOpened(screen);
            }
        } catch (Throwable t) {
            AgentLog.error("AttachBootstrap failed", t);
        }
    }

    private static Class<?> findLoadedMinecraftClient(JvmtiInstrumentation inst) {
        Class<?> named = findLoadedClass(inst, "net.minecraft.client.MinecraftClient");
        if (named != null) return named;
        return findLoadedClass(inst, "net.minecraft.class_310");
    }

    private static Class<?> findLoadedClass(JvmtiInstrumentation inst, String className) {
        for (Class<?> c : inst.getAllLoadedClasses()) {
            if (className.equals(c.getName())) {
                return c;
            }
        }
        return null;
    }
}
