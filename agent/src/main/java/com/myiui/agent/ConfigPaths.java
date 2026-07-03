package com.myiui.agent;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;

final class ConfigPaths {
    private ConfigPaths() {}

    static Path projectRoot() {
        String env = System.getenv("MYIUI_ROOT");
        if (env != null && !env.isBlank()) {
            return Paths.get(env);
        }

        String local = System.getenv("LOCALAPPDATA");
        if (local != null) {
            Path marker = Paths.get(local, "MyiUI", "project_root.txt");
            if (Files.isRegularFile(marker)) {
                try {
                    String root = Files.readString(marker, StandardCharsets.UTF_8).trim();
                    if (!root.isBlank()) {
                        return Paths.get(root);
                    }
                } catch (IOException ignored) {
                }
            }

            Path runtimeRoot = Paths.get(local, "MyiUI", "runtime");
            if (Files.isRegularFile(runtimeRoot.resolve("config/menu/theme.json"))) {
                return runtimeRoot;
            }
        }

        return Paths.get(System.getProperty("user.dir"));
    }

    static Path backgroundJson() {
        String local = System.getenv("LOCALAPPDATA");
        if (local != null) {
            Path runtime = Paths.get(local, "MyiUI", "runtime", "config", "menu", "background.json");
            if (Files.isRegularFile(runtime)) {
                return runtime;
            }
        }

        Path root = projectRoot();
        Path inRoot = root.resolve("config").resolve("menu").resolve("background.json");
        if (Files.isRegularFile(inRoot)) {
            return inRoot;
        }
        if (local != null) {
            return Paths.get(local, "MyiUI", "runtime", "config", "menu", "background.json");
        }
        return root.resolve("background.json");
    }
}
