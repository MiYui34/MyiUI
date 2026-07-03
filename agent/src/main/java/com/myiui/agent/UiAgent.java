package com.myiui.agent;

import java.lang.instrument.Instrumentation;

/** @deprecated v1 JDK attach entry — v2 uses {@link Main} via core.dll Preloader. */
@Deprecated
public final class UiAgent {
    private UiAgent() {}

    public static void agentmain(String args, Instrumentation inst) {
        AgentLog.init();
        AgentLog.info("UiAgent.agentmain is deprecated — use v2 DLL injection (Main via Preloader).");
        Main.main(new String[0]);
    }
}
