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

/** Fallback: activate menu when TitleScreen goes through base Screen.render. */
public final class ScreenTransformer implements ClassFileTransformer {
    @Override
    public byte[] transform(ClassLoader loader, String className, Class<?> classBeingRedefined,
                            ProtectionDomain protectionDomain, byte[] classfileBuffer) {
        if (className == null || !ClassUtil.isScreen(className)) {
            return null;
        }
        try {
            ClassReader reader = new ClassReader(classfileBuffer);
            ClassNode node = new ClassNode();
            reader.accept(node, 0);

            int hookedRender = 0;
            for (MethodNode method : node.methods) {
                if (TitleScreenTransformer.isRenderMethod(method)) {
                    injectScreenRenderHead(method);
                    hookedRender++;
                }
            }

            if (hookedRender == 0) {
                return null;
            }

            SafeClassWriter writer = new SafeClassWriter(reader, ClassWriter.COMPUTE_FRAMES | ClassWriter.COMPUTE_MAXS, loader);
            node.accept(writer);
            AgentLog.info("Transformed Screen: " + className + " (render=" + hookedRender + ")");
            return writer.toByteArray();
        } catch (Throwable t) {
            AgentLog.error("Screen transform error for " + className, t);
            return null;
        }
    }

    private static void injectScreenRenderHead(MethodNode method) {
        InsnList insns = new InsnList();
        insns.add(new VarInsnNode(Opcodes.ALOAD, 0));
        insns.add(new MethodInsnNode(
                Opcodes.INVOKESTATIC,
                "com/myiui/agent/SharedState",
                "onScreenRender",
                "(Ljava/lang/Object;)V",
                false));
        method.instructions.insert(insns);
    }
}
