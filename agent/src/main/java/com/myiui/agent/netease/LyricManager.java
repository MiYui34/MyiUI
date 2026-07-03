package com.myiui.agent.netease;

import com.google.gson.JsonObject;
import com.myiui.agent.AgentLog;
import com.myiui.agent.netease.NetEaseModels.LyricLine;

import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.concurrent.atomic.AtomicReference;

/**
 * 歌词管理：按当前歌曲拉取 LRC，按播放进度索引当前行。
 * 歌词获取在独立线程执行，避免阻塞渲染/播放。
 */
public final class LyricManager {
    private static final AtomicReference<List<LyricLine>> linesRef = new AtomicReference<>(Collections.emptyList());
    private static volatile long currentSongId = -1;
    private static volatile boolean fetching = false;

    private LyricManager() {}

    /** 切歌时调用：拉取新歌词。 */
    public static void onSongChanged(long songId) {
        currentSongId = songId;
        linesRef.set(Collections.emptyList());
        if (songId <= 0) return;
        if (!NetEaseConfig.lyricsSync()) return;
        // 独立线程拉歌词
        Thread t = new Thread(() -> fetchLyrics(songId), "MyiUI-LyricFetch");
        t.setDaemon(true);
        t.start();
    }

    private static void fetchLyrics(long songId) {
        if (fetching) return;
        fetching = true;
        try {
            JsonObject resp = NetEaseClient.get("lyric",
                    Map.of("id", String.valueOf(songId), "timestamp", String.valueOf(System.currentTimeMillis())));
            String lrc = "";
            if (resp.has("lrc") && resp.get("lrc").isJsonObject()) {
                JsonObject lrcObj = resp.getAsJsonObject("lrc");
                if (lrcObj.has("lyric") && !lrcObj.get("lyric").isJsonNull()) {
                    lrc = lrcObj.get("lyric").getAsString();
                }
            }
            if (lrc.isEmpty()) {
                AgentLog.info("NetEase: no lyric for song " + songId);
                linesRef.set(Collections.emptyList());
                return;
            }
            List<LyricLine> parsed = NetEaseModels.parseLrc(lrc);
            linesRef.set(parsed);
            AgentLog.info("NetEase: loaded " + parsed.size() + " lyric lines for song " + songId);
        } catch (Throwable t) {
            AgentLog.error("NetEase fetchLyrics failed: " + songId, t);
            linesRef.set(Collections.emptyList());
        } finally {
            fetching = false;
        }
    }

    /** 按当前播放进度返回歌词行文本（空字符串表示无歌词）。 */
    public static String currentLine(long positionMs) {
        List<LyricLine> lines = linesRef.get();
        if (lines.isEmpty()) return "";
        LyricLine found = null;
        for (LyricLine l : lines) {
            if (l.timeMs <= positionMs) found = l;
            else break;
        }
        return found == null ? "" : found.text;
    }

    /** 返回下一行歌词（用于 UI 显示当前+下一句）。 */
    public static String nextLine(long positionMs) {
        List<LyricLine> lines = linesRef.get();
        if (lines.isEmpty()) return "";
        for (LyricLine l : lines) {
            if (l.timeMs > positionMs) return l.text;
        }
        return "";
    }

    public static boolean hasLyrics() {
        return !linesRef.get().isEmpty();
    }

    public static void clear() {
        currentSongId = -1;
        linesRef.set(Collections.emptyList());
    }
}
