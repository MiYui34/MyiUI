package com.myiui.agent;

import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.Label;
import org.objectweb.asm.Opcodes;
import org.objectweb.asm.tree.AbstractInsnNode;
import org.objectweb.asm.tree.ClassNode;
import org.objectweb.asm.tree.InsnList;
import org.objectweb.asm.tree.MethodInsnNode;
import org.objectweb.asm.tree.MethodNode;
import org.objectweb.asm.tree.JumpInsnNode;
import org.objectweb.asm.tree.LabelNode;
import org.objectweb.asm.tree.VarInsnNode;

import com.myiui.agent.mapping.Mappings;

import java.lang.instrument.ClassFileTransformer;
import java.security.ProtectionDomain;

public final class TitleScreenTransformer implements ClassFileTransformer {
    @Override
    public byte[] transform(ClassLoader loader, String className, Class<?> classBeingRedefined,
                            ProtectionDomain protectionDomain, byte[] classfileBuffer) {
        AgentLog.info("TitleScreenTransformer.transform: className=" + className + " bufferLen=" + (classfileBuffer == null ? 0 : classfileBuffer.length));
        if (className == null || !ClassUtil.isTitleScreen(className)) {
            AgentLog.info("TitleScreenTransformer: isTitleScreen=false for " + className);
            return null;
        }
        AgentLog.info("TitleScreenTransformer: isTitleScreen=true for " + className);
        try {
            ClassReader reader = new ClassReader(classfileBuffer);
            ClassNode node = new ClassNode();
            reader.accept(node, 0);

            int hookedInit = 0;
            int hookedRender = 0;
            int hookedOnDisplayed = 0;
            for (MethodNode method : node.methods) {
                if (isInitMethod(method)) {
                    injectInitTail(method);
                    hookedInit++;
                } else if (isOnDisplayedMethod(method)) {
                    injectOnDisplayedTail(method);
                    hookedOnDisplayed++;
                } else if (isRenderMethod(method)) {
                    injectRenderSkip(method);
                    hookedRender++;
                }
            }

            if (hookedInit == 0 && hookedRender == 0 && hookedOnDisplayed == 0) {
                AgentLog.error("TitleScreen " + className + ": no init/render/onDisplayed methods matched");
                return null;
            }

            SafeClassWriter writer = new SafeClassWriter(reader, ClassWriter.COMPUTE_FRAMES | ClassWriter.COMPUTE_MAXS, loader);
            node.accept(writer);
            AgentLog.info("Transformed TitleScreen: " + className + " (init=" + hookedInit + ", render=" + hookedRender
                    + ", onDisplayed=" + hookedOnDisplayed + ")");
            return writer.toByteArray();
        } catch (Throwable t) {
            AgentLog.error("TitleScreen transform error for " + className, t);
            return null;
        }
    }

    static boolean isInitMethod(MethodNode method) {
        if (!"()V".equals(method.desc)) return false;
        if ("<init>".equals(method.name) || "<clinit>".equals(method.name)) return false;
        return Mappings.matchesAny(Mappings.TITLE_INIT_METHOD, method.name);
    }

    static boolean isOnDisplayedMethod(MethodNode method) {
        if (!"()V".equals(method.desc)) {
            return false;
        }
        return Mappings.matchesAny(Mappings.TITLE_ON_DISPLAYED_METHOD, method.name);
    }

    static boolean isRenderMethod(MethodNode method) {
        if (method.desc == null || !method.desc.endsWith("IIF)V") || !method.desc.startsWith("(L")) {
            return false;
        }
        if (Mappings.matchesAny(Mappings.SCREEN_RENDER_BACKGROUND_METHOD, method.name)) {
            return false;
        }
        return Mappings.matchesAny(Mappings.TITLE_RENDER_METHOD, method.name);
    }

    private static void injectInitTail(MethodNode method) {
        injectTitleScreenOpenedCall(method);
    }

    private static void injectOnDisplayedTail(MethodNode method) {
        injectTitleScreenOpenedCall(method);
    }

    private static void injectTitleScreenOpenedCall(MethodNode method) {
        AbstractInsnNode ret = findLastReturn(method);
        if (ret == null) {
            return;
        }
        InsnList insns = new InsnList();
        insns.add(new VarInsnNode(Opcodes.ALOAD, 0));
        insns.add(new MethodInsnNode(
                Opcodes.INVOKESTATIC,
                "com/myiui/agent/SharedState",
                "onTitleScreenOpened",
                "(Ljava/lang/Object;)V",
                false));
        method.instructions.insertBefore(ret, insns);
    }

    private static void injectRenderSkip(MethodNode method) {
        LabelNode renderVanilla = new LabelNode();
        InsnList insns = new InsnList();
        insns.add(new MethodInsnNode(
                Opcodes.INVOKESTATIC,
                "com/myiui/agent/SharedState",
                "shouldSkipVanillaTitleRender",
                "()Z",
                false));
        insns.add(new JumpInsnNode(Opcodes.IFEQ, renderVanilla));
        insns.add(new MethodInsnNode(
                Opcodes.INVOKESTATIC,
                "com/myiui/agent/SharedState",
                "onTitleScreenRender",
                "()V",
                false));
        insns.add(new org.objectweb.asm.tree.InsnNode(Opcodes.RETURN));
        insns.add(renderVanilla);
        method.instructions.insert(insns);
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
