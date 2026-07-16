package com.myiui.mixin;

import com.myiui.state.OverlayConnection;
import net.minecraft.client.gui.components.PlayerTabOverlay;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/**
 * Tab list is drawn by the Electron Dynamic Island instead.
 */
@Mixin(PlayerTabOverlay.class)
public abstract class PlayerListHudMixin {
    @Inject(method = "render", at = @At("HEAD"), cancellable = true, require = 0)
    private void myiui$hideTab(CallbackInfo ci) {
        if (OverlayConnection.shouldSuppressVanillaUi()) {
            ci.cancel();
        }
    }
}
