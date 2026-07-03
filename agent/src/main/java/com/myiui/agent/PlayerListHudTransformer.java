package com.myiui.agent;

import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.Opcodes;
import org.objectweb.asm.tree.AbstractInsnNode;
import org.objectweb.asm.tree.ClassNode;
import org.objectweb.asm.tree.InsnList;
import org.objectweb.asm.tree.InsnNode;
import org.objectweb.asm.tree.MethodNode;

import java.lang.instrument.ClassFileTransformer;
import java.security.ProtectionDomain;

/** Skips vanilla Tab player-list rendering; overlay draws the Dynamic Island list instead. */
public final class PlayerListHudTransformer implements ClassFileTransformer {
    @Override
    public byte[] transform(ClassLoader loader, String className, Class<?> classBeingRedefined,
                            ProtectionDomain protectionDomain, byte[] classfileBuffer) {
        if (className == null || !ClassUtil.isPlayerListHud(className)) {
            return null;
        }
        if (classfileBuffer == null || classfileBuffer.length < 8) {
            AgentLog.error("PlayerListHud " + className + ": invalid class bytes");
            return null;
        }
        try {
            ClassReader reader = new ClassReader(classfileBuffer);
            ClassNode node = new ClassNode();
            reader.accept(node, 0);

            int skipped = 0;
            for (MethodNode method : node.methods) {
                if (shouldSkipRender(method)) {
                    injectEarlyReturn(method);
                    skipped++;
                }
            }

            if (skipped == 0) {
                AgentLog.error("PlayerListHud " + className + ": no render method matched");
                return null;
            }

            SafeClassWriter writer = new SafeClassWriter(reader, ClassWriter.COMPUTE_FRAMES | ClassWriter.COMPUTE_MAXS, loader);
            node.accept(writer);
            AgentLog.info("Transformed PlayerListHud: " + className + " (skip=" + skipped + ")");
            return writer.toByteArray();
        } catch (Throwable t) {
            AgentLog.error("PlayerListHud transform error for " + className, t);
            return null;
        }
    }

    static boolean shouldSkipRender(MethodNode method) {
        if (method.desc == null || !method.desc.contains("Lnet/minecraft/class_332;")) {
            return false;
        }
        String name = method.name;
        return "render".equals(name) || "method_1919".equals(name);
    }

    private static void injectEarlyReturn(MethodNode method) {
        AbstractInsnNode first = method.instructions.getFirst();
        if (first == null) {
            return;
        }
        InsnList insns = new InsnList();
        insns.add(new InsnNode(Opcodes.RETURN));
        method.instructions.insert(first, insns);
    }
}
