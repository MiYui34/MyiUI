package com.myiui.agent.mapping;

import com.myiui.agent.AgentLog;

/**
 * Runtime Minecraft version detection.
 *
 * <p>The agent is a single jar that must run on every Fabric 1.21 client (1.21 through 1.21.11).
 * Because Fabric
 * intermediary names are stable across versions, the primary compatibility mechanism is the
 * "try every known candidate" lookup in {@code ReflectUtil}. This class is the auxiliary layer:
 * it resolves the concrete game version (e.g. {@code "1.21.4"}) so we can log which profile is
 * running and, in the rare case where two versions expose a same-named member with different
 * semantics, pick the right candidate deterministically.
 */
public final class McVersion {
    private McVersion() {}

    /** Parsed feature version, e.g. 1 for the "1" in 1.21.4. Always 1 for modern MC. */
    private static volatile int major = -1;
    /** Parsed minor, e.g. 21 for 1.21.4. */
    private static volatile int minor = -1;
    /** Parsed patch, e.g. 4 for 1.21.4. 0 when absent (plain "1.21"). */
    private static volatile int patch = -1;
    private static volatile String raw = "";
    private static volatile boolean detected = false;

    /**
     * Attempt to detect and cache the version from a MinecraftClient-supplied version string.
     * Safe to call repeatedly; only the first successful non-empty parse sticks.
     */
    public static synchronized void detectFromString(String versionString) {
        if (detected) {
            return;
        }
        if (versionString == null || versionString.isBlank()) {
            return;
        }
        raw = versionString.trim();
        parse(raw);
        detected = major > 0;
        if (detected) {
            AgentLog.info("McVersion detected: " + raw + " -> " + major + "." + minor + "." + patch);
            if (!isSupportedFabric121()) {
                AgentLog.error("Unsupported MC version for this build: " + raw
                        + " (supported: Fabric 1.21 - 1.21.11)");
            }
        }
    }

    /** True when detected version is within the supported Fabric 1.21 - 1.21.11 range. */
    public static boolean isSupportedFabric121() {
        if (!detected) {
            return true;
        }
        return major == 1 && minor == 21 && patch >= 0 && patch <= 11;
    }

    private static void parse(String s) {
        // Version strings look like "1.21", "1.21.4", "1.21.6-rc1", "24w13a" (snapshot).
        // Extract the leading dotted-numeric prefix; snapshots stay unresolved (major<0).
        int end = 0;
        while (end < s.length() && (Character.isDigit(s.charAt(end)) || s.charAt(end) == '.')) {
            end++;
        }
        String numeric = s.substring(0, end);
        String[] parts = numeric.split("\\.");
        try {
            if (parts.length >= 1 && !parts[0].isEmpty()) {
                major = Integer.parseInt(parts[0]);
            }
            minor = parts.length >= 2 && !parts[1].isEmpty() ? Integer.parseInt(parts[1]) : 0;
            patch = parts.length >= 3 && !parts[2].isEmpty() ? Integer.parseInt(parts[2]) : 0;
        } catch (NumberFormatException e) {
            major = -1;
            minor = -1;
            patch = -1;
        }
    }

    public static boolean isDetected() {
        return detected;
    }

    public static String raw() {
        return raw;
    }

    public static int major() {
        return major;
    }

    public static int minor() {
        return minor;
    }

    public static int patch() {
        return patch;
    }

    /** True when the detected version is >= the given major.minor.patch. Unknown versions return true (optimistic). */
    public static boolean atLeast(int wantMajor, int wantMinor, int wantPatch) {
        if (!detected) {
            return true;
        }
        if (major != wantMajor) {
            return major > wantMajor;
        }
        if (minor != wantMinor) {
            return minor > wantMinor;
        }
        return patch >= wantPatch;
    }

    /** True when the detected version is <= the given major.minor.patch. Unknown versions return true (optimistic). */
    public static boolean atMost(int wantMajor, int wantMinor, int wantPatch) {
        if (!detected) {
            return true;
        }
        if (major != wantMajor) {
            return major < wantMajor;
        }
        if (minor != wantMinor) {
            return minor < wantMinor;
        }
        return patch <= wantPatch;
    }
}
