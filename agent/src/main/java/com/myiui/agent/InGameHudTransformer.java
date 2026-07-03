package com.myiui.agent;

import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.Opcodes;
import org.objectweb.asm.tree.AbstractInsnNode;
import org.objectweb.asm.tree.ClassNode;
import org.objectweb.asm.tree.InsnList;
import org.objectweb.asm.tree.InsnNode;
import org.objectweb.asm.tree.MethodInsnNode;
import org.objectweb.asm.tree.MethodNode;
import org.objectweb.asm.tree.VarInsnNode;

import java.lang.instrument.ClassFileTransformer;
import java.security.ProtectionDomain;
import java.util.ArrayList;
import java.util.List;

/** Skips vanilla health/hunger drawing; keeps vanilla hotbar items. */
public final class InGameHudTransformer implements ClassFileTransformer {
    @Override
    public byte[] transform(ClassLoader loader, String className, Class<?> classBeingRedefined,
                            ProtectionDomain protectionDomain, byte[] classfileBuffer) {
        if (className == null || !ClassUtil.isInGameHud(className)) {
            return null;
        }
        try {
            ClassReader reader = new ClassReader(classfileBuffer);
            ClassNode node = new ClassNode();
            reader.accept(node, 0);

            int hookedRender = 0;
            int skippedParts = 0;
            List<String> skippedNames = new ArrayList<>();
            for (MethodNode method : node.methods) {
                if (isMainRender(method)) {
                    injectRenderHead(method);
                    hookedRender++;
                } else if (shouldSkipHudPart(method)) {
                    injectEarlyReturn(method);
                    skippedParts++;
                    skippedNames.add(method.name);
                }
            }

            if (hookedRender == 0 && skippedParts == 0) {
                AgentLog.error("InGameHud " + className + ": no hook targets matched");
                return null;
            }

            SafeClassWriter writer = new SafeClassWriter(reader, ClassWriter.COMPUTE_FRAMES | ClassWriter.COMPUTE_MAXS, loader);
            node.accept(writer);
            AgentLog.info("Transformed InGameHud: " + className + " (render=" + hookedRender + ", skip=" + skippedParts
                    + ", skipped=" + String.join(",", skippedNames) + ")");
            return writer.toByteArray();
        } catch (Throwable t) {
            AgentLog.error("InGameHud transform error for " + className, t);
            return null;
        }
    }

    static boolean isMainRender(MethodNode method) {
        if (method.desc == null || !method.desc.startsWith("(L") || !method.desc.endsWith(")V")) {
            return false;
        }
        if (!"render".equals(method.name) && !"method_1753".equals(method.name)) {
            return false;
        }
        // 1.21.6: (DrawContext, float) or (DrawContext, RenderTickCounter)
        return method.desc.endsWith("F)V")
                || method.desc.contains("RenderTickCounter")
                || method.desc.contains("class_9779");
    }

    static boolean shouldSkipHudPart(MethodNode method) {
        if (method.desc == null || !method.desc.endsWith(")V")) {
            return false;
        }
        String name = method.name;
        return "renderStatusBars".equals(name) || "method_1760".equals(name)
                || "renderFood".equals(name) || "method_1756".equals(name)
                || "renderHealthBar".equals(name) || "method_1761".equals(name)
                || "renderArmor".equals(name) || "method_1755".equals(name)
                || "renderMountHealth".equals(name) || "method_1757".equals(name)
                || "renderAirBubbles".equals(name) || "method_1764".equals(name)
                || "renderAir".equals(name) || "method_1765".equals(name)
                || "renderExperienceBar".equals(name) || "method_1758".equals(name)
                || "renderStatusEffectOverlay".equals(name) || "method_1769".equals(name)
                || "renderPlayerList".equals(name) || "method_55804".equals(name);
    }

    private static void injectRenderHead(MethodNode method) {
        AbstractInsnNode first = method.instructions.getFirst();
        if (first == null) {
            return;
        }
        InsnList insns = new InsnList();
        insns.add(new VarInsnNode(Opcodes.ALOAD, 0));
        insns.add(new VarInsnNode(Opcodes.ALOAD, 1));
        if (method.desc != null && method.desc.endsWith("F)V")) {
            insns.add(new VarInsnNode(Opcodes.FLOAD, 2));
        } else {
            insns.add(new InsnNode(Opcodes.FCONST_0));
        }
        insns.add(new MethodInsnNode(
                Opcodes.INVOKESTATIC,
                "com/myiui/agent/HudBridge",
                "onHudRender",
                "(Ljava/lang/Object;Ljava/lang/Object;F)V",
                false));
        method.instructions.insert(first, insns);
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
