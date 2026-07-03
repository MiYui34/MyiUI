package com.myiui.agent;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;

public final class AgentLog {
    private static volatile boolean initialized = false;

    private AgentLog() {}

    public static void init() {
        initialized = true;
        try {
            String base = System.getenv("LOCALAPPDATA");
            if (base == null) base = System.getProperty("java.io.tmpdir");
            Path dir = Path.of(base, "MyiUI");
            Files.createDirectories(dir);
            Path log = dir.resolve("agent.log");
            Files.writeString(log, "=== Agent session " + java.time.LocalDateTime.now() + " ===\n",
                    StandardCharsets.UTF_8, StandardOpenOption.CREATE, StandardOpenOption.APPEND);
        } catch (IOException e) {
            System.err.println("[MyiUI] AgentLog init failed: " + e.getMessage());
        }
        info("AgentLog initialized for classloader " + AgentLog.class.getClassLoader());
    }

    public static void info(String message) {
        log(message, false);
    }

    public static void error(String message) {
        log(message, true);
    }

    public static void error(String message, Throwable t) {
        log(message, true);
        if (t != null) {
            java.io.StringWriter sw = new java.io.StringWriter();
            t.printStackTrace(new java.io.PrintWriter(sw));
            log(sw.toString(), true);
        }
    }

    private static void log(String message, boolean err) {
        String line = "[MyiUI] " + message;
        if (err) System.err.println(line);
        else System.out.println(line);
        try {
            String base = System.getenv("LOCALAPPDATA");
            if (base == null) base = System.getProperty("java.io.tmpdir");
            Path log = Path.of(base, "MyiUI", "agent.log");
            Files.writeString(log, line + "\n",
                    StandardCharsets.UTF_8, StandardOpenOption.CREATE, StandardOpenOption.APPEND);
        } catch (IOException ignored) {
        }
    }
}
