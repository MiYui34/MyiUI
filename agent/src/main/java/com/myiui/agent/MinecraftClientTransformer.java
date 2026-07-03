package com.myiui.agent;

import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.tree.AbstractInsnNode;
import org.objectweb.asm.tree.ClassNode;
import org.objectweb.asm.tree.InsnList;
import org.objectweb.asm.tree.MethodInsnNode;
import org.objectweb.asm.tree.MethodNode;
import org.objectweb.asm.tree.VarInsnNode;

import java.lang.instrument.ClassFileTransformer;
import java.security.ProtectionDomain;

public final class MinecraftClientTransformer implements ClassFileTransformer {
    @Override
    public byte[] transform(ClassLoader loader, String className, Class<?> classBeingRedefined,
                            ProtectionDomain protectionDomain, byte[] classfileBuffer) {
        if (className == null || !ClassUtil.isMinecraftClient(className)) {
            return null;
        }
        try {
            ClassReader reader = new ClassReader(classfileBuffer);
            ClassNode node = new ClassNode();
            reader.accept(node, 0);

            int hooked = 0;
            int hookedTick = 0;
            int hookedRender = 0;
            int hookedDisconnect = 0;
            for (MethodNode method : node.methods) {
                if (isSetScreenMethod(method)) {
                    injectSetScreenHook(method);
                    hooked++;
                } else if (isTickMethod(method)) {
                    injectTickTail(method);
                    hookedTick++;
                } else if (isRenderMethod(method)) {
                    // syncMenuWithClient only on tick — avoid double broadcast per frame
                } else if (isDisconnectMethod(method)) {
                    injectDisconnectTail(method);
                    hookedDisconnect++;
                }
            }

            if (hooked == 0) {
                AgentLog.error("MinecraftClient " + className + ": setScreen not matched");
                return null;
            }

            SafeClassWriter writer = new SafeClassWriter(reader, ClassWriter.COMPUTE_FRAMES | ClassWriter.COMPUTE_MAXS, loader);
            node.accept(writer);
            AgentLog.info("Transformed MinecraftClient: " + className + " (setScreen=" + hooked + ", tick=" + hookedTick
                    + ", render=" + hookedRender + ", disconnect=" + hookedDisconnect + ")");
            return writer.toByteArray();
        } catch (Throwable t) {
            AgentLog.error("MinecraftClient transform error for " + className, t);
            return null;
        }
    }

    static boolean isSetScreenMethod(MethodNode method) {
        if (method.desc == null) return false;
        if ("setScreen".equals(method.name) || "method_1507".equals(method.name)) {
            return method.desc.startsWith("(L") && method.desc.endsWith(")V");
        }
        return false;
    }

    static boolean isTickMethod(MethodNode method) {
        return "()V".equals(method.desc) && ("tick".equals(method.name) || "method_1574".equals(method.name));
    }

    static boolean isRenderMethod(MethodNode method) {
        return "(Z)V".equals(method.desc) && ("render".equals(method.name) || "method_1523".equals(method.name));
    }

    static boolean isDisconnectMethod(MethodNode method) {
        if (!"disconnect".equals(method.name)
                && !"method_18099".equals(method.name)
                && !"method_56134".equals(method.name)
                && !"method_18096".equals(method.name)) {
            return false;
        }
        return "()V".equals(method.desc)
                || (method.desc != null && method.desc.startsWith("(L") && method.desc.endsWith(")V"));
    }

    private static void injectDisconnectTail(MethodNode method) {
        AbstractInsnNode ret = findLastReturn(method);
        if (ret == null) {
            return;
        }
        InsnList insns = new InsnList();
        insns.add(new VarInsnNode(org.objectweb.asm.Opcodes.ALOAD, 0));
        insns.add(new MethodInsnNode(
                org.objectweb.asm.Opcodes.INVOKESTATIC,
                "com/myiui/agent/SharedState",
                "onAfterDisconnect",
                "(Ljava/lang/Object;)V",
                false));
        method.instructions.insertBefore(ret, insns);
    }

    private static void injectSetScreenHook(MethodNode method) {
        AbstractInsnNode first = method.instructions.getFirst();
        if (first == null) return;
        InsnList insns = new InsnList();
        insns.add(new VarInsnNode(org.objectweb.asm.Opcodes.ALOAD, 1));
        insns.add(new MethodInsnNode(
                org.objectweb.asm.Opcodes.INVOKESTATIC,
                "com/myiui/agent/SharedState",
                "onSetScreen",
                "(Ljava/lang/Object;)V",
                false));
        method.instructions.insert(first, insns);
        injectSetScreenTail(method);
    }

    private static void injectSetScreenTail(MethodNode method) {
        for (AbstractInsnNode insn : method.instructions) {
            int op = insn.getOpcode();
            if (op < org.objectweb.asm.Opcodes.IRETURN || op > org.objectweb.asm.Opcodes.RETURN) {
                continue;
            }
            InsnList insns = new InsnList();
            insns.add(new VarInsnNode(org.objectweb.asm.Opcodes.ALOAD, 0));
            insns.add(new MethodInsnNode(
                    org.objectweb.asm.Opcodes.INVOKESTATIC,
                    "com/myiui/agent/SharedState",
                    "onSetScreenTail",
                    "(Ljava/lang/Object;)V",
                    false));
            method.instructions.insertBefore(insn, insns);
        }
    }

    private static void injectTickTail(MethodNode method) {
        AbstractInsnNode ret = findLastReturn(method);
        if (ret == null) {
            return;
        }
        InsnList insns = new InsnList();
        insns.add(new VarInsnNode(org.objectweb.asm.Opcodes.ALOAD, 0));
        insns.add(new MethodInsnNode(
                org.objectweb.asm.Opcodes.INVOKESTATIC,
                "com/myiui/agent/SharedState",
                "syncMenuWithClient",
                "(Ljava/lang/Object;)V",
                false));
        insns.add(new VarInsnNode(org.objectweb.asm.Opcodes.ALOAD, 0));
        insns.add(new MethodInsnNode(
                org.objectweb.asm.Opcodes.INVOKESTATIC,
                "com/myiui/agent/IslandBridge",
                "onClientTick",
                "(Ljava/lang/Object;)V",
                false));
        method.instructions.insertBefore(ret, insns);
    }

    private static AbstractInsnNode findLastReturn(MethodNode method) {
        AbstractInsnNode last = null;
        for (AbstractInsnNode insn : method.instructions) {
            int op = insn.getOpcode();
            if (op >= org.objectweb.asm.Opcodes.IRETURN && op <= org.objectweb.asm.Opcodes.RETURN) {
                last = insn;
            }
        }
        return last;
    }
}
