package com.myiui.agent;

import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.Opcodes;
import org.objectweb.asm.tree.AbstractInsnNode;
import org.objectweb.asm.tree.ClassNode;
import org.objectweb.asm.tree.InsnList;
import org.objectweb.asm.tree.MethodInsnNode;
import org.objectweb.asm.tree.MethodNode;
import org.objectweb.asm.tree.VarInsnNode;

import java.lang.instrument.ClassFileTransformer;
import java.security.ProtectionDomain;

/** Ensure MyiUI menu state recovers after Sodium options closes. */
public final class SodiumScreenTransformer implements ClassFileTransformer {
    @Override
    public byte[] transform(ClassLoader loader, String className, Class<?> classBeingRedefined,
                            ProtectionDomain protectionDomain, byte[] classfileBuffer) {
        if (className == null || !className.contains("SodiumOptionsGUI")) {
            return null;
        }
        try {
            ClassReader reader = new ClassReader(classfileBuffer);
            ClassNode node = new ClassNode();
            reader.accept(node, 0);

            int hooked = 0;
            for (MethodNode method : node.methods) {
                if (!shouldHookSodiumExit(method)) {
                    continue;
                }
                injectCloseTail(method);
                hooked++;
            }

            if (hooked == 0) {
                return null;
            }

            SafeClassWriter writer = new SafeClassWriter(reader, ClassWriter.COMPUTE_FRAMES | ClassWriter.COMPUTE_MAXS, loader);
            node.accept(writer);
            AgentLog.info("Transformed Sodium screen: " + className + " (exit=" + hooked + ")");
            return writer.toByteArray();
        } catch (Throwable t) {
            AgentLog.error("Sodium screen transform error for " + className, t);
            return null;
        }
    }

    private static boolean shouldHookSodiumExit(MethodNode method) {
        if (method.desc == null || !"()V".equals(method.desc)) {
            return false;
        }
        if ("onClose".equals(method.name) || "close".equals(method.name)
                || "method_25419".equals(method.name)) {
            return true;
        }
        return callsClientSetScreen(method);
    }

    private static boolean callsClientSetScreen(MethodNode method) {
        for (AbstractInsnNode insn : method.instructions) {
            if (!(insn instanceof MethodInsnNode min)) {
                continue;
            }
            if ("setScreen".equals(min.name) || "method_1507".equals(min.name)) {
                return true;
            }
        }
        return false;
    }

    private static void injectCloseTail(MethodNode method) {
        AbstractInsnNode ret = findLastReturn(method);
        if (ret == null) {
            return;
        }
        InsnList insns = new InsnList();
        insns.add(new VarInsnNode(Opcodes.ALOAD, 0));
        insns.add(new MethodInsnNode(
                Opcodes.INVOKESTATIC,
                "com/myiui/agent/SharedState",
                "onSubScreenClosedFromScreen",
                "(Ljava/lang/Object;)V",
                false));
        method.instructions.insertBefore(ret, insns);
    }

    private static AbstractInsnNode findLastReturn(MethodNode method) {
        AbstractInsnNode last = null;
        for (AbstractInsnNode insn : method.instructions) {
            int op = insn.getOpcode();
            if (op >= Opcodes.IRETURN && op <= Opcodes.RETURN) {
                last = insn;
            }
        }
        return last;
    }
}
