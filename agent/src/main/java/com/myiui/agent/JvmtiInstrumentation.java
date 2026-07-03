package com.myiui.agent;

import java.lang.instrument.ClassFileTransformer;
import java.lang.instrument.IllegalClassFormatException;
import java.security.ProtectionDomain;
import java.util.ArrayList;
import java.util.List;

/** Minimal Instrumentation shim backed by JVMTI natives in core.dll. */
public final class JvmtiInstrumentation {
    private final List<ClassFileTransformer> transformers = new ArrayList<>();

    public void addTransformer(ClassFileTransformer transformer, boolean canRetransform) {
        if (transformer == null) {
            return;
        }
        transformers.add(transformer);
        if (canRetransform) {
            // JVMTI startup enables retransform globally.
        }
    }

    public void retransformClasses(Class<?>... classes) throws Exception {
        AgentLog.info("retransformClasses: " + transformers.size() + " transformers registered");
        for (Class<?> target : classes) {
            if (target == null) {
                continue;
            }
            AgentLog.info("retransformClasses: " + target.getName());
            byte[] bytes = NativeBridge.getClassBytes(target);
            if (bytes == null || bytes.length == 0) {
                AgentLog.info("retransformClasses: no bytes for " + target.getName());
                continue;
            }
            byte[] transformed = applyTransformers(target.getName(), target, bytes);
            if (transformed != null && transformed.length > 0 && transformed != bytes) {
                int err = NativeBridge.redefineClasses(target, transformed);
                if (err != 0) {
                    AgentLog.error("redefineClasses failed for " + target.getName() + " err=" + err);
                } else {
                    AgentLog.info("redefineClasses ok for " + target.getName());
                }
            } else {
                AgentLog.info("retransformClasses: no change for " + target.getName());
            }
        }
    }

    public Class<?>[] getAllLoadedClasses() {
        Class<?>[] jvmClasses = NativeBridge.jvmLoadedClasses();
        if (jvmClasses != null) {
            for (Class<?> c : jvmClasses) {
                LoadedClassesCache.track(c);
            }
        }
        return LoadedClassesCache.snapshot();
    }

    private byte[] applyTransformers(String className, Class<?> classBeingRedefined, byte[] classfileBuffer)
            throws IllegalClassFormatException {
        byte[] current = classfileBuffer;
        ClassLoader loader = classBeingRedefined != null ? classBeingRedefined.getClassLoader() : null;
        String internal = className.replace('.', '/');
        for (ClassFileTransformer transformer : transformers) {
            try {
                byte[] next = transformer.transform(loader, internal, classBeingRedefined, null, current);
                if (next != null) {
                    current = next;
                }
            } catch (Throwable t) {
                AgentLog.error("transformer threw for " + internal + ": " + t.getClass().getName() + ": " + t.getMessage());
            }
        }
        return current;
    }
}
