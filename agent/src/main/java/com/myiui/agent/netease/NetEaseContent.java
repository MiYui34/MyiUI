package com.myiui.agent.netease;

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;
import com.myiui.agent.AgentLog;
import com.myiui.agent.netease.NetEaseModels.Comment;
import com.myiui.agent.netease.NetEaseModels.Playlist;
import com.myiui.agent.netease.NetEaseModels.Song;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * 网易云内容功能：搜索、我的歌单、每日推荐、播放记录、签到、喜欢、评论。
 * 全部同步阻塞调用，调用方须在 QueryJava 线程（非渲染线程）。
 */
public final class NetEaseContent {
    private NetEaseContent() {}

    // ── 搜索歌曲 ──
    public static String search(String keywords) {
        try {
            if (keywords == null || keywords.trim().isEmpty()) return "ERR empty_keywords";
            Map<String, String> params = new LinkedHashMap<>();
            params.put("keywords", keywords.trim());
            params.put("type", "1");           // 1=歌曲
            params.put("limit", "30");
            params.put("offset", "0");
            params.put("timestamp", String.valueOf(System.currentTimeMillis()));
            JsonObject resp = NetEaseClient.get("search", params);
            JsonArray songs = extractSongsFromSearch(resp);
            return "OK " + songsJson(NetEaseModels.parseSongList(songs));
        } catch (Throwable t) {
            AgentLog.error("NetEase search failed: " + keywords, t);
            return "ERR search:" + t.getMessage();
        }
    }

    // ── 我的歌单 ──
    public static String myPlaylists() {
        try {
            long uid = LoginManager.userId();
            if (uid == 0) {
                // 未拿到 uid，尝试 login/status
                NetEaseBridge.refreshProfile();
                uid = LoginManager.userId();
            }
            if (uid == 0) return "ERR no_uid";
            JsonObject resp = NetEaseClient.get("user/playlist",
                    Map.of("uid", String.valueOf(uid), "limit", "50", "timestamp", String.valueOf(System.currentTimeMillis())));
            JsonArray arr = null;
            if (resp.has("playlist") && resp.get("playlist").isJsonArray()) arr = resp.getAsJsonArray("playlist");
            List<Playlist> list = NetEaseModels.parsePlaylistList(arr);
            return "OK " + playlistsJson(list);
        } catch (Throwable t) {
            AgentLog.error("NetEase myPlaylists failed", t);
            return "ERR playlists:" + t.getMessage();
        }
    }

    // ── 歌单详情（含歌曲列表） ──
    public static String playlistTracks(long playlistId) {
        try {
            JsonObject resp = NetEaseClient.get("playlist/detail",
                    Map.of("id", String.valueOf(playlistId), "timestamp", String.valueOf(System.currentTimeMillis())));
            JsonArray tracks = null;
            if (resp.has("playlist") && resp.get("playlist").isJsonObject()) {
                JsonObject pl = resp.getAsJsonObject("playlist");
                if (pl.has("tracks") && pl.get("tracks").isJsonArray()) tracks = pl.getAsJsonArray("tracks");
            }
            List<Song> list = NetEaseModels.parseSongList(tracks);
            return "OK " + songsJson(list);
        } catch (Throwable t) {
            AgentLog.error("NetEase playlistTracks failed: " + playlistId, t);
            return "ERR playlist_tracks:" + t.getMessage();
        }
    }

    // ── 每日推荐 ──
    public static String dailyRecommend() {
        try {
            JsonObject resp = NetEaseClient.get("recommend/songs",
                    Map.of("timestamp", String.valueOf(System.currentTimeMillis())));
            JsonArray songs = null;
            if (resp.has("data") && resp.get("data").isJsonObject()) {
                JsonObject d = resp.getAsJsonObject("data");
                if (d.has("dailySongs") && d.get("dailySongs").isJsonArray()) songs = d.getAsJsonArray("dailySongs");
            }
            // dailySongs 字段名与 songs 略不同，parseSong 兼容
            List<Song> list = new ArrayList<>();
            if (songs != null) {
                for (JsonElement e : songs) {
                    if (e.isJsonObject()) list.add(NetEaseModels.parseSong(e.getAsJsonObject()));
                }
            }
            return "OK " + songsJson(list);
        } catch (Throwable t) {
            AgentLog.error("NetEase dailyRecommend failed", t);
            return "ERR daily:" + t.getMessage();
        }
    }

    // ── 播放记录（type=1 本周，type=0 全部） ──
    public static String playHistory(int type) {
        try {
            long uid = LoginManager.userId();
            if (uid == 0) return "ERR no_uid";
            JsonObject resp = NetEaseClient.get("user/record",
                    Map.of("uid", String.valueOf(uid), "type", String.valueOf(type),
                            "timestamp", String.valueOf(System.currentTimeMillis())));
            // 响应 {weekData:[{playCount, song:{...}}]} 或 {allData:[...]}
            String key = type == 1 ? "weekData" : "allData";
            JsonArray arr = resp.has(key) && resp.get(key).isJsonArray() ? resp.getAsJsonArray(key) : null;
            List<Song> list = new ArrayList<>();
            if (arr != null) {
                for (JsonElement e : arr) {
                    if (!e.isJsonObject()) continue;
                    JsonObject o = e.getAsJsonObject();
                    if (o.has("song") && o.get("song").isJsonObject()) {
                        list.add(NetEaseModels.parseSong(o.getAsJsonObject("song")));
                    }
                }
            }
            return "OK " + songsJson(list);
        } catch (Throwable t) {
            AgentLog.error("NetEase playHistory failed", t);
            return "ERR history:" + t.getMessage();
        }
    }

    // ── 每日签到 ──
    public static String dailySignin() {
        try {
            // type 0=移动端签到（带云贝），1=网页端
            NetEaseClient.get("daily_signin", Map.of("type", "0", "timestamp", String.valueOf(System.currentTimeMillis())));
            return "OK";
        } catch (Throwable t) {
            AgentLog.error("NetEase dailySignin failed", t);
            return "ERR signin:" + t.getMessage();
        }
    }

    // ── 喜欢/红心切换 ──
    public static String toggleLike(long songId, boolean like) {
        try {
            NetEaseClient.get("like",
                    Map.of("id", String.valueOf(songId), "like", like ? "true" : "false",
                            "timestamp", String.valueOf(System.currentTimeMillis())));
            return "OK";
        } catch (Throwable t) {
            AgentLog.error("NetEase toggleLike failed: " + songId, t);
            return "ERR like:" + t.getMessage();
        }
    }

    // ── 评论 ──
    public static String comments(long songId, int offset) {
        try {
            JsonObject resp = NetEaseClient.get("comment/music",
                    Map.of("id", String.valueOf(songId), "limit", "20",
                            "offset", String.valueOf(offset), "timestamp", String.valueOf(System.currentTimeMillis())));
            JsonArray arr = null;
            if (resp.has("comments") && resp.get("comments").isJsonArray()) arr = resp.getAsJsonArray("comments");
            List<Comment> list = NetEaseModels.parseComments(arr);
            return "OK " + commentsJson(list);
        } catch (Throwable t) {
            AgentLog.error("NetEase comments failed: " + songId, t);
            return "ERR comments:" + t.getMessage();
        }
    }

    // ── 播放完成上报 scrobble ──
    public static void scrobble(long songId) {
        try {
            NetEaseClient.get("scrobble",
                    Map.of("id", String.valueOf(songId), "sourceid", "personalradio",
                            "time", "60", "timestamp", String.valueOf(System.currentTimeMillis())));
        } catch (Throwable t) {
            AgentLog.error("NetEase scrobble failed: " + songId, t);
        }
    }

    // ── JSON 序列化 ──

    private static String songsJson(List<Song> songs) {
        StringBuilder sb = new StringBuilder();
        sb.append("{\"songs\":[");
        for (int i = 0; i < songs.size(); i++) {
            if (i > 0) sb.append(',');
            Song s = songs.get(i);
            sb.append("{\"id\":").append(s.id)
                    .append(",\"name\":\"").append(esc(s.name)).append('"')
                    .append(",\"artist\":\"").append(esc(s.artist)).append('"')
                    .append(",\"album\":\"").append(esc(s.album)).append('"')
                    .append(",\"cover\":\"").append(esc(s.coverUrl)).append('"')
                    .append(",\"duration_ms\":").append(s.durationMs).append('}');
        }
        sb.append("]}");
        return sb.toString();
    }

    private static String playlistsJson(List<Playlist> pls) {
        StringBuilder sb = new StringBuilder();
        sb.append("{\"playlists\":[");
        for (int i = 0; i < pls.size(); i++) {
            if (i > 0) sb.append(',');
            Playlist p = pls.get(i);
            sb.append("{\"id\":").append(p.id)
                    .append(",\"name\":\"").append(esc(p.name)).append('"')
                    .append(",\"cover\":\"").append(esc(p.coverUrl)).append('"')
                    .append(",\"track_count\":").append(p.trackCount)
                    .append(",\"creator\":\"").append(esc(p.creator)).append('"')
                    .append('}');
        }
        sb.append("]}");
        return sb.toString();
    }

    private static String commentsJson(List<Comment> comments) {
        StringBuilder sb = new StringBuilder();
        sb.append("{\"comments\":[");
        for (int i = 0; i < comments.size(); i++) {
            if (i > 0) sb.append(',');
            Comment c = comments.get(i);
            sb.append("{\"id\":").append(c.commentId)
                    .append(",\"user\":\"").append(esc(c.user)).append('"')
                    .append(",\"avatar\":\"").append(esc(c.avatar)).append('"')
                    .append(",\"content\":\"").append(esc(c.content)).append('"')
                    .append(",\"liked_count\":").append(c.likedCount).append('}');
        }
        sb.append("]}");
        return sb.toString();
    }

    private static JsonArray extractSongsFromSearch(JsonObject resp) {
        // search 响应 {result:{songs:[...]}}
        if (resp.has("result") && resp.get("result").isJsonObject()) {
            JsonObject result = resp.getAsJsonObject("result");
            if (result.has("songs") && result.get("songs").isJsonArray()) {
                return result.getAsJsonArray("songs");
            }
        }
        return null;
    }

    private static String esc(String s) {
        if (s == null) return "";
        return s.replace("\\", "\\\\").replace("\"", "\\\"").replace("\n", "\\n").replace("\r", "");
    }
}
