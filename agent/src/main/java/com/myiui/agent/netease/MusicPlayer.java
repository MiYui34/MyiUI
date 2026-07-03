package com.myiui.agent.netease;

import com.myiui.agent.AgentLog;
import com.myiui.agent.netease.NetEaseModels.Song;
import org.bytedeco.ffmpeg.global.avutil;
import org.bytedeco.javacv.FFmpegFrameGrabber;
import org.bytedeco.javacpp.Loader;

import javax.sound.sampled.AudioFormat;
import javax.sound.sampled.AudioSystem;
import javax.sound.sampled.DataLine;
import javax.sound.sampled.FloatControl;
import javax.sound.sampled.SourceDataLine;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.ShortBuffer;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

/**
 * 网易云音乐播放引擎：JavaCV FFmpegFrameGrabber 解码 + javax.sound SourceDataLine 播放。
 * 独立播放线程，绝不阻塞渲染线程。
 *
 * 支持播放/暂停/停止/seek、音量、播放模式（顺序/随机/单曲循环）、播放队列。
 */
public final class MusicPlayer {
    public enum PlayMode { SEQUENCE, RANDOM, SINGLE_LOOP }

    // 状态
    private static volatile List<Song> queue = new ArrayList<>();
    private static volatile int currentIndex = -1;
    private static volatile boolean playing = false;
    private static volatile boolean paused = false;
    private static volatile long positionMs = 0;
    private static volatile long durationMs = 0;
    private static volatile PlayMode mode = PlayMode.SEQUENCE;
    private static volatile int volume = 80;  // 0..100

    // 播放线程控制
    private static volatile Thread playThread = null;
    private static volatile FFmpegFrameGrabber activeGrabber = null;
    private static volatile SourceDataLine activeLine = null;
    private static volatile boolean stopRequested = false;
    private static volatile boolean seekRequested = false;
    private static volatile long seekTargetMs = 0;
    private static final Object PLAY_LOCK = new Object();
    private static volatile boolean ffmpegReady = false;
    private static final Object FFMPEG_LOCK = new Object();

    // 当前歌曲（原子引用，避免竞态）
    private static final AtomicReference<Song> currentSong = new AtomicReference<>(null);

    // 单首播放完成回调（由 NetEaseBridge 设置，用于上报播放记录）
    public interface CompletionListener { void onCompleted(long songId); }
    private static volatile CompletionListener completionListener;
    public static void setCompletionListener(CompletionListener l) { completionListener = l; }

    private MusicPlayer() {}

    public static void init() {
        volume = NetEaseConfig.volume();
    }

    // ── 队列与播放控制 ──

    public static void playQueue(List<Song> songs, int startIndex) {
        if (songs == null || songs.isEmpty()) return;
        synchronized (PLAY_LOCK) {
            queue = new ArrayList<>(songs);
            currentIndex = Math.max(0, Math.min(startIndex, songs.size() - 1));
            startPlayback();
        }
    }

    public static void playOne(Song song) {
        if (song == null) return;
        List<Song> q = new ArrayList<>();
        q.add(song);
        playQueue(q, 0);
    }

    public static void pause() {
        paused = true;
        if (activeLine != null) activeLine.stop();
    }

    public static void resume() {
        paused = false;
        if (activeLine != null) activeLine.start();
    }

    public static void stop() {
        synchronized (PLAY_LOCK) {
            stopRequested = true;
            playing = false;
            paused = false;
            closeActive();
            if (playThread != null) playThread.interrupt();
        }
    }

    public static void next() {
        synchronized (PLAY_LOCK) {
            if (queue.isEmpty()) return;
            if (mode == PlayMode.RANDOM && queue.size() > 1) {
                int next;
                do { next = (int) (Math.random() * queue.size()); } while (next == currentIndex);
                currentIndex = next;
            } else {
                currentIndex = (currentIndex + 1) % queue.size();
            }
            startPlayback();
        }
    }

    public static void prev() {
        synchronized (PLAY_LOCK) {
            if (queue.isEmpty()) return;
            currentIndex = (currentIndex - 1 + queue.size()) % queue.size();
            startPlayback();
        }
    }

    public static void seek(long targetMs) {
        seekTargetMs = Math.max(0, targetMs);
        seekRequested = true;
    }

    public static void setVolume(int v) {
        volume = Math.max(0, Math.min(100, v));
        NetEaseConfig.setVolume(volume);
        applyVolumeToLine();
    }

    public static void setMode(PlayMode m) { mode = m; }
    public static void toggleMode() {
        mode = switch (mode) {
            case SEQUENCE -> PlayMode.RANDOM;
            case RANDOM -> PlayMode.SINGLE_LOOP;
            case SINGLE_LOOP -> PlayMode.SEQUENCE;
        };
    }

    // ── 状态查询 ──

    public static Song currentSong() { return currentSong.get(); }
    public static List<Song> queue() { return Collections.unmodifiableList(queue); }
    public static int currentIndex() { return currentIndex; }
    public static boolean isPlaying() { return playing && !paused; }
    public static boolean isPaused() { return paused; }
    public static long positionMs() { return positionMs; }
    public static long durationMs() { return durationMs; }
    public static int volume() { return volume; }
    public static PlayMode mode() { return mode; }
    public static String modeName() {
        return switch (mode) {
            case SEQUENCE -> "顺序";
            case RANDOM -> "随机";
            case SINGLE_LOOP -> "单曲";
        };
    }

    // ── 播放核心 ──

    private static void startPlayback() {
        // 若由播放线程自身调用（自动切歌），不要 interrupt/join 自己（会死等 1.5s）
        Thread current = Thread.currentThread();
        if (playThread != null && playThread != current) {
            stopRequested = true;
            closeActive();
            playThread.interrupt();
            try { playThread.join(1500); } catch (InterruptedException ignored) { Thread.currentThread().interrupt(); }
        }
        // 自身调用时：只需让当前循环退出（stopRequested 会在下面重置为 false）

        final Song song = queue.get(currentIndex);
        currentSong.set(song);
        positionMs = 0;
        durationMs = song.durationMs;
        playing = true;
        paused = false;
        stopRequested = false;
        seekRequested = false;
        LyricManager.onSongChanged(song.id);

        playThread = new Thread(() -> playbackLoop(song), "MyiUI-MusicPlayer");
        playThread.setDaemon(true);
        playThread.start();
    }

    private static void playbackLoop(Song song) {
        FFmpegFrameGrabber grabber = null;
        SourceDataLine line = null;
        try {
            ensureFfmpeg();
            String streamUrl = resolveSongUrl(song.id);
            if (streamUrl == null || streamUrl.isEmpty()) {
                AgentLog.error("NetEase: no url for song " + song.id, null);
                return;
            }
            AgentLog.info("NetEase playing: " + song.name + " (" + streamUrl + ")");

            grabber = new FFmpegFrameGrabber(streamUrl);
            grabber.setOption("probesize", "2000000");
            grabber.setOption("analyzeduration", "2000000");
            grabber.start();
            activeGrabber = grabber;

            int channels = grabber.getAudioChannels() > 0 ? grabber.getAudioChannels() : 2;
            int sampleRate = grabber.getSampleRate() > 0 ? grabber.getSampleRate() : 44100;
            AudioFormat format = new AudioFormat(sampleRate, 16, channels, true, false);

            DataLine.Info info = new DataLine.Info(SourceDataLine.class, format);
            line = (SourceDataLine) AudioSystem.getLine(info);
            line.open(format, 4096 * channels * 2);
            line.start();
            activeLine = line;
            applyVolumeToLine();

            while (!stopRequested && !Thread.currentThread().isInterrupted()) {
                if (seekRequested) {
                    seekRequested = false;
                    try {
                        grabber.setTimestamp(seekTargetMs * 1000);  // FFmpeg 用 μs
                        line.flush();
                    } catch (Throwable t) {
                        AgentLog.error("NetEase seek failed", t);
                    }
                }
                if (paused) {
                    sleepQuiet(50);
                    continue;
                }
                org.bytedeco.javacv.Frame frame = grabber.grabSamples();
                if (frame == null || frame.samples == null) {
                    // 流结束
                    if (!stopRequested) onSongCompleted(song);
                    break;
                }
                byte[] pcm = frameToPcm16(frame, channels);
                if (pcm != null && pcm.length > 0) {
                    line.write(pcm, 0, pcm.length);
                }
                positionMs = grabber.getTimestamp() / 1000;  // μs → ms
                if (durationMs <= 0 && grabber.getLengthInTime() > 0) {
                    durationMs = grabber.getLengthInTime() / 1000;
                }
            }
        } catch (Throwable t) {
            if (!stopRequested) AgentLog.error("NetEase playback failed: " + song.name, t);
        } finally {
            if (line != null) { try { line.drain(); } catch (Throwable ignored) {} try { line.stop(); } catch (Throwable ignored) {} try { line.close(); } catch (Throwable ignored) {} }
            if (grabber != null) { try { grabber.stop(); } catch (Throwable ignored) {} try { grabber.release(); } catch (Throwable ignored) {} }
            // 仅当本线程的资源仍是 active 时才清空，避免清掉新播放线程刚设置的引用
            synchronized (PLAY_LOCK) {
                if (activeGrabber == grabber) activeGrabber = null;
                if (activeLine == line) activeLine = null;
            }
        }
    }

    private static byte[] frameToPcm16(org.bytedeco.javacv.Frame frame, int channels) {
        // Frame.samples 是 Buffer[]，每个 buffer 是一个通道（planar）或全部（interleaved）
        java.nio.Buffer[] samples = frame.samples;
        if (samples == null || samples.length == 0) return null;

        if (samples.length == channels) {
            // Planar：每个 buffer 是一个通道，需 interleave
            int perChannel = samples[0].limit();
            int bytes = perChannel * channels * 2;
            ByteBuffer out = ByteBuffer.allocate(bytes).order(ByteOrder.LITTLE_ENDIAN);
            ShortBuffer[] chans = new ShortBuffer[channels];
            int max = 0;
            for (int c = 0; c < channels; c++) {
                java.nio.Buffer b = samples[c];
                chans[c] = (b instanceof ShortBuffer) ? (ShortBuffer) b : toShortBuffer(b);
                if (chans[c].limit() > max) max = chans[c].limit();
            }
            for (int i = 0; i < max; i++) {
                for (int c = 0; c < channels; c++) {
                    out.putShort(i < chans[c].limit() ? chans[c].get(i) : (short) 0);
                }
            }
            return out.array();
        } else {
            // Interleaved：samples[0] 已包含全部交错样本
            ShortBuffer sb = toShortBuffer(samples[0]);
            int n = sb.limit();
            ByteBuffer out = ByteBuffer.allocate(n * 2).order(ByteOrder.LITTLE_ENDIAN);
            for (int i = 0; i < n; i++) out.putShort(sb.get(i));
            return out.array();
        }
    }

    private static ShortBuffer toShortBuffer(java.nio.Buffer b) {
        if (b instanceof ShortBuffer) return (ShortBuffer) b;
        int n = b.limit();
        ShortBuffer out = ByteBuffer.allocate(n * 2).order(ByteOrder.LITTLE_ENDIAN).asShortBuffer();
        if (b instanceof java.nio.FloatBuffer) {
            java.nio.FloatBuffer fb = (java.nio.FloatBuffer) b;
            for (int i = 0; i < n; i++) {
                float v = fb.get(i);
                int s = Math.round(v * 32767f);
                if (s > 32767) s = 32767;
                if (s < -32768) s = -32768;
                out.put((short) s);
            }
        } else if (b instanceof java.nio.IntBuffer) {
            java.nio.IntBuffer ib = (java.nio.IntBuffer) b;
            for (int i = 0; i < n; i++) out.put((short) (ib.get(i) >> 16));
        } else if (b instanceof java.nio.ByteBuffer) {
            ByteBuffer bb = (ByteBuffer) b;
            ShortBuffer sb = bb.order(ByteOrder.LITTLE_ENDIAN).asShortBuffer();
            return sb;
        }
        out.flip();
        return out;
    }

    private static String resolveSongUrl(long songId) {
        try {
            String level = NetEaseConfig.quality();
            java.util.Map<String, String> params = new java.util.LinkedHashMap<>();
            params.put("id", String.valueOf(songId));
            params.put("level", level);
            params.put("timestamp", String.valueOf(System.currentTimeMillis()));
            com.google.gson.JsonObject resp = NetEaseClient.get("song/url/v1", params);
            if (resp.has("data") && resp.get("data").isJsonArray()) {
                com.google.gson.JsonArray arr = resp.getAsJsonArray("data");
                for (com.google.gson.JsonElement e : arr) {
                    if (e.isJsonObject()) {
                        com.google.gson.JsonObject o = e.getAsJsonObject();
                        if (o.has("url") && !o.get("url").isJsonNull()) {
                            return o.get("url").getAsString();
                        }
                    }
                }
            }
            return null;
        } catch (Throwable t) {
            AgentLog.error("NetEase resolveSongUrl failed: " + songId, t);
            return null;
        }
    }

    private static void onSongCompleted(Song song) {
        try {
            if (completionListener != null) completionListener.onCompleted(song.id);
        } catch (Throwable ignored) {}
        // 自动下一首
        if (!stopRequested && mode != PlayMode.SINGLE_LOOP) {
            next();
        } else if (mode == PlayMode.SINGLE_LOOP) {
            startPlayback();
        }
    }

    private static void closeActive() {
        if (activeGrabber != null) { try { activeGrabber.stop(); } catch (Throwable ignored) {} }
        if (activeLine != null) { try { activeLine.stop(); } catch (Throwable ignored) {} try { activeLine.close(); } catch (Throwable ignored) {} }
        activeGrabber = null;
        activeLine = null;
    }

    private static void applyVolumeToLine() {
        try {
            if (activeLine != null && activeLine.isControlSupported(FloatControl.Type.MASTER_GAIN)) {
                FloatControl c = (FloatControl) activeLine.getControl(FloatControl.Type.MASTER_GAIN);
                float min = c.getMinimum();
                float max = c.getMaximum();
                // 音量 0..100 → gain dB：20*log10(ratio)，ratio∈[0.001,1.0]
                // 0% → min（最弱），100% → 0dB（max 上限）
                float ratio = volume / 100.f;
                float gain;
                if (ratio <= 0.001f) {
                    gain = min;
                } else {
                    if (ratio > 1.f) ratio = 1.f;
                    gain = 20.f * (float) Math.log10(ratio);  // dB
                }
                if (gain < min) gain = min;
                if (gain > max) gain = max;
                c.setValue(gain);
            }
        } catch (Throwable ignored) {}
    }

    private static void ensureFfmpeg() {
        if (ffmpegReady) return;
        synchronized (FFMPEG_LOCK) {
            if (ffmpegReady) return;
            try {
                Loader.load(avutil.class);
                ffmpegReady = true;
                AgentLog.info("NetEase: FFmpeg 原生库已加载");
            } catch (Throwable t) {
                AgentLog.error("NetEase: FFmpeg 加载失败", t);
                throw new IllegalStateException("FFmpeg unavailable", t);
            }
        }
    }

    private static void sleepQuiet(long ms) {
        try { Thread.sleep(ms); } catch (InterruptedException e) { Thread.currentThread().interrupt(); }
    }
}
