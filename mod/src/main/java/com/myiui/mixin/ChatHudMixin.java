package com.myiui.mixin;

import com.myiui.bridge.ChatBridge;
import com.myiui.state.OverlayConnection;
import net.minecraft.client.gui.components.ChatComponent;
import net.minecraft.network.chat.Component;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/**
 * Forwards chat lines to Electron and optionally hides vanilla chat.
 */
@Mixin(ChatComponent.class)
public abstract class ChatHudMixin {
    @Inject(method = "addMessage(Lnet/minecraft/network/chat/Component;)V", at = @At("TAIL"), require = 0)
    private void myiui$captureSimple(Component message, CallbackInfo ci) {
        ChatBridge.onMessage(message);
    }

    @Inject(method = "render", at = @At("HEAD"), cancellable = true, require = 0)
    private void myiui$hideChat(CallbackInfo ci) {
        if (OverlayConnection.shouldSuppressVanillaUi() && ChatBridge.isElectronChatEnabled()) {
            ci.cancel();
        }
    }
}
