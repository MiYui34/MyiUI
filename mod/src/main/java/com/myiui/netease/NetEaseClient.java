package com.myiui.netease;

import com.google.gson.JsonObject;
import com.google.gson.JsonParser;
import com.myiui.ws.Protocol;

import java.net.URI;
import java.net.URLEncoder;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.nio.charset.StandardCharsets;
import java.time.Duration;

/**
 * Thin HTTP client for api-enhanced (default http://127.0.0.1:3000).
 */
public final class NetEaseClient {
    private final HttpClient http = HttpClient.newBuilder()
            .connectTimeout(Duration.ofSeconds(3))
            .build();
    private String baseUrl = "http://127.0.0.1:3000";
    private String qrKey = "";

    // Local playback state mirrored for overlay
    private boolean playing;
    private String songId = "";
    private String songName = "";
    private String artist = "";
    private String coverUrl = "";
    private double progress;
    private double duration;
    private int volume = 80;

    public JsonObject apiStatus() {
        try {
            JsonObject body = get("/login/status");
            return Protocol.obj().add("online", true).add("raw", body).build();
        } catch (Exception e) {
            return Protocol.obj().add("online", false).add("error", e.getMessage()).build();
        }
    }

    public JsonObject loginStatus() {
        try {
            return get("/login/status");
        } catch (Exception e) {
            return Protocol.obj().add("error", e.getMessage()).build();
        }
    }

    public JsonObject qrStart() {
        try {
            JsonObject key = get("/login/qr/key?timestamp=" + System.currentTimeMillis());
            qrKey = key.getAsJsonObject("data").get("unikey").getAsString();
            JsonObject create = get("/login/qr/create?key=" + enc(qrKey) + "&qrimg=true");
            return Protocol.obj().add("key", qrKey).add("qr", create).build();
        } catch (Exception e) {
            return Protocol.obj().add("error", e.getMessage()).build();
        }
    }

    public JsonObject qrPoll(JsonObject data) {
        try {
            String key = data.has("key") ? data.get("key").getAsString() : qrKey;
            return get("/login/qr/check?key=" + enc(key) + "&timestamp=" + System.currentTimeMillis());
        } catch (Exception e) {
            return Protocol.obj().add("error", e.getMessage()).build();
        }
    }

    public JsonObject qrImage(JsonObject data) {
        return qrStart();
    }

    public JsonObject qrCancel() {
        qrKey = "";
        return Protocol.obj().add("ok", true).build();
    }

    public JsonObject logout() {
        try {
            return get("/logout");
        } catch (Exception e) {
            return Protocol.obj().add("error", e.getMessage()).build();
        }
    }

    public JsonObject search(JsonObject data) {
        try {
            String q = data.has("q") ? data.get("q").getAsString() : "";
            return get("/cloudsearch?keywords=" + enc(q));
        } catch (Exception e) {
            return Protocol.obj().add("error", e.getMessage()).build();
        }
    }

    public JsonObject myPlaylists() {
        try {
            JsonObject status = get("/login/status");
            long uid = status.getAsJsonObject("data").getAsJsonObject("profile").get("userId").getAsLong();
            return get("/user/playlist?uid=" + uid);
        } catch (Exception e) {
            return Protocol.obj().add("error", e.getMessage()).build();
        }
    }

    public JsonObject playlistTracks(JsonObject data) {
        try {
            String id = data.has("id") ? data.get("id").getAsString() : "";
            return get("/playlist/track/all?id=" + enc(id));
        } catch (Exception e) {
            return Protocol.obj().add("error", e.getMessage()).build();
        }
    }

    public JsonObject playStatus() {
        return Protocol.obj()
                .add("playing", playing)
                .add("songId", songId)
                .add("songName", songName)
                .add("artist", artist)
                .add("coverUrl", coverUrl)
                .add("progress", progress)
                .add("duration", duration)
                .add("volume", volume)
                .build();
    }

    public JsonObject playSong(JsonObject data) {
        songId = data.has("id") ? data.get("id").getAsString() : songId;
        songName = data.has("name") ? data.get("name").getAsString() : songName;
        artist = data.has("artist") ? data.get("artist").getAsString() : artist;
        coverUrl = data.has("coverUrl") ? data.get("coverUrl").getAsString() : coverUrl;
        playing = true;
        pushMusicHud();
        return playStatus();
    }

    public JsonObject pause() {
        playing = false;
        pushMusicHud();
        return playStatus();
    }

    public JsonObject resume() {
        playing = true;
        pushMusicHud();
        return playStatus();
    }

    public JsonObject stop() {
        playing = false;
        progress = 0;
        pushMusicHud();
        return playStatus();
    }

    public JsonObject next() {
        return playStatus();
    }

    public JsonObject prev() {
        return playStatus();
    }

    public JsonObject seek(JsonObject data) {
        if (data.has("progress")) {
            progress = data.get("progress").getAsDouble();
        }
        pushMusicHud();
        return playStatus();
    }

    public JsonObject setVolume(JsonObject data) {
        if (data.has("volume")) {
            volume = data.get("volume").getAsInt();
        }
        return playStatus();
    }

    public JsonObject lyrics(JsonObject data) {
        try {
            String id = data.has("id") ? data.get("id").getAsString() : songId;
            return get("/lyric?id=" + enc(id));
        } catch (Exception e) {
            return Protocol.obj().add("error", e.getMessage()).build();
        }
    }

    public JsonObject dailyRecommend() {
        try {
            return get("/recommend/songs");
        } catch (Exception e) {
            return Protocol.obj().add("error", e.getMessage()).build();
        }
    }

    private void pushMusicHud() {
        var server = com.myiui.MyiuiClient.getSocketServer();
        if (server == null) {
            return;
        }
        server.enqueue(Protocol.push("music", playStatus()));
    }

    private JsonObject get(String path) throws Exception {
        HttpRequest req = HttpRequest.newBuilder()
                .uri(URI.create(baseUrl + path))
                .timeout(Duration.ofSeconds(8))
                .GET()
                .build();
        HttpResponse<String> res = http.send(req, HttpResponse.BodyHandlers.ofString());
        return JsonParser.parseString(res.body()).getAsJsonObject();
    }

    private static String enc(String s) {
        return URLEncoder.encode(s == null ? "" : s, StandardCharsets.UTF_8);
    }
}
