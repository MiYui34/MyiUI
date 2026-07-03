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

/** Intercept ChatHud: capture addMessage + suppress vanilla render. */
public final class ChatHudTransformer implements ClassFileTransformer {
    @Override
    public byte[] transform(ClassLoader loader, String className, Class<?> classBeingRedefined,
                            ProtectionDomain protectionDomain, byte[] classfileBuffer) {
        if (className == null || !isChatHud(className)) {
            return null;
        }
        try {
            ClassReader reader = new ClassReader(classfileBuffer);
            ClassNode node = new ClassNode();
            reader.accept(node, 0);

            AgentLog.info("ChatHud matched: " + className + " super=" + node.superName + " interfaces=" + node.interfaces);

            int addHooked = 0;
            int renderHooked = 0;

            for (MethodNode method : node.methods) {
                AgentLog.info("  ChatHud method: " + method.name + method.desc);

                if (isAddMessageLike(method)) {
                    injectAddMessageHook(method);
                    addHooked++;
                    AgentLog.info("  -> hooked addMessage");
                } else if (isRenderMethod(method)) {
                    injectRenderSuppress(method);
                    renderHooked++;
                    AgentLog.info("  -> suppressed render");
                }
            }

            if (addHooked == 0 && renderHooked == 0) {
                AgentLog.info("ChatHud " + className + ": no methods matched (add=0 render=0)");
                return null;
            }

            SafeClassWriter writer = new SafeClassWriter(reader, ClassWriter.COMPUTE_FRAMES | ClassWriter.COMPUTE_MAXS, loader);
            node.accept(writer);
            AgentLog.info("Transformed ChatHud: " + className + " (add=" + addHooked + ", render=" + renderHooked + ")");
            return writer.toByteArray();
        } catch (Throwable t) {
            AgentLog.error("ChatHud transform error for " + className, t);
            return null;
        }
    }

    private static boolean isChatHud(String className) {
        // ChatHud in 1.21.x Yarn intermediary = class_340
        // Use exact match to avoid false positives on class_3340, class_3401, etc.
        return className.equals("net/minecraft/class_340")
                || className.equals("net/minecraft/client/gui/hud/ChatHud")
                || (className.contains("ChatHud") && !className.contains("ChatHudLine"));
    }

    /** Match any void method whose name suggests adding a message and takes at least one object param. */
    private static boolean isAddMessageLike(MethodNode method) {
        if (method.desc == null || !method.desc.endsWith(")V")) return false;
        if (method.name.startsWith("<")) return false; // skip constructors
        String name = method.name;
        // Match addMessage, method_1805, or any method with "add" in name taking an object
        if (name.contains("add") || "method_1805".equals(name)) {
            return method.desc.startsWith("(L");
        }
        // Fallback: any method taking a Text (class_2561) as first param
        if (method.desc.startsWith("(Lnet/minecraft/class_2561;")) {
            return true;
        }
        return false;
    }

    /** Match the render method to suppress vanilla chat drawing. */
    private static boolean isRenderMethod(MethodNode method) {
        if (method.desc == null) return false;
        String name = method.name;
        if (name.startsWith("<")) return false;
        if (name.contains("add")) return false;
        return (name.contains("render") || name.contains("draw")) && method.desc.endsWith(")V");
    }

    private static void injectAddMessageHook(MethodNode method) {
        AbstractInsnNode first = method.instructions.getFirst();
        if (first == null) return;
        InsnList insns = new InsnList();
        insns.add(new VarInsnNode(Opcodes.ALOAD, 1));
        insns.add(new MethodInsnNode(
                Opcodes.INVOKESTATIC,
                "com/myiui/agent/ChatManager",
                "captureMessage",
                "(Ljava/lang/Object;)V",
                false));
        method.instructions.insertBefore(first, insns);
    }

    /** Inject RETURN at method start to suppress vanilla chat rendering. */
    private static void injectRenderSuppress(MethodNode method) {
        AbstractInsnNode first = method.instructions.getFirst();
        if (first == null) return;
        InsnList insns = new InsnList();
        insns.add(new InsnNode(Opcodes.RETURN));
        method.instructions.insertBefore(first, insns);
    }
}
