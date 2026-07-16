package com.myiui.mixin;

import com.myiui.state.OverlayConnection;
import net.minecraft.client.gui.Gui;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/**
 * Hides vanilla status bars when the Electron overlay is ready.
 * Method signatures vary across 1.21.x / 26.x — require=0 keeps missing targets soft.
 */
@Mixin(Gui.class)
public abstract class InGameHudMixin {
    @Inject(method = "renderPlayerHealth", at = @At("HEAD"), cancellable = true, require = 0)
    private void myiui$hideHealth(CallbackInfo ci) {
        if (OverlayConnection.shouldSuppressVanillaUi()) {
            ci.cancel();
        }
    }

    @Inject(method = "renderFood", at = @At("HEAD"), cancellable = true, require = 0)
    private void myiui$hideFood(CallbackInfo ci) {
        if (OverlayConnection.shouldSuppressVanillaUi()) {
            ci.cancel();
        }
    }

    @Inject(method = "renderExperienceBar", at = @At("HEAD"), cancellable = true, require = 0)
    private void myiui$hideXp(CallbackInfo ci) {
        if (OverlayConnection.shouldSuppressVanillaUi()) {
            ci.cancel();
        }
    }

    @Inject(method = "renderItemHotbar", at = @At("HEAD"), cancellable = true, require = 0)
    private void myiui$hideHotbar(CallbackInfo ci) {
        if (OverlayConnection.shouldSuppressVanillaUi()) {
            ci.cancel();
        }
    }

    @Inject(method = "renderVehicleHealth", at = @At("HEAD"), cancellable = true, require = 0)
    private void myiui$hideVehicle(CallbackInfo ci) {
        if (OverlayConnection.shouldSuppressVanillaUi()) {
            ci.cancel();
        }
    }
}
