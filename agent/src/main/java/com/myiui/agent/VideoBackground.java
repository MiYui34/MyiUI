package com.myiui.agent;

import com.google.gson.JsonObject;
import com.google.gson.JsonParser;

import org.bytedeco.ffmpeg.global.avutil;
import org.bytedeco.javacv.FFmpegFrameGrabber;
import org.bytedeco.javacv.Java2DFrameConverter;
import org.bytedeco.javacpp.Loader;

import java.awt.Graphics2D;
import java.awt.RenderingHints;
import java.awt.image.BufferedImage;
import java.awt.image.DataBufferInt;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;

public final class VideoBackground {
    private static volatile Thread decodeThread;
    private static volatile boolean running = true;
    private static volatile String currentPath = "";
    private static volatile int reloadGeneration = 0;
    private static volatile int maxFps = 30;
    private static volatile int maxWidth = 1920;
    private static volatile int maxHeight = 1080;
    private static volatile boolean ffmpegReady = false;
    private static volatile boolean firstDecodeDelay = true;
    private static final Object ffmpegLock = new Object();

    private VideoBackground() {}

    public static void start() {
        loadConfig();
        MediaLimits.loadFromConfig();
        String configured = readConfiguredPath();
        currentPath = configured == null ? "" : configured;
        AgentLog.info("背景路径: " + (currentPath.isBlank() ? "(无，待选择)" : currentPath + "（待应用）"));
        AgentLog.info("视频背景服务已启动");
    }

    /** 仅在需要解码视频时加载 FFmpeg，避免与 Sodium/OpenGL 在注入时冲突崩溃。 */
    private static void ensureFfmpegForVideo() {
        if (ffmpegReady) return;
        synchronized (ffmpegLock) {
            if (ffmpegReady) return;
            try {
                Loader.load(avutil.class);
                ffmpegReady = true;
                AgentLog.info("FFmpeg 原生库已加载（延迟）");
            } catch (Throwable t) {
                AgentLog.error("FFmpeg 原生库加载失败", t);
                throw new IllegalStateException("FFmpeg unavailable", t);
            }
        }
    }

    public static String currentPath() {
        return currentPath;
    }

    public static void resumeIfNeeded() {
        if (currentPath == null || currentPath.isBlank()) {
            return;
        }
        if (decodeThread != null && decodeThread.isAlive()) {
            return;
        }
        reload(currentPath);
    }

    /** Stop writing video frames while a vanilla/Sodium screen is shown. */
    public static void suspendForGameScreen() {
        SharedState.clearVideoFrame();
    }

    public static synchronized void reload(String pathOverride) {
        MediaLimits.loadFromConfig();

        running = false;
        final Thread previous = decodeThread;
        if (previous != null) {
            previous.interrupt();
            try {
                previous.join(8000);
            } catch (InterruptedException ignored) {
                Thread.currentThread().interrupt();
            }
            if (previous.isAlive()) {
                AgentLog.error("视频解码线程未能在 8 秒内结束，跳过重载以避免崩溃");
                running = true;
                return;
            }
            decodeThread = null;
        }

        final int generation = ++reloadGeneration;
        running = true;
        String path = pathOverride != null && !pathOverride.isBlank() ? pathOverride : readConfiguredPath();
        if (path != null && !path.isBlank()) {
            try {
                path = Path.of(path).toAbsolutePath().normalize().toString();
            } catch (Exception ignored) {
            }
            persistVideoPath(path);
        }
        currentPath = path == null ? "" : path;
        AgentLog.info("背景路径: " + (currentPath.isBlank() ? "(无)" : currentPath));

        if (currentPath.isBlank()) {
            SharedState.clearVideoFrame();
            return;
        }

        decodeThread = new Thread(() -> {
            if (firstDecodeDelay) {
                sleepQuiet(2500);
                firstDecodeDelay = false;
            } else {
                sleepQuiet(150);
            }
            if (generation == reloadGeneration && running) {
                decodeLoop(generation);
            }
        }, "MyiUI-VideoDecode");
        decodeThread.setDaemon(true);
        decodeThread.start();
    }

    private static void persistVideoPath(String path) {
        try {
            Path cfg = ConfigPaths.backgroundJson();
            JsonObject root;
            if (Files.exists(cfg)) {
                root = JsonParser.parseString(Files.readString(cfg)).getAsJsonObject();
            } else {
                root = new JsonObject();
            }
            root.addProperty("video_path", path);
            if (cfg.getParent() != null) {
                Files.createDirectories(cfg.getParent());
            }
            Files.writeString(cfg, root.toString());
            AgentLog.info("已保存 video_path 至 " + cfg);
        } catch (Exception e) {
            AgentLog.error("保存 video_path 失败", e);
        }
    }

    private static void loadConfig() {
        try {
            Path cfg = ConfigPaths.backgroundJson();
            if (!Files.exists(cfg)) return;
            JsonObject root = JsonParser.parseString(Files.readString(cfg)).getAsJsonObject();
            if (root.has("max_decode_fps")) {
                maxFps = root.get("max_decode_fps").getAsInt();
            }
            if (root.has("max_width")) {
                maxWidth = root.get("max_width").getAsInt();
            }
            if (root.has("max_height")) {
                maxHeight = root.get("max_height").getAsInt();
            }
        } catch (Exception ignored) {
        }
    }

    private static String readConfiguredPath() {
        try {
            Path cfg = ConfigPaths.backgroundJson();
            if (Files.exists(cfg)) {
                JsonObject root = JsonParser.parseString(Files.readString(cfg)).getAsJsonObject();
                if (root.has("video_path")) {
                    String p = root.get("video_path").getAsString();
                    if (p != null && !p.isBlank() && Files.exists(Path.of(p))) {
                        return p;
                    }
                }
            }
        } catch (Exception ignored) {
        }
        return readLibrarySelectionPath();
    }

    private static String readLibrarySelectionPath() {
        try {
            String local = System.getenv("LOCALAPPDATA");
            if (local == null) return "";
            Path libJson = Path.of(local, "MyiUI", "backgrounds", "library.json");
            if (!Files.isRegularFile(libJson)) return "";
            JsonObject root = JsonParser.parseString(Files.readString(libJson)).getAsJsonObject();
            if (!root.has("selected")) return "";
            String name = root.get("selected").getAsString();
            if (name == null || name.isBlank()) return "";
            Path file = Path.of(local, "MyiUI", "backgrounds", name);
            return Files.isRegularFile(file) ? file.toAbsolutePath().normalize().toString() : "";
        } catch (Exception ignored) {
            return "";
        }
    }

    private static void decodeLoop(int generation) {
        long frameIntervalMs = Math.max(1, 1000 / Math.max(1, maxFps));
        int decodeFailStreak = 0;
        while (running && generation == reloadGeneration) {
            String path = currentPath;
            if (path == null || path.isBlank() || !Files.exists(Path.of(path))) {
                sleepQuiet(500);
                continue;
            }
            if (!MediaLimits.isWithinLimit(Path.of(path))) {
                AgentLog.error("背景被拒绝: " + MediaLimits.limitMessage(Path.of(path)));
                sleepQuiet(1000);
                continue;
            }
            String lower = path.toLowerCase();
            if (lower.endsWith(".png") || lower.endsWith(".jpg") || lower.endsWith(".jpeg")) {
                decodeStaticImage(path);
                decodeFailStreak = 0;
                while (running && generation == reloadGeneration && path.equals(currentPath)) {
                    sleepQuiet(500);
                }
                continue;
            }
            if (lower.endsWith(".mp4") || lower.endsWith(".webm")) {
                if (decodeVideo(path, frameIntervalMs, generation)) {
                    decodeFailStreak = 0;
                } else {
                    decodeFailStreak++;
                    sleepQuiet(Math.min(15000, 1000L * decodeFailStreak));
                }
                continue;
            }
            sleepQuiet(500);
        }
    }

    private static BufferedImage fitFrame(BufferedImage img) {
        int w = img.getWidth();
        int h = img.getHeight();
        if (w <= maxWidth && h <= maxHeight) {
            return img;
        }
        float scale = Math.min((float) maxWidth / w, (float) maxHeight / h);
        int nw = Math.max(1, Math.round(w * scale));
        int nh = Math.max(1, Math.round(h * scale));
        BufferedImage out = new BufferedImage(nw, nh, BufferedImage.TYPE_INT_ARGB);
        Graphics2D g = out.createGraphics();
        g.setRenderingHint(RenderingHints.KEY_INTERPOLATION, RenderingHints.VALUE_INTERPOLATION_BILINEAR);
        g.drawImage(img, 0, 0, nw, nh, null);
        g.dispose();
        return out;
    }

    private static byte[] extractRgba(BufferedImage fitted) {
        int w = fitted.getWidth();
        int h = fitted.getHeight();
        byte[] rgba = new byte[w * h * 4];
        if (fitted.getType() == BufferedImage.TYPE_INT_ARGB || fitted.getType() == BufferedImage.TYPE_INT_RGB) {
            int[] pixels = ((DataBufferInt) fitted.getRaster().getDataBuffer()).getData();
            int idx = 0;
            for (int argb : pixels) {
                rgba[idx++] = (byte) ((argb >> 16) & 0xFF);
                rgba[idx++] = (byte) ((argb >> 8) & 0xFF);
                rgba[idx++] = (byte) (argb & 0xFF);
                rgba[idx++] = (byte) (((argb >> 24) & 0xFF) == 0 ? 0xFF : ((argb >> 24) & 0xFF));
            }
            return rgba;
        }
        int idx = 0;
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                int argb = fitted.getRGB(x, y);
                rgba[idx++] = (byte) ((argb >> 16) & 0xFF);
                rgba[idx++] = (byte) ((argb >> 8) & 0xFF);
                rgba[idx++] = (byte) (argb & 0xFF);
                rgba[idx++] = (byte) (((argb >> 24) & 0xFF) == 0 ? 0xFF : ((argb >> 24) & 0xFF));
            }
        }
        return rgba;
    }

    private static BufferedImage toArgb(BufferedImage src) {
        if (src.getType() == BufferedImage.TYPE_INT_ARGB || src.getType() == BufferedImage.TYPE_INT_RGB) {
            return src;
        }
        BufferedImage out = new BufferedImage(src.getWidth(), src.getHeight(), BufferedImage.TYPE_INT_ARGB);
        Graphics2D g = out.createGraphics();
        g.drawImage(src, 0, 0, null);
        g.dispose();
        return out;
    }

    private static void decodeStaticImage(String path) {
        try {
            BufferedImage img = javax.imageio.ImageIO.read(Path.of(path).toFile());
            if (img == null) return;
            BufferedImage fitted = fitFrame(toArgb(img));
            byte[] rgba = extractRgba(fitted);
            SharedState.writeFrame(fitted.getWidth(), fitted.getHeight(), fitted.getWidth() * 4, rgba);
            AgentLog.info("静态图片背景已加载: " + path);
        } catch (IOException e) {
            AgentLog.error("静态图片解码失败: " + path, e);
        }
    }

    private static FFmpegFrameGrabber openVideoGrabber(String path) throws Exception {
        ensureFfmpegForVideo();
        Path file = Path.of(path).toAbsolutePath().normalize();
        if (!Files.isReadable(file)) {
            throw new IOException("无法读取: " + path);
        }
        FFmpegFrameGrabber grabber = new FFmpegFrameGrabber(file.toFile());
        grabber.setOption("an", "dn");
        grabber.setOption("sn", "dn");
        grabber.setOption("probesize", "5000000");
        grabber.setOption("analyzeduration", "5000000");
        grabber.setVideoOption("threads", "1");
        grabber.setImageWidth(maxWidth);
        grabber.setImageHeight(maxHeight);
        grabber.setFrameRate(maxFps);
        return grabber;
    }

    private static boolean decodeVideo(String path, long frameIntervalMs, int generation) {
        FFmpegFrameGrabber grabber = null;
        try {
            grabber = openVideoGrabber(path);
            grabber.start();
            AgentLog.info("视频解码已开始: " + path + " (输出上限 " + maxWidth + "x" + maxHeight + ")");
            try (Java2DFrameConverter converter = new Java2DFrameConverter()) {
                while (running && generation == reloadGeneration && path.equals(currentPath)) {
                    var frame = grabber.grab();
                    if (frame == null || frame.image == null) {
                        frame = grabber.grabImage();
                    }
                    if (frame == null) {
                        grabber.setTimestamp(0);
                        continue;
                    }
                    BufferedImage img = converter.convert(frame);
                    if (img == null) continue;
                    if (!SharedState.isMenuActive()) {
                        sleepQuiet(frameIntervalMs);
                        continue;
                    }
                    BufferedImage fitted = fitFrame(toArgb(img));
                    byte[] rgba = extractRgba(fitted);
                    SharedState.writeFrame(fitted.getWidth(), fitted.getHeight(), fitted.getWidth() * 4, rgba);
                    sleepQuiet(frameIntervalMs);
                }
            }
            return true;
        } catch (Throwable e) {
            AgentLog.error("视频解码失败: " + path, e);
            return false;
        } finally {
            if (grabber != null) {
                try {
                    grabber.stop();
                } catch (Exception ignored) {
                }
                try {
                    grabber.release();
                } catch (Exception ignored) {
                }
            }
        }
    }

    private static void sleepQuiet(long ms) {
        try {
            Thread.sleep(ms);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
    }
}
