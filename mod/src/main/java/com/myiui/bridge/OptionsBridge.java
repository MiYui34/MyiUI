package com.myiui.bridge;

import com.google.gson.JsonObject;
import com.myiui.ws.Protocol;
import net.minecraft.client.Minecraft;
import net.minecraft.client.Options;

/**
 * Lightweight options bridge — reads/writes a subset of vanilla Options.
 */
public final class OptionsBridge {
    private OptionsBridge() {}

    public static JsonObject dispatch(Minecraft client, String cmd, JsonObject data) {
        Options options = client.options;
        if (cmd.startsWith("GET_OPTIONS")) {
            JsonObject out = new JsonObject();
            out.addProperty("fov", options.fov().get());
            out.addProperty("renderDistance", options.renderDistance().get());
            out.addProperty("simulationDistance", options.simulationDistance().get());
            out.addProperty("guiScale", options.guiScale().get());
            out.addProperty("fullscreen", options.fullscreen().get());
            out.addProperty("musicVolume", options.getSoundSourceVolume(net.minecraft.sounds.SoundSource.MUSIC));
            out.addProperty("masterVolume", options.getSoundSourceVolume(net.minecraft.sounds.SoundSource.MASTER));
            return out;
        }
        if (cmd.startsWith("SET_OPTION")) {
            String key = data.has("key") ? data.get("key").getAsString() : "";
            if ("fov".equals(key) && data.has("value")) {
                options.fov().set(data.get("value").getAsInt());
            } else if ("guiScale".equals(key) && data.has("value")) {
                options.guiScale().set(data.get("value").getAsInt());
            } else if ("renderDistance".equals(key) && data.has("value")) {
                options.renderDistance().set(data.get("value").getAsInt());
            }
            options.save();
            return Protocol.obj().add("saved", true).build();
        }
        return Protocol.obj().add("unsupported", true).build();
    }
}
