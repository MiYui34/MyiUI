package com.myiui.agent.netease;

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;

import java.util.ArrayList;
import java.util.List;

/**
 * 网易云数据模型 — 从 api-enhanced 的 JSON 响应中提取所需字段。
 * 用 JsonObject 直接读取避免定义大量 POJO，保持轻量。
 * 所有 parse 方法对缺失字段安全（返回 null/0/空）。
 */
public final class NetEaseModels {

    private NetEaseModels() {}

    public static final class Song {
        public long id;
        public String name = "";
        public String artist = "";
        public String album = "";
        public long durationMs;
        public String coverUrl = "";
        public boolean liked;

        public String displayKey() { return id + "|" + name; }
    }

    public static final class Playlist {
        public long id;
        public String name = "";
        public String coverUrl = "";
        public int trackCount;
        public String creator = "";
    }

    public static final class LyricLine {
        public long timeMs;
        public String text = "";
    }

    public static final class Comment {
        public long commentId;
        public String user = "";
        public String avatar = "";
        public String content = "";
        public int likedCount;
    }

    public static Song parseSong(JsonObject o) {
        Song s = new Song();
        if (o == null) return s;
        s.id = o.has("id") ? o.get("id").getAsLong() : 0;
        s.name = str(o, "name");
        // artists 数组
        if (o.has("ar")) s.artist = parseArtistNames(o.get("ar"));
        else if (o.has("artists")) s.artist = parseArtistNames(o.get("artists"));
        if (o.has("al")) s.album = objStr(o.getAsJsonObject("al"), "name");
        else if (o.has("album")) s.album = objStr(safeObj(o, "album"), "name");
        s.durationMs = o.has("dt") ? o.get("dt").getAsLong() : (o.has("duration") ? o.get("duration").getAsLong() : 0);
        if (o.has("al")) s.coverUrl = objStr(o.getAsJsonObject("al"), "picUrl");
        else if (o.has("album")) s.coverUrl = objStr(safeObj(o, "album"), "picUrl");
        return s;
    }

    public static List<Song> parseSongList(JsonArray arr) {
        List<Song> out = new ArrayList<>();
        if (arr == null) return out;
        for (JsonElement e : arr) {
            if (e.isJsonObject()) out.add(parseSong(e.getAsJsonObject()));
        }
        return out;
    }

    public static Playlist parsePlaylist(JsonObject o) {
        Playlist p = new Playlist();
        if (o == null) return p;
        p.id = o.has("id") ? o.get("id").getAsLong() : 0;
        p.name = str(o, "name");
        p.coverUrl = str(o, "picUrl");
        if (p.coverUrl.isEmpty() && o.has("coverImgUrl")) p.coverUrl = str(o, "coverImgUrl");
        p.trackCount = o.has("trackCount") ? o.get("trackCount").getAsInt() : 0;
        if (o.has("creator")) p.creator = objStr(safeObj(o, "creator"), "nickname");
        return p;
    }

    public static List<Playlist> parsePlaylistList(JsonArray arr) {
        List<Playlist> out = new ArrayList<>();
        if (arr == null) return out;
        for (JsonElement e : arr) {
            if (e.isJsonObject()) out.add(parsePlaylist(e.getAsJsonObject()));
        }
        return out;
    }

    public static List<LyricLine> parseLrc(String lrcText) {
        List<LyricLine> out = new ArrayList<>();
        if (lrcText == null || lrcText.isEmpty()) return out;
        // [mm:ss.xx]text  或  [mm:ss.xxx]text
        java.util.regex.Pattern p = java.util.regex.Pattern.compile("\\[(\\d+):(\\d+)(?:\\.(\\d+))?\\]([^\\[]*)");
        java.util.regex.Matcher m = p.matcher(lrcText);
        while (m.find()) {
            LyricLine line = new LyricLine();
            int min = Integer.parseInt(m.group(1));
            int sec = Integer.parseInt(m.group(2));
            String msStr = m.group(3);
            int ms = msStr == null ? 0 : Integer.parseInt(msStr.length() > 3 ? msStr.substring(0, 3) : msStr);
            if (msStr != null && msStr.length() == 2) ms *= 10;
            line.timeMs = min * 60_000L + sec * 1000L + ms;
            line.text = m.group(4).trim();
            if (line.text.isEmpty() || isCreditLine(line.text)) continue;
            out.add(line);
        }
        out.sort(java.util.Comparator.comparingLong(l -> l.timeMs));
        return out;
    }

    /** 过滤 LRC 内嵌的制作信息行（作曲/作词等），避免 UI 显示乱码元数据。 */
    private static boolean isCreditLine(String text) {
        if (text == null || text.isEmpty()) return true;
        String lower = text.toLowerCase();
        if (lower.contains("作詞") || lower.contains("作词") || lower.contains("作曲")
                || lower.contains("編曲") || lower.contains("编曲") || lower.contains("监棚")
                || lower.contains("producer") || lower.contains("lyricist") || lower.contains("composer")) {
            return true;
        }
        // "key : value" 形式且含多个 / 分隔的名字，通常是制作名单
        if (text.matches(".*[：:].*[/／].*")) return true;
        return false;
    }

    public static List<Comment> parseComments(JsonArray arr) {
        List<Comment> out = new ArrayList<>();
        if (arr == null) return out;
        for (JsonElement e : arr) {
            if (!e.isJsonObject()) continue;
            JsonObject o = e.getAsJsonObject();
            Comment c = new Comment();
            c.commentId = o.has("commentId") ? o.get("commentId").getAsLong() : 0;
            c.content = str(o, "content");
            c.likedCount = o.has("likedCount") ? o.get("likedCount").getAsInt() : 0;
            if (o.has("user")) {
                JsonObject u = safeObj(o, "user");
                c.user = objStr(u, "nickname");
                c.avatar = objStr(u, "avatarUrl");
            }
            out.add(c);
        }
        return out;
    }

    private static String parseArtistNames(JsonElement e) {
        if (e == null || !e.isJsonArray()) return "";
        JsonArray arr = e.getAsJsonArray();
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < arr.size(); i++) {
            if (i > 0) sb.append('/');
            JsonObject a = arr.get(i).getAsJsonObject();
            sb.append(objStr(a, "name"));
        }
        return sb.toString();
    }

    private static String str(JsonObject o, String key) {
        if (o == null || !o.has(key)) return "";
        JsonElement e = o.get(key);
        if (e.isJsonNull()) return "";
        return e.getAsString();
    }

    private static String objStr(JsonObject o, String key) {
        return str(o, key);
    }

    private static JsonObject safeObj(JsonObject parent, String key) {
        if (parent == null || !parent.has(key) || !parent.get(key).isJsonObject()) return null;
        return parent.getAsJsonObject(key);
    }
}
