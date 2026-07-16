package com.myiui.bridge;

import com.google.gson.JsonObject;
import com.myiui.netease.NetEaseClient;
import com.myiui.ws.Protocol;
import net.minecraft.client.Minecraft;

/**
 * Proxies NetEase Cloud Music API commands to the local api-enhanced service.
 */
public final class NetEaseBridge {
    private static final NetEaseClient CLIENT = new NetEaseClient();

    private NetEaseBridge() {}

    public static JsonObject dispatch(Minecraft client, String cmd, JsonObject data) {
        return switch (cmd) {
            case "NE_API_STATUS" -> CLIENT.apiStatus();
            case "NE_LOGIN_STATUS" -> CLIENT.loginStatus();
            case "NE_QR_START" -> CLIENT.qrStart();
            case "NE_QR_POLL" -> CLIENT.qrPoll(data);
            case "NE_QR_IMAGE" -> CLIENT.qrImage(data);
            case "NE_QR_CANCEL" -> CLIENT.qrCancel();
            case "NE_LOGOUT" -> CLIENT.logout();
            case "NE_SEARCH" -> CLIENT.search(data);
            case "NE_MY_PLAYLISTS" -> CLIENT.myPlaylists();
            case "NE_PLAYLIST_TRACKS" -> CLIENT.playlistTracks(data);
            case "NE_PLAY_STATUS" -> CLIENT.playStatus();
            case "NE_PLAY_SONG" -> CLIENT.playSong(data);
            case "NE_PLAY_PAUSE" -> CLIENT.pause();
            case "NE_PLAY_RESUME" -> CLIENT.resume();
            case "NE_PLAY_STOP" -> CLIENT.stop();
            case "NE_PLAY_NEXT" -> CLIENT.next();
            case "NE_PLAY_PREV" -> CLIENT.prev();
            case "NE_PLAY_SEEK" -> CLIENT.seek(data);
            case "NE_SET_VOLUME" -> CLIENT.setVolume(data);
            case "NE_LYRICS" -> CLIENT.lyrics(data);
            case "NE_DAILY_RECOMMEND" -> CLIENT.dailyRecommend();
            default -> Protocol.obj().add("unknown", true).add("cmd", cmd).build();
        };
    }

    public static NetEaseClient client() {
        return CLIENT;
    }
}
