package com.myiui.mixin;

import com.myiui.state.OverlayConnection;
import net.minecraft.client.gui.screens.TitleScreen;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/**
 * Suppresses vanilla title screen so Electron can draw the modern menu.
 * 26.1 renamed Screen#render → extractRenderState.
 */
@Mixin(TitleScreen.class)
public abstract class TitleScreenMixin {
    //? if <26.1 {
    @Inject(method = "render", at = @At("HEAD"), cancellable = true)
    private void myiui$hideTitle(CallbackInfo ci) {
        if (OverlayConnection.shouldSuppressVanillaUi()) {
            ci.cancel();
        }
    }
    //?} else {
    /*@Inject(method = "extractRenderState", at = @At("HEAD"), cancellable = true, require = 0)
    private void myiui$hideTitle26(CallbackInfo ci) {
        if (OverlayConnection.shouldSuppressVanillaUi()) {
            ci.cancel();
        }
    }
    *///?}
}
