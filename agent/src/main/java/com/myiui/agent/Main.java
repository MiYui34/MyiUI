package com.myiui.agent;

public final class Main {
    private static final java.util.concurrent.atomic.AtomicBoolean STARTED = new java.util.concurrent.atomic.AtomicBoolean();

    private Main() {}

    public static void main(String[] args) {
        AgentLog.init();
        Runtime.getRuntime().addShutdownHook(new Thread(() -> {
            try {
                com.myiui.agent.netease.NetEaseBridge.shutdown();
            } catch (Throwable ignored) {
            }
        }, "MyiUI-Shutdown"));
        if (!STARTED.compareAndSet(false, true)) {
            AgentLog.info("Agent already started — refreshing transformers");
            try {
                UiAgentHolder.getInstrumentation().retransformClasses(findLoadedForRetransform(UiAgentHolder.getInstrumentation()));
            } catch (Throwable t) {
                AgentLog.error("Agent re-start failed", t);
            }
            return;
        }

        try {
            if (!NativeBridge.startup()) {
                throw new IllegalStateException("NativeBridge.startup failed");
            }

            JvmtiInstrumentation inst = new JvmtiInstrumentation();
            UiAgentHolder.setInstrumentation(inst);

            ClassLoader gameLoader = AgentExposure.resolveGameLoader(inst);
            if (gameLoader == null) {
                AgentLog.error("Game ClassLoader missing");
            } else {
                AgentLog.info("Game ClassLoader: " + gameLoader.getClass().getName());
                AgentExposure.verifyLoad(gameLoader, "com.myiui.agent.SharedState");
            }

        GameActions.init(inst);
        SharedState.init();
        VideoBackground.start();
        com.myiui.agent.netease.NetEaseBridge.init();

            inst.addTransformer(new TitleScreenTransformer(), true);
            inst.addTransformer(new SodiumScreenTransformer(), true);
            inst.addTransformer(new MinecraftClientTransformer(), true);
        inst.addTransformer(new InGameHudTransformer(), true);
        inst.addTransformer(new PlayerListHudTransformer(), true);

            Class<?>[] retransform = findLoadedForRetransform(inst);
            if (retransform.length == 0) {
                AgentLog.info("No game classes loaded yet for retransform.");
            } else {
                AgentLog.info("retransformClasses: " + retransform.length + " classes to retransform");
                inst.retransformClasses(retransform);
                AgentLog.info("Retransformed " + retransform.length + " classes.");
            }
            AgentLog.info("About to call AttachBootstrap.onAttached...");
            AttachBootstrap.onAttached(inst);
            AgentLog.info("AttachBootstrap.onAttached returned.");
            AgentLog.info("MyiUI agent ready (v2 JVMTI)");
        } catch (Throwable t) {
            AgentLog.error("Agent startup failed", t);
            if (t instanceof RuntimeException re) {
                throw re;
            }
            if (t instanceof Error err) {
                throw err;
            }
            throw new RuntimeException(t);
        }
    }

    private static Class<?>[] findLoadedForRetransform(JvmtiInstrumentation inst) {
        java.util.List<Class<?>> out = new java.util.ArrayList<>();
        addIfLoaded(inst, out, "net.minecraft.client.gui.screen.TitleScreen");
        addIfLoaded(inst, out, "net.minecraft.class_442");
        addIfLoaded(inst, out, "net.minecraft.client.MinecraftClient");
        addIfLoaded(inst, out, "net.minecraft.class_310");
        addIfLoaded(inst, out, "net.minecraft.client.gui.hud.InGameHud");
        addIfLoaded(inst, out, "net.minecraft.class_329");
        addIfLoaded(inst, out, "net.minecraft.client.gui.hud.PlayerListHud");
        addIfLoaded(inst, out, "net.minecraft.class_355");
        addIfLoaded(inst, out, "net.caffeinemc.mods.sodium.client.gui.SodiumOptionsGUI");
        addIfLoaded(inst, out, "me.jellysquid.mods.sodium.client.gui.SodiumOptionsGUI");
        return out.toArray(Class[]::new);
    }

    private static void addIfLoaded(JvmtiInstrumentation inst, java.util.List<Class<?>> out, String className) {
        for (Class<?> c : inst.getAllLoadedClasses()) {
            if (className.equals(c.getName()) && !out.contains(c)) {
                out.add(c);
                return;
            }
        }
    }
}
