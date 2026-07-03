package com.myiui.agent.netease;

import com.myiui.agent.AgentLog;

import java.io.File;
import java.io.IOException;
import java.net.HttpURLConnection;
import java.net.URI;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * 自动启动 api-enhanced 服务。
 *
 * 优先使用独立可执行文件 ncm-api-win-x64.exe（无需 Node.js），
 * 其次尝试 Node.js + app.js（需克隆仓库并安装依赖）。
 *
 * 查找 ncm-api-win-x64.exe 的位置：
 *   - $MYIUI_ROOT/ncm-api-win-x64.exe
 *   - $MYIUI_ROOT/api-enhanced/ncm-api-win-x64.exe
 *   - %LOCALAPPDATA%/MyiUI/ncm-api-win-x64.exe
 *   - %LOCALAPPDATA%/MyiUI/api-enhanced/ncm-api-win-x64.exe
 *   - project_root.txt 指向目录下的 ncm-api-win-x64.exe
 *   - 当前工作目录下的 ncm-api-win-x64.exe
 */
public final class ApiServiceStarter {
    private static volatile Process apiProcess = null;
    private static volatile boolean started = false;
    private static volatile String startError = "";

    // 独立 exe 下载地址（GitHub Releases v4.36.1）
    private static final String EXE_DOWNLOAD_URL =
            "https://github.com/NeteaseCloudMusicApiEnhanced/api-enhanced/releases/download/v4.36.1/ncm-api-win-x64.exe";
    private static final String EXE_FILENAME = "ncm-api-win-x64.exe";

    private ApiServiceStarter() {}

    /** 检查 API 服务是否可达（GET /banner 轻量请求）。 */
    public static boolean isReachable() {
        try {
            String base = NetEaseConfig.baseUrl();
            HttpURLConnection conn = (HttpURLConnection) URI.create(base + "/banner?type=0").toURL().openConnection();
            conn.setConnectTimeout(5000);
            conn.setReadTimeout(5000);
            conn.setRequestMethod("GET");
            int code = conn.getResponseCode();
            conn.disconnect();
            return code >= 200 && code < 500;
        } catch (Throwable t) {
            return false;
        }
    }

    /**
     * 确保 API 服务运行。若已可达直接返回 true。
     * 否则依次尝试：独立 exe → Node.js + app.js → 自动下载 exe。
     */
    public static boolean ensureRunning() {
        // 如果进程仍在运行，先检查是否可达（不急于重启）
        if (apiProcess != null && apiProcess.isAlive()) {
            if (isReachable()) {
                if (!started) {
                    AgentLog.info("NetEase API service already running at " + NetEaseConfig.baseUrl());
                    started = true;
                }
                return true;
            }
            // 进程活着但不可达 — 可能只是忙，等待重试而非重启
            AgentLog.info("NetEase: API process alive but not reachable, waiting...");
            return false;
        }

        // 进程不存在，检查是否有外部服务在跑
        if (isReachable()) {
            if (!started) {
                AgentLog.info("NetEase API service already running at " + NetEaseConfig.baseUrl());
                started = true;
            }
            return true;
        }

        // 方式一：独立 exe（优先，无需 Node.js）
        Path exePath = findExe();
        if (exePath != null) {
            AgentLog.info("NetEase: found standalone exe at " + exePath);
            if (startProcess(exePath.toString())) return true;
        }

        // 方式二：Node.js + app.js（需要克隆仓库）
        Path apiDir = findApiEnhancedDir();
        if (apiDir != null && Files.isRegularFile(apiDir.resolve("app.js"))
                && Files.isDirectory(apiDir.resolve("node_modules"))) {
            String nodeExe = findNodeExe();
            if (nodeExe != null) {
                AgentLog.info("NetEase: trying Node.js + app.js at " + apiDir);
                if (startProcess(nodeExe, "app.js", apiDir)) return true;
            }
        }

        // 方式三：自动下载 exe
        Path downloadTarget = getDownloadTarget();
        if (downloadTarget != null && !Files.exists(downloadTarget)) {
            AgentLog.info("NetEase: attempting auto-download exe to " + downloadTarget);
            if (downloadExe(downloadTarget)) {
                if (startProcess(downloadTarget.toString())) return true;
            }
        }

        if (startError.isEmpty()) {
            startError = "无法启动 API 服务。请下载 ncm-api-win-x64.exe 放到项目根目录或 %LOCALAPPDATA%\\MyiUI\\";
        }
        AgentLog.error("NetEase: " + startError, null);
        return false;
    }

    /** 启动独立 exe 进程。 */
    private static boolean startProcess(String exePath) {
        return startProcess(exePath, null, null);
    }

    /** 启动进程（通用），等待最多 15 秒服务就绪。 */
    private static boolean startProcess(String exe, String arg, Path workDir) {
        try {
            List<String> cmd = new ArrayList<>();
            cmd.add(exe);
            if (arg != null) cmd.add(arg);

            ProcessBuilder pb = new ProcessBuilder(cmd);
            if (workDir != null) pb.directory(workDir.toFile());
            pb.redirectErrorStream(true);
            // 设置端口
            pb.environment().put("PORT", extractPort(NetEaseConfig.baseUrl()));
            // 清除代理环境变量（避免干扰）
            pb.environment().remove("http_proxy");
            pb.environment().remove("https_proxy");
            pb.environment().remove("HTTP_PROXY");
            pb.environment().remove("HTTPS_PROXY");

            apiProcess = pb.start();
            AgentLog.info("NetEase: starting API process: " + String.join(" ", cmd)
                    + " (pid=" + apiProcess.pid() + ")");

            // 等待服务就绪（最多 15 秒）
            for (int i = 0; i < 30; i++) {
                try { Thread.sleep(500); } catch (InterruptedException e) { Thread.currentThread().interrupt(); break; }
                if (isReachable()) {
                    started = true;
                    AgentLog.info("NetEase API service started successfully");
                    return true;
                }
                if (apiProcess != null && !apiProcess.isAlive()) {
                    startError = "API 进程启动后立即退出";
                    AgentLog.error("NetEase: " + startError, null);
                    apiProcess = null;
                    return false;
                }
            }
            startError = "API 启动超时（15s）";
            AgentLog.error("NetEase: " + startError, null);
            return false;
        } catch (IOException e) {
            startError = "启动 API 失败: " + e.getMessage();
            AgentLog.error("NetEase: " + startError, e);
            return false;
        }
    }

    /** 停止由本类启动的 API 进程。 */
    public static void shutdown() {
        if (apiProcess != null && apiProcess.isAlive()) {
            apiProcess.destroy();
            AgentLog.info("NetEase API service stopped");
        }
        apiProcess = null;
    }

    public static String startError() { return startError; }
    public static boolean wasStarted() { return started; }

    // ── 查找独立 exe ──

    private static Path findExe() {
        List<Path> candidates = new ArrayList<>();
        String root = System.getenv("MYIUI_ROOT");
        String local = System.getenv("LOCALAPPDATA");

        if (root != null && !root.isBlank()) {
            candidates.add(Paths.get(root, EXE_FILENAME));
            candidates.add(Paths.get(root, "api-enhanced", EXE_FILENAME));
        }
        if (local != null) {
            candidates.add(Paths.get(local, "MyiUI", EXE_FILENAME));
            candidates.add(Paths.get(local, "MyiUI", "api-enhanced", EXE_FILENAME));
            Path marker = Paths.get(local, "MyiUI", "project_root.txt");
            if (Files.isRegularFile(marker)) {
                try {
                    String pr = Files.readString(marker, StandardCharsets.UTF_8).trim();
                    if (!pr.isBlank()) {
                        candidates.add(Paths.get(pr, EXE_FILENAME));
                        candidates.add(Paths.get(pr, "api-enhanced", EXE_FILENAME));
                    }
                } catch (IOException ignored) {}
            }
        }
        candidates.add(Paths.get(System.getProperty("user.dir"), EXE_FILENAME));

        for (Path p : candidates) {
            try {
                if (Files.isRegularFile(p) && Files.size(p) > 1024) {
                    return p;
                }
            } catch (IOException ignored) {}
        }
        return null;
    }

    // ── 自动下载 exe ──

    private static Path getDownloadTarget() {
        String local = System.getenv("LOCALAPPDATA");
        if (local == null) return null;
        Path dir = Paths.get(local, "MyiUI");
        try { Files.createDirectories(dir); } catch (IOException e) { return null; }
        return dir.resolve(EXE_FILENAME);
    }

    private static boolean downloadExe(Path target) {
        try {
            AgentLog.info("NetEase: downloading " + EXE_FILENAME + " from GitHub Releases...");
            HttpURLConnection conn = (HttpURLConnection) URI.create(EXE_DOWNLOAD_URL).toURL().openConnection();
            conn.setConnectTimeout(10000);
            conn.setReadTimeout(60000);
            conn.setRequestProperty("User-Agent", "MyiUI/1.0");
            conn.setInstanceFollowRedirects(true);
            int code = conn.getResponseCode();
            if (code < 200 || code >= 300) {
                conn.disconnect();
                startError = "下载 exe 失败: HTTP " + code;
                return false;
            }
            try (var in = conn.getInputStream()) {
                Files.copy(in, target, java.nio.file.StandardCopyOption.REPLACE_EXISTING);
            }
            conn.disconnect();
            long size = Files.size(target);
            AgentLog.info("NetEase: downloaded exe (" + (size / 1024 / 1024) + " MB) to " + target);
            return size > 1024 * 1024;  // 至少 1MB 才算下载成功
        } catch (Throwable t) {
            startError = "下载 exe 失败: " + t.getMessage();
            AgentLog.error("NetEase: " + startError, t);
            try { Files.deleteIfExists(target); } catch (IOException ignored) {}
            return false;
        }
    }

    // ── 查找 Node.js + api-enhanced 目录（备选方案） ──

    private static Path findApiEnhancedDir() {
        List<Path> candidates = new ArrayList<>();
        String root = System.getenv("MYIUI_ROOT");
        String local = System.getenv("LOCALAPPDATA");

        if (root != null && !root.isBlank()) candidates.add(Paths.get(root, "api-enhanced"));
        if (local != null) {
            candidates.add(Paths.get(local, "MyiUI", "api-enhanced"));
            Path marker = Paths.get(local, "MyiUI", "project_root.txt");
            if (Files.isRegularFile(marker)) {
                try {
                    String pr = Files.readString(marker, StandardCharsets.UTF_8).trim();
                    if (!pr.isBlank()) candidates.add(Paths.get(pr, "api-enhanced"));
                } catch (IOException ignored) {}
            }
        }
        candidates.add(Paths.get(System.getProperty("user.dir"), "api-enhanced"));

        for (Path p : candidates) {
            if (Files.isDirectory(p) && Files.isRegularFile(p.resolve("app.js"))) return p;
        }
        return null;
    }

    private static String findNodeExe() {
        try {
            Process p = new ProcessBuilder("node", "--version").redirectErrorStream(true).start();
            if (p.waitFor(3, TimeUnit.SECONDS) && p.exitValue() == 0) return "node";
        } catch (Throwable ignored) {}
        String[] paths = {
                "C:\\Program Files\\nodejs\\node.exe",
                "C:\\Program Files (x86)\\nodejs\\node.exe",
        };
        for (String p : paths) {
            if (new File(p).isFile()) return p;
        }
        String localAppData = System.getenv("LOCALAPPDATA");
        if (localAppData != null) {
            Path nvmDir = Paths.get(localAppData, "nvm");
            if (Files.isDirectory(nvmDir)) {
                try (var stream = Files.list(nvmDir)) {
                    var nodeExe = stream.filter(Files::isDirectory)
                            .map(d -> d.resolve("node.exe"))
                            .filter(Files::isRegularFile)
                            .findFirst();
                    if (nodeExe.isPresent()) return nodeExe.get().toString();
                } catch (IOException ignored) {}
            }
        }
        return null;
    }

    private static String extractPort(String baseUrl) {
        try {
            var url = URI.create(baseUrl).toURL();
            int port = url.getPort();
            return port > 0 ? String.valueOf(port) : "3000";
        } catch (Throwable t) {
            return "3000";
        }
    }
}
