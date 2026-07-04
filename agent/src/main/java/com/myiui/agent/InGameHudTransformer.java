package com.myiui.agent;

import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.Opcodes;
import org.objectweb.asm.tree.AbstractInsnNode;
import org.objectweb.asm.tree.ClassNode;
import org.objectweb.asm.tree.InsnList;
import org.objectweb.asm.tree.InsnNode;
import org.objectweb.asm.tree.JumpInsnNode;
import org.objectweb.asm.tree.LabelNode;
import org.objectweb.asm.tree.MethodInsnNode;
import org.objectweb.asm.tree.MethodNode;
import org.objectweb.asm.tree.VarInsnNode;

import com.myiui.agent.mapping.Mappings;

import java.lang.instrument.ClassFileTransformer;
import java.security.ProtectionDomain;
import java.util.ArrayList;
import java.util.List;

/** Hooks HUD data collection; optional chat skip for overlay. */
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
                } else if (isConditionalChat(method)) {
                    injectFlaggedReturn(method, "isChatVisible", true);
                    skippedParts++;
                    skippedNames.add(method.name + "?chat");
                }
            }

            if (hookedRender == 0) {
                AgentLog.error("InGameHud " + className + ": main render hook NOT matched (skip=" + skippedParts
                        + "); dumping methods for mapping diagnosis:");
                dumpMethods(node);
                if (skippedParts == 0) {
                    return null;
                }
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
        if (!Mappings.matchesAny(Mappings.HUD_RENDER_METHOD, method.name)) {
            return false;
        }
        // render(DrawContext, RenderTickCounter)V or the legacy render(DrawContext, ..., float)V.
        return method.desc.endsWith("F)V")
                || Mappings.descContainsAny(Mappings.RENDER_TICK_COUNTER_MARKERS, method.desc);
    }

    static boolean shouldSkipHudPart(MethodNode method) {
        if (method.desc == null || !method.desc.endsWith(")V")) {
            return false;
        }
        // renderStatusEffectOverlay exists only on <=1.21.1 (HUD refactored afterward); skipping it
        // there prevents vanilla status-effect icons overlapping our overlay. On newer versions the
        // method is absent and we simply don't skip (harmless cosmetic). renderPlayerList is stable.
        return Mappings.matchesAny(Mappings.HUD_RENDER_STATUS_EFFECTS_METHOD, method.name)
                || Mappings.matchesAny(Mappings.HUD_RENDER_PLAYER_LIST_METHOD, method.name);
    }

    static boolean isConditionalChat(MethodNode method) {
        if (method.desc == null || !method.desc.endsWith(")V")) {
            return false;
        }
        return Mappings.matchesAny(Mappings.HUD_RENDER_CHAT_METHOD, method.name);
    }

    private static void dumpMethods(ClassNode node) {
        StringBuilder sb = new StringBuilder("  methods: ");
        int n = 0;
        for (MethodNode m : node.methods) {
            sb.append(m.name).append(m.desc).append(' ');
            if (++n >= 80) break;
        }
        AgentLog.error(sb.toString());
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

    /** Skip method body when flag getter returns true (chat). */
    private static void injectFlaggedReturn(MethodNode method, String getter) {
        injectFlaggedReturn(method, getter, false);
    }

    private static void injectFlaggedReturn(MethodNode method, String getter, boolean invert) {
        AbstractInsnNode first = method.instructions.getFirst();
        if (first == null) {
            return;
        }
        LabelNode continueLabel = new LabelNode();
        InsnList insns = new InsnList();
        insns.add(new MethodInsnNode(
                Opcodes.INVOKESTATIC,
                "com/myiui/agent/OverlayUiBridge",
                getter,
                "()Z",
                false));
        if (invert) {
            insns.add(new JumpInsnNode(Opcodes.IFNE, continueLabel));
        } else {
            insns.add(new JumpInsnNode(Opcodes.IFEQ, continueLabel));
        }
        insns.add(new InsnNode(Opcodes.RETURN));
        insns.add(continueLabel);
        method.instructions.insert(first, insns);
    }
}
