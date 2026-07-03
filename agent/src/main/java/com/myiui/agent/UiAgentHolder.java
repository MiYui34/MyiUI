package com.myiui.agent;

final class UiAgentHolder {
    private static JvmtiInstrumentation instrumentation;

    private UiAgentHolder() {}

    static void setInstrumentation(JvmtiInstrumentation inst) {
        instrumentation = inst;
    }

    static JvmtiInstrumentation getInstrumentation() {
        return instrumentation;
    }

    static JvmtiInstrumentation getJvmtiInstrumentation() {
        return instrumentation;
    }
}
