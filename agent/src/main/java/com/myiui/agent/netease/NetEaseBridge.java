package com.myiui.agent.netease;

import com.google.gson.JsonObject;
import com.myiui.agent.AgentLog;

import java.util.LinkedHashMap;
import java.util.Map;

/**
 * 网易云命令桥接：被 GameDataBridge 调用，处理所有 `NE_*` 命令。
 * 所有方法在调用线程执行（即 C++ QueryJava 线程，非渲染线程），可直接做 HTTP。
 * 返回 "OK {json}" 或 "ERR xxx"。
 *
 * Phase 2：登录（手机号 + 二维码扫码）。
 */
public final class NetEaseBridge {
    private NetEaseBridge() {}

    // 二维码登录状态码（api-enhanced login_qr/check）
    private static final int QR_CODE_WAITING = 801;   // 等待扫码
    private static final int QR_CODE_SCANNED = 802;   // 已扫码，等待确认
    private static final int QR_CODE_SUCCESS = 803;   // 确认登录
    private static final int QR_CODE_EXPIRED = 800;   // 二维码过期

    // 二维码登录会话状态
    private static final Object QR_LOCK = new Object();
    private static volatile String qrKey = "";
    private static volatile String qrImageB64 = "";   // 纯 base64（不含 data: 前缀）
    private static volatile int qrStatus = 0;
    private static volatile long qrLastPollMs = 0;

    public static void init() {
        NetEaseConfig.load();
        LoginManager.load();
        MusicPlayer.init();
        // 播放完成上报播放记录
        MusicPlayer.setCompletionListener(songId -> NetEaseContent.scrobble(songId));
        AgentLog.info("NetEaseBridge ready. logged_in=" + LoginManager.isLoggedIn());
        // 后台线程自动启动 api-enhanced 服务（不阻塞 Agent 初始化）
        Thread starter = new Thread(() -> {
            boolean ok = ApiServiceStarter.ensureRunning();
            if (ok) {
                AgentLog.info("NetEase API service is reachable");
            } else {
                AgentLog.error("NetEase API service unavailable: " + ApiServiceStarter.startError(), null);
            }
        }, "MyiUI-ApiServiceStarter");
        starter.setDaemon(true);
        starter.start();
    }

    /** 重新拉取用户资料（用于 myPlaylists 等需要 uid 的接口）。 */
    public static void refreshProfile() {
        try {
            com.google.gson.JsonObject resp = NetEaseClient.get("login/status",
                    java.util.Map.of("timestamp", String.valueOf(System.currentTimeMillis())));
            com.google.gson.JsonObject data = resp.has("data") && resp.get("data").isJsonObject()
                    ? resp.getAsJsonObject("data") : resp;
            if (data == null) return;
            com.google.gson.JsonObject profile = data.has("profile") && data.get("profile").isJsonObject()
                    ? data.getAsJsonObject("profile") : null;
            if (profile == null) return;
            long uid = profile.has("userId") ? profile.get("userId").getAsLong() : 0;
            String nick = profile.has("nickname") ? profile.get("nickname").getAsString() : "";
            String avatar = profile.has("avatarUrl") ? profile.get("avatarUrl").getAsString() : "";
            LoginManager.setProfile(uid, nick, avatar);
        } catch (Throwable t) {
            AgentLog.error("refreshProfile failed", t);
        }
    }

    /**
     * 统一命令分发入口，由 GameDataBridge 调用。
     * 命令格式：NE_<CMD> 或 NE_<CMD>:arg1:arg2
     * 返回 "OK {json}" 或 "ERR xxx"。
     */
    public static String dispatch(String cmd) {
        try {
            // 登录类
            if (cmd.equals("NE_LOGIN_STATUS")) return loginStatus();
            if (cmd.equals("NE_API_STATUS")) return apiStatus();
            if (cmd.equals("NE_API_START")) return apiStart();
            if (cmd.equals("NE_QR_IMAGE")) return qrImage();
            if (cmd.equals("NE_QR_POLL")) return qrPoll();
            if (cmd.equals("NE_QR_START")) return qrStart();
            if (cmd.equals("NE_QR_CANCEL")) return qrCancel();
            if (cmd.equals("NE_LOGOUT")) return logout();
            if (cmd.startsWith("NE_LOGIN_PHONE:")) {
                String rest = cmd.substring("NE_LOGIN_PHONE:".length());
                int sep = rest.indexOf(':');
                if (sep <= 0) return "ERR missing_credentials";
                return loginPhone(rest.substring(0, sep).trim(), rest.substring(sep + 1).trim());
            }
            // 播放控制类
            if (cmd.equals("NE_PLAY_STATUS")) return playStatus();
            if (cmd.equals("NE_LYRICS")) return lyricsQuery();
            if (cmd.equals("NE_PLAY_PAUSE")) { MusicPlayer.pause(); return "OK"; }
            if (cmd.equals("NE_PLAY_RESUME")) { MusicPlayer.resume(); return "OK"; }
            if (cmd.equals("NE_PLAY_STOP")) { MusicPlayer.stop(); return "OK"; }
            if (cmd.equals("NE_PLAY_NEXT")) { MusicPlayer.next(); return "OK"; }
            if (cmd.equals("NE_PLAY_PREV")) { MusicPlayer.prev(); return "OK"; }
            if (cmd.equals("NE_PLAY_TOGGLE_MODE")) { MusicPlayer.toggleMode(); return "OK"; }
            if (cmd.startsWith("NE_PLAY_SEEK:")) {
                try { MusicPlayer.seek(Long.parseLong(cmd.substring("NE_PLAY_SEEK:".length()).trim())); return "OK"; }
                catch (NumberFormatException e) { return "ERR seek"; }
            }
            if (cmd.startsWith("NE_SET_VOLUME:")) {
                try { MusicPlayer.setVolume(Integer.parseInt(cmd.substring("NE_SET_VOLUME:".length()).trim())); return "OK"; }
                catch (NumberFormatException e) { return "ERR volume"; }
            }
            if (cmd.startsWith("NE_PLAY_SONG:")) {
                // NE_PLAY_SONG:<json> — 单首播放
                return playSongFromJson(cmd.substring("NE_PLAY_SONG:".length()));
            }
            // 内容功能类
            if (cmd.startsWith("NE_SEARCH:")) {
                return NetEaseContent.search(cmd.substring("NE_SEARCH:".length()));
            }
            if (cmd.equals("NE_MY_PLAYLISTS")) return NetEaseContent.myPlaylists();
            if (cmd.startsWith("NE_PLAYLIST_TRACKS:")) {
                try { return NetEaseContent.playlistTracks(Long.parseLong(cmd.substring("NE_PLAYLIST_TRACKS:".length()).trim())); }
                catch (NumberFormatException e) { return "ERR playlist_id"; }
            }
            if (cmd.equals("NE_DAILY_RECOMMEND")) return NetEaseContent.dailyRecommend();
            if (cmd.startsWith("NE_HISTORY:")) {
                try { return NetEaseContent.playHistory(Integer.parseInt(cmd.substring("NE_HISTORY:".length()).trim())); }
                catch (NumberFormatException e) { return "ERR history_type"; }
            }
            if (cmd.equals("NE_SIGNIN")) return NetEaseContent.dailySignin();
            if (cmd.startsWith("NE_LIKE:")) {
                // NE_LIKE:<songId>:<true|false>
                String rest = cmd.substring("NE_LIKE:".length());
                int sep = rest.indexOf(':');
                if (sep <= 0) return "ERR like_args";
                try {
                    long sid = Long.parseLong(rest.substring(0, sep).trim());
                    boolean like = Boolean.parseBoolean(rest.substring(sep + 1).trim());
                    return NetEaseContent.toggleLike(sid, like);
                } catch (NumberFormatException e) { return "ERR like_id"; }
            }
            if (cmd.startsWith("NE_COMMENTS:")) {
                // NE_COMMENTS:<songId>:<offset>
                String rest = cmd.substring("NE_COMMENTS:".length());
                int sep = rest.indexOf(':');
                long sid;
                int offset = 0;
                try {
                    if (sep > 0) {
                        sid = Long.parseLong(rest.substring(0, sep).trim());
                        offset = Integer.parseInt(rest.substring(sep + 1).trim());
                    } else {
                        sid = Long.parseLong(rest.trim());
                    }
                    return NetEaseContent.comments(sid, offset);
                } catch (NumberFormatException e) { return "ERR comments_args"; }
            }
            // 播放整个队列（JSON 数组）
            if (cmd.startsWith("NE_PLAY_QUEUE:")) {
                return playQueueFromJson(cmd.substring("NE_PLAY_QUEUE:".length()));
            }
            AgentLog.info("NetEaseBridge unknown cmd: " + cmd);
            return "ERR ne_unknown:" + cmd;
        } catch (Throwable t) {
            AgentLog.error("NetEaseBridge dispatch failed: " + cmd, t);
            return "ERR ne_exception:" + t.getClass().getSimpleName() + ":" + t.getMessage();
        }
    }

    // ── 播放状态查询 ──
    public static String playStatus() {
        com.google.gson.JsonObject o = new com.google.gson.JsonObject();
        NetEaseModels.Song s = MusicPlayer.currentSong();
        o.addProperty("playing", MusicPlayer.isPlaying());
        o.addProperty("paused", MusicPlayer.isPaused());
        o.addProperty("position_ms", MusicPlayer.positionMs());
        o.addProperty("duration_ms", MusicPlayer.durationMs());
        o.addProperty("volume", MusicPlayer.volume());
        o.addProperty("mode", MusicPlayer.modeName());
        o.addProperty("queue_size", MusicPlayer.queue().size());
        o.addProperty("queue_index", MusicPlayer.currentIndex());
        if (s != null) {
            o.addProperty("song_id", s.id);
            o.addProperty("song_name", s.name);
            o.addProperty("song_artist", s.artist);
            o.addProperty("song_album", s.album);
            o.addProperty("song_cover", s.coverUrl);
            o.addProperty("song_duration_ms", s.durationMs);
        }
        return "OK " + o.toString();
    }

    private static String playSongFromJson(String json) {
        try {
            com.google.gson.JsonObject o = com.google.gson.JsonParser.parseString(json).getAsJsonObject();
            NetEaseModels.Song s = NetEaseModels.parseSong(o);
            if (s.id == 0) return "ERR no_song_id";
            MusicPlayer.playOne(s);
            return "OK";
        } catch (Throwable t) {
            return "ERR play_song:" + t.getMessage();
        }
    }

    private static String playQueueFromJson(String json) {
        try {
            com.google.gson.JsonElement root = com.google.gson.JsonParser.parseString(json);
            com.google.gson.JsonArray arr = root.isJsonArray() ? root.getAsJsonArray()
                    : (root.isJsonObject() && root.getAsJsonObject().has("songs")
                            ? root.getAsJsonObject().getAsJsonArray("songs") : null);
            if (arr == null) return "ERR no_songs";
            java.util.List<NetEaseModels.Song> list = new java.util.ArrayList<>();
            for (com.google.gson.JsonElement e : arr) {
                if (e.isJsonObject()) list.add(NetEaseModels.parseSong(e.getAsJsonObject()));
            }
            if (list.isEmpty()) return "ERR empty_queue";
            MusicPlayer.playQueue(list, 0);
            return "OK";
        } catch (Throwable t) {
            return "ERR play_queue:" + t.getMessage();
        }
    }

    // ── 歌词查询（当前行 + 下一行） ──
    public static String lyricsQuery() {
        com.google.gson.JsonObject o = new com.google.gson.JsonObject();
        long pos = MusicPlayer.positionMs();
        o.addProperty("current", LyricManager.currentLine(pos));
        o.addProperty("next", LyricManager.nextLine(pos));
        o.addProperty("has_lyrics", LyricManager.hasLyrics());
        return "OK " + o.toString();
    }

    // ── 登录状态查询 ──
    public static String loginStatus() {
        JsonObject o = new JsonObject();
        o.addProperty("logged_in", LoginManager.isLoggedIn());
        o.addProperty("nickname", LoginManager.nickname());
        o.addProperty("avatar", LoginManager.avatarUrl());
        o.addProperty("user_id", LoginManager.userId());
        o.addProperty("qr_pending", !qrImageB64.isEmpty() && qrStatus != QR_CODE_SUCCESS && qrStatus != QR_CODE_EXPIRED);
        o.addProperty("qr_status", qrStatus);
        return "OK " + o.toString();
    }

    // ── API 服务状态 ──
    public static String apiStatus() {
        JsonObject o = new JsonObject();
        o.addProperty("reachable", ApiServiceStarter.isReachable());
        o.addProperty("started", ApiServiceStarter.wasStarted());
        String err = ApiServiceStarter.startError();
        if (err != null && !err.isEmpty()) o.addProperty("error", err);
        return "OK " + o.toString();
    }

    // ── 手动触发启动 API 服务 ──
    public static String apiStart() {
        boolean ok = ApiServiceStarter.ensureRunning();
        JsonObject o = new JsonObject();
        o.addProperty("ok", ok);
        if (!ok) o.addProperty("error", ApiServiceStarter.startError());
        return "OK " + o.toString();
    }

    // ── 手机号登录 ──
    public static String loginPhone(String phone, String password) {
        try {
            if (phone == null || phone.isEmpty() || password == null || password.isEmpty()) {
                return "ERR missing_credentials";
            }
            Map<String, String> params = new LinkedHashMap<>();
            params.put("phone", phone);
            // api-enhanced 支持 password（明文）或 md5_password；用 md5 更安全
            params.put("md5_password", md5Hex(password));
            JsonObject resp = NetEaseClient.post("login/cellphone", params);
            return handleLoginSuccess(resp);
        } catch (Throwable t) {
            AgentLog.error("NetEase loginPhone failed", t);
            return "ERR login:" + t.getMessage();
        }
    }

    // ── 二维码：生成 key + 获取图片 ──
    public static String qrStart() {
        synchronized (QR_LOCK) {
        try {
            // 1. 获取 key
            JsonObject keyResp = NetEaseClient.get("login/qr/key", Map.of("timestamp", String.valueOf(System.currentTimeMillis())));
            JsonObject keyData = dataObject(keyResp);
            if (keyData == null || !keyData.has("unikey")) {
                return "ERR qr_key";
            }
            qrKey = keyData.get("unikey").getAsString();
            // 2. 获取二维码（必须带上 key，否则 qrurl 与 qrKey 不一致）
            JsonObject qrResp = NetEaseClient.get("login/qr/create",
                    Map.of("key", qrKey, "qrimg", "true", "timestamp", String.valueOf(System.currentTimeMillis())));
            JsonObject qrData = dataObject(qrResp);
            if (qrData == null) return "ERR qr_create";

            String qrurl = qrData.has("qrurl") && !qrData.get("qrurl").isJsonNull()
                    ? qrData.get("qrurl").getAsString() : "";
            if (qrurl.isEmpty()) {
                AgentLog.error("NetEase qrStart: missing qrurl in response", null);
                return "ERR qr_no_url";
            }
            if (!qrurl.contains(qrKey)) {
                AgentLog.error("NetEase qrStart: qrurl/key mismatch url=" + qrurl + " key=" + qrKey, null);
                return "ERR qr_key_mismatch";
            }

            // 优先使用 API 返回的 qrimg（base64）
            String qrimg = qrData.has("qrimg") && !qrData.get("qrimg").isJsonNull()
                    ? qrData.get("qrimg").getAsString() : "";
            if (qrimg.startsWith("data:image/png;base64,")) {
                qrImageB64 = qrimg.substring("data:image/png;base64,".length());
            } else if (!qrimg.isEmpty() && !"true".equals(qrimg)) {
                qrImageB64 = qrimg;
            } else {
                // ncm-api exe 无内置 qrcode 库时 qrimg 为空，用 qrurl 本地生成
                AgentLog.info("NetEase: qrimg empty, generating QR locally from qrurl");
                byte[] qrBytes = QrCodeUtil.pngBytes(qrurl, 280);
                if (qrBytes == null || qrBytes.length < 100) {
                    AgentLog.error("NetEase qrStart: local QR generation failed", null);
                    return "ERR qr_gen_failed";
                }
                qrImageB64 = java.util.Base64.getEncoder().encodeToString(qrBytes);
                AgentLog.info("NetEase: QR generated locally (" + qrBytes.length + " bytes) url=" + qrurl);
            }
            qrStatus = QR_CODE_WAITING;
            qrLastPollMs = System.currentTimeMillis();
            AgentLog.info("NetEase QR started, key=" + qrKey);
            return "OK";
        } catch (Throwable t) {
            AgentLog.error("NetEase qrStart failed", t);
            return "ERR qr_start:" + t.getMessage();
        }
        }
    }

    // ── 二维码图片查询（Overlay 用来显示） ──
    public static String qrImage() {
        if (qrImageB64.isEmpty()) {
            return "OK {\"image_b64\":\"\"}";
        }
        JsonObject o = new JsonObject();
        o.addProperty("image_b64", qrImageB64);
        return "OK " + o.toString();
    }

    // ── 二维码状态轮询 ──
    public static String qrPoll() {
        synchronized (QR_LOCK) {
        try {
            if (qrKey.isEmpty()) return "ERR qr_no_key";
            // 限制轮询频率 ≥ 1.5s
            long now = System.currentTimeMillis();
            if (now - qrLastPollMs < 1400) {
                return qrStatusJson();
            }
            qrLastPollMs = now;
            JsonObject resp = NetEaseClient.getRaw("login/qr/check",
                    Map.of("key", qrKey, "timestamp", String.valueOf(now)));
            int code = resp.has("code") ? resp.get("code").getAsInt() : 0;
            qrStatus = code;
            if (code == QR_CODE_SUCCESS) {
                // 803：cookie 可能在 JSON body 或 Set-Cookie 头中
                if (resp.has("cookie") && !resp.get("cookie").isJsonNull()) {
                    String cookie = resp.get("cookie").getAsString();
                    if (cookie != null && !cookie.isBlank()) {
                        LoginManager.saveCookie(cookie);
                    }
                }
                fetchProfile();
                qrImageB64 = "";
                qrKey = "";
                AgentLog.info("NetEase QR login success, nickname=" + LoginManager.nickname());
            } else if (code == QR_CODE_EXPIRED) {
                qrImageB64 = "";
                qrKey = "";
                AgentLog.info("NetEase QR expired");
            }
            return qrStatusJson();
        } catch (Throwable t) {
            AgentLog.error("NetEase qrPoll failed", t);
            return "ERR qr_poll:" + t.getMessage();
        }
        }
    }

    // ── 取消二维码登录 ──
    public static String qrCancel() {
        qrKey = "";
        qrImageB64 = "";
        qrStatus = 0;
        return "OK";
    }

    // ── 登出 ──
    public static String logout() {
        LoginManager.logout();
        return "OK";
    }

    private static String handleLoginSuccess(JsonObject resp) {
        try {
            JsonObject data = dataObject(resp);
            if (data == null) return "ERR login_no_data";
            // login/cellphone 返回 {account:{...}, profile:{...}, cookie:"..."}
            long uid = 0;
            String nick = "";
            String avatar = "";
            if (data.has("profile") && data.get("profile").isJsonObject()) {
                JsonObject profile = data.getAsJsonObject("profile");
                uid = profile.has("userId") ? profile.get("userId").getAsLong() : 0;
                nick = profile.has("nickname") ? profile.get("nickname").getAsString() : "";
                avatar = profile.has("avatarUrl") ? profile.get("avatarUrl").getAsString() : "";
            }
            if (uid == 0 && data.has("account") && data.get("account").isJsonObject()) {
                uid = data.getAsJsonObject("account").has("id") ? data.getAsJsonObject("account").get("id").getAsLong() : 0;
            }
            LoginManager.setProfile(uid, nick, avatar);
            AgentLog.info("NetEase phone login success: uid=" + uid + " nick=" + nick);
            return loginStatus();
        } catch (Throwable t) {
            AgentLog.error("handleLoginSuccess failed", t);
            return "ERR login_parse:" + t.getMessage();
        }
    }

    private static void fetchProfile() {
        try {
            // 用 login/status 拿当前账号信息
            JsonObject resp = NetEaseClient.get("login/status", Map.of("timestamp", String.valueOf(System.currentTimeMillis())));
            JsonObject data = dataObject(resp);
            if (data == null) return;
            JsonObject profile = data.has("profile") && data.get("profile").isJsonObject()
                    ? data.getAsJsonObject("profile") : null;
            if (profile == null) return;
            long uid = profile.has("userId") ? profile.get("userId").getAsLong() : 0;
            String nick = profile.has("nickname") ? profile.get("nickname").getAsString() : "";
            String avatar = profile.has("avatarUrl") ? profile.get("avatarUrl").getAsString() : "";
            LoginManager.setProfile(uid, nick, avatar);
        } catch (Throwable t) {
            AgentLog.error("fetchProfile failed", t);
        }
    }

    private static String qrStatusJson() {
        JsonObject o = new JsonObject();
        o.addProperty("status", qrStatus);
        o.addProperty("logged_in", LoginManager.isLoggedIn());
        String msg = switch (qrStatus) {
            case QR_CODE_WAITING -> "waiting";
            case QR_CODE_SCANNED -> "scanned";
            case QR_CODE_SUCCESS -> "success";
            case QR_CODE_EXPIRED -> "expired";
            default -> "unknown";
        };
        o.addProperty("message", msg);
        return "OK " + o.toString();
    }

    private static JsonObject dataObject(JsonObject resp) {
        if (resp == null) return null;
        if (resp.has("data") && resp.get("data").isJsonObject()) return resp.getAsJsonObject("data");
        // 有些接口直接把数据放在顶层
        return resp;
    }

    private static String md5Hex(String input) {
        try {
            java.security.MessageDigest md = java.security.MessageDigest.getInstance("MD5");
            byte[] digest = md.digest(input.getBytes(java.nio.charset.StandardCharsets.UTF_8));
            StringBuilder sb = new StringBuilder();
            for (byte b : digest) sb.append(String.format("%02x", b));
            return sb.toString();
        } catch (Throwable t) {
            // 降级：返回原密码（api-enhanced 也接受明文 password）
            return input;
        }
    }
}
