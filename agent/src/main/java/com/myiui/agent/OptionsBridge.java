package com.myiui.agent;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.function.BiFunction;

public final class OptionsBridge {
    private OptionsBridge() {}

    private static final Object OPTION_WRITE_LOCK = new Object();

    private record OptionRef(String jsonKey, String getterNamed, String getterIntermediary,
                               String fieldNamed, String fieldIntermediary) {}

    @FunctionalInterface
    private interface OptionWriter {
        Boolean apply(Object options, ClassLoader loader, String uiValue);
    }

    private record Binding(String category, BiFunction<Object, ClassLoader, String> reader,
                           OptionWriter writer) {}

    private static final List<OptionRef> SIMPLE_OPTION_REFS = List.of(
            new OptionRef("fullscreen", "getFullscreen", "method_42447", "fullscreen", "field_1857"),
            new OptionRef("gui_scale", "getGuiScale", "method_42474", "guiScale", "field_1868"),
            new OptionRef("fov", "getFov", "method_41808", "fov", "field_1826"),
            new OptionRef("brightness", "getGamma", "method_42473", "gamma", "field_1840"),
            new OptionRef("graphics", "getGraphicsMode", "method_42534", "graphicsMode", "field_25444"),
            new OptionRef("render_distance", "getViewDistance", "method_42503", "viewDistance", "field_1870"),
            new OptionRef("vsync", "getEnableVsync", "method_42433", "enableVsync", "field_1884"),
            new OptionRef("max_fps", "getMaxFps", "method_42524", "maxFps", "field_1909"),
            new OptionRef("subtitles", "getShowSubtitles", "method_42443", "showSubtitles", "field_1818"),
            new OptionRef("force_unicode", "getForceUnicodeFont", "method_42437", "forceUnicodeFont", "field_1819"),
            new OptionRef("chat_opacity", "getChatOpacity", "method_42542", "chatOpacity", "field_1820"),
            new OptionRef("chat_scale", "getChatScale", "method_42554", "chatScale", "field_1908"),
            new OptionRef("chat_width", "getChatWidth", "method_42556", "chatWidth", "field_1915"),
            new OptionRef("chat_colors", "getChatColors", "method_42427", "chatColors", "field_1900"),
            new OptionRef("chat_links", "getChatLinks", "method_42429", "chatLinks", "field_1911"),
            new OptionRef("chat_narrator", "getNarrator", "method_42476", "narrator", "field_1896"),
            new OptionRef("narrator", "getNarrator", "method_42476", "narrator", "field_1896"),
            new OptionRef("high_contrast", "getHighContrast", "method_49600", "highContrast", "field_43044"),
            new OptionRef("distortion_effects", "getDistortionEffectScale", "method_42453", "distortionEffectScale", "field_26675"),
            new OptionRef("screen_shake", "getDamageTiltStrength", "method_48974", "damageTiltStrength", "field_42482"),
            new OptionRef("fov_effects", "getFovEffectScale", "method_42454", "fovEffectScale", "field_26676"),
            new OptionRef("auto_jump", "getAutoJump", "method_42423", "autoJump", "field_1848"),
            new OptionRef("toggle_sneak", "getSneakToggled", "method_42449", "sneakToggled", "field_21332"),
            new OptionRef("toggle_sprint", "getSprintToggled", "method_42450", "sprintToggled", "field_21333"),
            new OptionRef("skin_model", "getMainArm", "method_42552", "mainArm", "field_1829"),
            new OptionRef("chat_visible", "getChatVisibility", "method_42539", "chatVisibility", "field_1877"),
            new OptionRef("output_device", "getSoundDevice", "method_42477", "soundDevice", "field_34783"),
            new OptionRef("sensitivity", "getMouseSensitivity", "method_42495", "mouseSensitivity", "field_1843"),
            new OptionRef("invert_y", "getInvertYMouse", "method_42438", "invertYMouse", "field_1865"),
            new OptionRef("discrete_scroll", "getDiscreteMouseScroll", "method_42439", "discreteMouseScroll", "field_19244")
    );

    private static final Map<String, Binding> BINDINGS = new LinkedHashMap<>();

    static {
        for (OptionRef ref : SIMPLE_OPTION_REFS) {
            registerSimple(ref);
        }
        registerSound("options_sound", "master_volume", "MASTER");
        registerSound("options_sound", "music_volume", "MUSIC");
        registerSound("options_sound", "weather_volume", "WEATHER");
        registerSound("options_sound", "block_volume", "BLOCKS");
        registerSound("options_sound", "hostile_volume", "HOSTILE");
        registerSound("options_sound", "neutral_volume", "NEUTRAL");
        registerSound("options_sound", "player_volume", "PLAYERS");
        registerLanguage();
        registerModelPart("options_skin", "cape", "CAPE");
        registerModelPart("options_skin", "jacket", "JACKET");
        registerModelPart("options_skin", "hat", "HAT");
    }

    public static String getOptionsJson(String category) {
        if (GameActions.resolveClientForBridge() == null) {
            return null;
        }
        return GameActions.readOnGameOptions(options -> buildOptionsJson(options, category), null);
    }

    private static String buildOptionsJson(Object options, String category) {
        try {
            ClassLoader loader = options.getClass().getClassLoader();
            StringBuilder sb = new StringBuilder("{");
            boolean first = true;
            for (Map.Entry<String, Binding> entry : BINDINGS.entrySet()) {
                if (!entry.getValue().category().equals(category)) continue;
                String value = entry.getValue().reader().apply(options, loader);
                if (!first) sb.append(',');
                first = false;
                appendJsonValue(sb, entry.getKey(), value);
            }
            sb.append("}");
            return sb.toString();
        } catch (Throwable e) {
            AgentLog.error("GET_OPTIONS failed: " + category, e);
            return null;
        }
    }

    public static boolean setOption(String key, String value) {
        synchronized (OPTION_WRITE_LOCK) {
            final String normalized = normalizeKey(key);
            Binding binding = BINDINGS.get(normalized);
            if (binding == null) {
                if ("gamma".equals(normalized)) {
                    binding = BINDINGS.get("brightness");
                }
            }
            if (binding == null) {
                AgentLog.error("未知选项: " + key);
                return false;
            }
            final Binding resolved = binding;
            final String uiValue = value;
            GameActions.OptionsWriteResult result = GameActions.writeOnGameOptions(options -> {
                ClassLoader loader = options.getClass().getClassLoader();
                return resolved.writer().apply(options, loader, uiValue);
            });
            if (!result.scheduledOnMainThread()) {
                AgentLog.error("选项写入未执行（主线程调度失败）: " + key);
            } else if (!result.valueWritten()) {
                AgentLog.error("选项写入失败: " + key + "=" + value);
            }
            return result.success();
        }
    }

    private static void registerSimple(OptionRef ref) {
        String category = categoryForKey(ref.jsonKey());
        BINDINGS.put(ref.jsonKey(), new Binding(category,
                (options, loader) -> readSimpleOption(options, ref),
                (options, loader, uiValue) -> writeSimpleOption(options, ref, uiValue)));
    }

    private static void registerSound(String category, String key, String soundCategoryName) {
        BINDINGS.put(key, new Binding(category,
                (options, loader) -> readSoundVolume(options, loader, soundCategoryName),
                (options, loader, uiValue) -> writeSoundVolume(options, loader, soundCategoryName, uiValue)));
    }

    private static void registerLanguage() {
        BINDINGS.put("language", new Binding("options_language",
                (options, loader) -> {
                    try {
                        Object lang = ReflectUtil.getField(options, "language", "field_1883");
                        return languageCodeToLabel(String.valueOf(lang));
                    } catch (ReflectiveOperationException e) {
                        return "";
                    }
                },
                (options, loader, uiValue) -> {
                    try {
                        String code = languageLabelToCode(uiValue);
                        if (code == null) return false;
                        Field field = findField(options.getClass(), "language", "field_1883");
                        if (field == null) return false;
                        field.setAccessible(true);
                        field.set(options, code);
                        return true;
                    } catch (ReflectiveOperationException e) {
                        return false;
                    }
                }));
    }

    private static void registerModelPart(String category, String key, String partName) {
        BINDINGS.put(key, new Binding(category,
                (options, loader) -> readModelPart(options, loader, partName),
                (options, loader, uiValue) -> writeModelPart(options, loader, partName, uiValue)));
    }

    private static String categoryForKey(String key) {
        return switch (key) {
            case "fullscreen", "gui_scale", "fov", "brightness", "graphics", "render_distance", "vsync", "max_fps" ->
                    "options_video";
            case "master_volume", "music_volume", "weather_volume", "block_volume", "hostile_volume", "neutral_volume",
                 "player_volume", "output_device", "subtitles" -> "options_sound";
            case "language", "force_unicode" -> "options_language";
            case "chat_visible", "chat_opacity", "chat_scale", "chat_width", "chat_colors", "chat_links", "chat_narrator" ->
                    "options_chat";
            case "narrator", "high_contrast", "distortion_effects", "screen_shake", "fov_effects",
                 "auto_jump", "toggle_sneak", "toggle_sprint" -> "options_accessibility";
            case "skin_model", "cape", "jacket", "hat" -> "options_skin";
            case "sensitivity", "invert_y", "discrete_scroll" -> "options_controls";
            default -> "options_video";
        };
    }

    private static String readSimpleOption(Object options, OptionRef ref) {
        Object holder = resolveOptionHolder(options, ref);
        Object raw = unwrapOptionValue(holder);
        if (raw == null) return "";
        return formatUiValue(ref.jsonKey(), raw);
    }

    private static boolean writeSimpleOption(Object options, OptionRef ref, String uiValue) {
        Object holder = resolveOptionHolder(options, ref);
        if (holder == null) {
            AgentLog.error("选项 holder 为空: " + ref.jsonKey());
            return false;
        }
        if (!isSimpleOption(holder.getClass())) {
            AgentLog.error("选项 holder 类型异常: " + ref.jsonKey() + " -> " + holder.getClass().getName());
            return false;
        }
        Object parsed = parseUiValue(ref.jsonKey(), holder, uiValue);
        if (parsed == null) {
            AgentLog.error("选项值解析失败: " + ref.jsonKey() + "=" + uiValue);
            return false;
        }
        return invokeSetValue(holder, parsed);
    }

    private static String readSoundVolume(Object options, ClassLoader loader, String categoryName) {
        try {
            Object category = resolveSoundCategory(loader, categoryName);
            if (category == null) {
                return "";
            }
            ReflectiveOperationException last = null;
            for (String[] getter : new String[][]{
                    {"getCategorySoundVolume", "method_1630"},
                    {"getSoundVolume", "method_71978"},
                    {"getCategorySoundVolume", "method_42432"},
                    {"getSoundVolume", "method_42431"},
            }) {
                try {
                    Method method = ReflectUtil.findInstanceMethod(options.getClass(), getter[0], getter[1],
                            category.getClass());
                    Object raw = method.invoke(options, category);
                    if (raw instanceof Number n) {
                        return formatVolumePercent(n.doubleValue());
                    }
                } catch (ReflectiveOperationException e) {
                    last = e;
                }
            }
            Method scanned = findCategoryVolumeMethod(options.getClass(), category.getClass());
            if (scanned != null) {
                Object raw = scanned.invoke(options, category);
                if (raw instanceof Number n) {
                    return formatVolumePercent(n.doubleValue());
                }
            }
            Object holder = resolveSoundVolumeOption(options, loader, categoryName);
            Object raw = unwrapOptionValue(holder);
            if (raw instanceof Number n) {
                return formatVolumePercent(n.doubleValue());
            }
            if (last != null) {
                throw last;
            }
            return "";
        } catch (ReflectiveOperationException e) {
            AgentLog.error("readSoundVolume failed: " + categoryName, e);
            return "";
        }
    }

    private static Method findCategoryVolumeMethod(Class<?> optionsClass, Class<?> categoryClass) {
        Method named = null;
        Method fallback = null;
        for (Method method : optionsClass.getMethods()) {
            if (method.getParameterCount() != 1) {
                continue;
            }
            if (!categoryClass.isAssignableFrom(method.getParameterTypes()[0])) {
                continue;
            }
            Class<?> ret = method.getReturnType();
            if (ret != float.class && ret != Float.class && ret != double.class && ret != Double.class) {
                continue;
            }
            if ("getCategorySoundVolume".equals(method.getName()) || "method_1630".equals(method.getName())) {
                named = method;
                break;
            }
            if ("getSoundVolume".equals(method.getName())) {
                fallback = method;
            }
        }
        Method chosen = named != null ? named : fallback;
        if (chosen != null) {
            chosen.setAccessible(true);
        }
        return chosen;
    }

    private static String formatVolumePercent(double raw) {
        if (raw <= 1.0 && raw >= 0.0) {
            return String.valueOf(Math.round(raw * 100.0));
        }
        return String.valueOf(Math.round(raw));
    }

    private static boolean writeSoundVolume(Object options, ClassLoader loader, String categoryName, String uiValue) {
        try {
            Object holder = resolveSoundVolumeOption(options, loader, categoryName);
            if (holder == null) {
                AgentLog.error("音量选项 holder 为空: " + categoryName);
                return false;
            }
            double volume = Double.parseDouble(uiValue) / 100.0;
            for (Object candidate : new Object[]{
                    Double.valueOf(volume),
                    volume,
                    Float.valueOf((float) volume),
                    (float) volume
            }) {
                if (invokeSetValue(holder, candidate)) {
                    return true;
                }
            }
            AgentLog.error("音量 setValue 全部失败: " + categoryName + "=" + uiValue
                    + " holder=" + holder.getClass().getName());
            return false;
        } catch (ReflectiveOperationException | NumberFormatException e) {
            AgentLog.error("音量写入异常: " + categoryName + "=" + uiValue, e);
            return false;
        }
    }

    private static Object resolveSoundVolumeOption(Object options, ClassLoader loader, String categoryName)
            throws ReflectiveOperationException {
        Object category = resolveSoundCategory(loader, categoryName);
        if (category == null) {
            AgentLog.error("SoundCategory 解析失败: " + categoryName);
            return null;
        }
        ReflectiveOperationException last = null;
        for (String[] getter : new String[][]{{"getSoundVolumeOption", "method_45578"}}) {
            try {
                Method method = ReflectUtil.findInstanceMethod(options.getClass(), getter[0], getter[1],
                        category.getClass());
                Object holder = method.invoke(options, category);
                if (holder != null) {
                    return holder;
                }
            } catch (ReflectiveOperationException e) {
                last = e;
            }
        }
        try {
            Object map = ReflectUtil.getField(options, "soundVolumeLevels", "field_1916");
            if (map instanceof Map<?, ?> volumeMap) {
                Object holder = volumeMap.get(category);
                if (holder != null) {
                    return holder;
                }
            }
        } catch (ReflectiveOperationException e) {
            last = e;
        }
        if (last != null) {
            throw last;
        }
        return null;
    }

    private static Object resolveSoundCategory(ClassLoader loader, String categoryName)
            throws ReflectiveOperationException {
        ReflectiveOperationException last = null;
        for (String className : new String[]{
                "net.minecraft.sound.SoundCategory",
                "net.minecraft.class_3419"
        }) {
            try {
                Class<?> soundCategoryClass = Class.forName(className, true, loader);
                if (!soundCategoryClass.isEnum()) {
                    continue;
                }
                @SuppressWarnings({"unchecked", "rawtypes"})
                Object category = Enum.valueOf((Class<? extends Enum>) soundCategoryClass, categoryName);
                return category;
            } catch (ClassNotFoundException | IllegalArgumentException e) {
                last = new ReflectiveOperationException(e);
            }
        }
        if (last != null) {
            throw last;
        }
        return null;
    }

    private static String readModelPart(Object options, ClassLoader loader, String partName) {
        try {
            Method isEnabled = ReflectUtil.findInstanceMethod(options.getClass(), "isPlayerModelPartEnabled",
                    "method_32594", Class.forName("net.minecraft.entity.player.PlayerModelPart", true, loader));
            Object part = enumConstant(loader, "net.minecraft.entity.player.PlayerModelPart", partName);
            Object result = isEnabled.invoke(options, part);
            return Boolean.TRUE.equals(result) ? "true" : "false";
        } catch (ReflectiveOperationException e) {
            return "";
        }
    }

    private static boolean writeModelPart(Object options, ClassLoader loader, String partName, String uiValue) {
        try {
            Class<?> partClass = Class.forName("net.minecraft.entity.player.PlayerModelPart", true, loader);
            Object part = enumConstant(loader, "net.minecraft.entity.player.PlayerModelPart", partName);
            boolean enabled = "true".equalsIgnoreCase(uiValue) || "1".equals(uiValue);
            Method setter = ReflectUtil.findInstanceMethod(options.getClass(), "setPlayerModelPart", "method_1635",
                    partClass, boolean.class);
            setter.invoke(options, part, enabled);
            return true;
        } catch (ReflectiveOperationException e) {
            return false;
        }
    }

    private static Object resolveOptionHolder(Object options, OptionRef ref) {
        try {
            Method getter = ReflectUtil.findInstanceMethod(options.getClass(), ref.getterNamed(), ref.getterIntermediary());
            Object holder = getter.invoke(options);
            if (holder != null) {
                return holder;
            }
        } catch (Throwable ignored) {
        }
        try {
            Object holder = ReflectUtil.getField(options, ref.fieldNamed(), ref.fieldIntermediary());
            if (holder != null) {
                return holder;
            }
        } catch (Throwable ignored) {
        }
        // Runtime fallback across versions: locate the SimpleOption-typed field whose name matches
        // any known candidate, independent of exact getter/field intermediary numbering.
        return scanSimpleOptionByName(options, ref.fieldNamed(), ref.fieldIntermediary(), ref.getterNamed());
    }

    private static Object scanSimpleOptionByName(Object options, String... nameCandidates) {
        Class<?> clazz = options.getClass();
        while (clazz != null && clazz != Object.class) {
            for (Field f : clazz.getDeclaredFields()) {
                if (!isSimpleOption(f.getType())) {
                    continue;
                }
                for (String candidate : nameCandidates) {
                    if (candidate != null && candidate.equals(f.getName())) {
                        try {
                            f.setAccessible(true);
                            return f.get(options);
                        } catch (ReflectiveOperationException ignored) {
                        }
                    }
                }
            }
            clazz = clazz.getSuperclass();
        }
        return null;
    }

    private static Object unwrapOptionValue(Object holder) {
        if (holder == null) return null;
        if (!isSimpleOption(holder.getClass())) {
            return holder;
        }
        for (String[] getter : new String[][]{{"getValue", "method_41749"}}) {
            try {
                Method getValue = ReflectUtil.findInstanceMethod(holder.getClass(), getter[0], getter[1]);
                return getValue.invoke(holder);
            } catch (ReflectiveOperationException ignored) {
            }
        }
        return null;
    }

    private static String formatUiValue(String jsonKey, Object raw) {
        String key = normalizeKey(jsonKey);
        if ("brightness".equals(key) || "gamma".equals(key) || "chat_opacity".equals(key)) {
            double v = raw instanceof Number n ? n.doubleValue() : 0.5;
            if (v <= 1.0) return String.valueOf(Math.round(v * 100.0));
            return String.valueOf(Math.round(v));
        }
        if ("chat_scale".equals(key) || "screen_shake".equals(key) || "distortion_effects".equals(key)
                || "fov_effects".equals(key)) {
            if (raw instanceof Number n) {
                if ("chat_scale".equals(key)) {
                    double v = n.doubleValue();
                    if (v <= 1.5) return String.valueOf(Math.round(v * 100.0));
                    return String.valueOf(Math.round(v));
                }
                if ("distortion_effects".equals(key) || "fov_effects".equals(key)) {
                    return n.doubleValue() > 0.001 ? "true" : "false";
                }
                return String.valueOf(Math.round(n.doubleValue() * 100.0));
            }
        }
        if ("chat_width".equals(key) && raw instanceof Number n) {
            double normalized = n.doubleValue();
            if (normalized <= 1.5) {
                return String.valueOf(Math.round(200.0 + normalized * 600.0));
            }
            return String.valueOf(Math.round(normalized));
        }
        if ("graphics".equals(key)) {
            if (raw != null && raw.getClass().isEnum()) {
                return formatGraphicsLabel(((Enum<?>) raw).name());
            }
        }
        if ("output_device".equals(key)) {
            String s = String.valueOf(raw);
            if (s.isEmpty() || "default".equalsIgnoreCase(s)) return "系统默认";
            return s;
        }
        if ("render_distance".equals(key) || "max_fps".equals(key) || "gui_scale".equals(key) || "fov".equals(key)) {
            if (raw instanceof Number n) {
                return String.valueOf(n.intValue());
            }
        }
        if ("sensitivity".equals(key) && raw instanceof Number n) {
            return String.valueOf(Math.round(n.doubleValue() * 200.0));
        }
        if ("chat_visible".equals(key) && raw instanceof Enum<?> e) {
            return "HIDDEN".equals(e.name()) ? "false" : "true";
        }
        if (("chat_narrator".equals(key) || "narrator".equals(key)) && raw instanceof Enum<?> e) {
            if ("narrator".equals(key)) {
                return "OFF".equals(e.name()) ? "false" : "true";
            }
            return switch (e.name()) {
                case "OFF" -> "关闭";
                case "SYSTEM" -> "系统";
                case "ALL" -> "全部";
                default -> e.name().toLowerCase(Locale.ROOT);
            };
        }
        if ("skin_model".equals(key) && raw instanceof Enum<?> e) {
            return "RIGHT".equals(e.name()) ? "纤细（Alex）" : "经典（Steve）";
        }
        if (raw instanceof Boolean b) {
            return b ? "true" : "false";
        }
        if (raw instanceof Enum<?> e) {
            return e.name().toLowerCase(Locale.ROOT);
        }
        if (raw instanceof Number n) {
            if ("max_fps".equals(key) && n.intValue() >= 260) {
                return "无限制";
            }
            if (n.doubleValue() == Math.rint(n.doubleValue())) {
                return String.valueOf(n.intValue());
            }
            return String.valueOf(n);
        }
        return String.valueOf(raw);
    }

    private static Object parseUiValue(String jsonKey, Object holder, String value) {
        Object current = unwrapOptionValue(holder);
        Class<?> valueType = current != null ? current.getClass() : String.class;
        String key = normalizeKey(jsonKey);

        if ("max_fps".equals(key)) {
            if ("无限制".equals(value) || "unlimited".equalsIgnoreCase(value)) {
                return 260;
            }
            try {
                return Integer.parseInt(value);
            } catch (NumberFormatException e) {
                return null;
            }
        }

        if ("gui_scale".equals(key) || "fov".equals(key) || "render_distance".equals(key)) {
            try {
                return Integer.parseInt(value);
            } catch (NumberFormatException e) {
                return null;
            }
        }

        if ("sensitivity".equals(key)) {
            try {
                return Double.parseDouble(value) / 200.0;
            } catch (NumberFormatException e) {
                return null;
            }
        }

        if ("brightness".equals(key) || "gamma".equals(key) || "chat_opacity".equals(key)
                || key.endsWith("_volume")) {
            try {
                double v = Double.parseDouble(value);
                if (v > 1.0) v /= 100.0;
                return v;
            } catch (NumberFormatException e) {
                return null;
            }
        }
        if ("chat_scale".equals(key)) {
            try {
                double v = Double.parseDouble(value);
                if (v > 1.5) return v / 100.0;
                return v;
            } catch (NumberFormatException e) {
                return null;
            }
        }
        if ("chat_width".equals(key)) {
            try {
                double px = Double.parseDouble(value);
                if (px > 1.5) {
                    return Math.max(0.0, Math.min(1.0, (px - 200.0) / 600.0));
                }
                return px;
            } catch (NumberFormatException e) {
                return null;
            }
        }
        if ("screen_shake".equals(key)) {
            try {
                return Double.parseDouble(value) / 100.0;
            } catch (NumberFormatException e) {
                return null;
            }
        }
        if ("distortion_effects".equals(key) || "fov_effects".equals(key)) {
            boolean on = "true".equalsIgnoreCase(value) || "1".equals(value);
            return on ? 1.0 : 0.0;
        }
        if ("graphics".equals(key)) {
            return parseGraphicsMode(value, valueType);
        }
        if ("chat_visible".equals(key)) {
            boolean show = "true".equalsIgnoreCase(value) || "1".equals(value);
            return parseEnumValue(valueType, show ? "FULL" : "HIDDEN");
        }
        if ("chat_narrator".equals(key) || "narrator".equals(key)) {
            String mapped = switch (value) {
                case "关闭" -> "OFF";
                case "系统" -> "SYSTEM";
                case "全部" -> "ALL";
                default -> value.toUpperCase(Locale.ROOT);
            };
            if ("narrator".equals(key) && ("true".equalsIgnoreCase(value) || "false".equalsIgnoreCase(value))) {
                mapped = "true".equalsIgnoreCase(value) ? "ALL" : "OFF";
            }
            return parseEnumValue(valueType, mapped);
        }
        if ("skin_model".equals(key)) {
            String mapped = value.contains("Alex") || value.contains("纤细") ? "RIGHT" : "LEFT";
            return parseEnumValue(valueType, mapped);
        }
        if (valueType == Boolean.class || valueType == boolean.class) {
            return "true".equalsIgnoreCase(value) || "1".equals(value);
        }
        if (valueType == Integer.class || valueType == int.class) {
            try {
                return Integer.parseInt(value);
            } catch (NumberFormatException e) {
                return null;
            }
        }
        if (valueType == Double.class || valueType == double.class) {
            try {
                return Double.parseDouble(value);
            } catch (NumberFormatException e) {
                return null;
            }
        }
        if (valueType == Float.class || valueType == float.class) {
            try {
                return Float.parseFloat(value);
            } catch (NumberFormatException e) {
                return null;
            }
        }
        if (valueType.isEnum()) {
            return parseEnumValue(valueType, value);
        }
        return value;
    }

    private static Object parseGraphicsMode(String value, Class<?> valueType) {
        String mapped = switch (value) {
            case "快速", "流畅" -> "FAST";
            case "fancy", "FANCY", "华丽" -> "FANCY";
            case "极佳", "FABULOUS" -> "FABULOUS";
            default -> value.toUpperCase(Locale.ROOT);
        };
        Object parsed = parseEnumValue(valueType, mapped);
        if (parsed != null) {
            return parsed;
        }
        try {
            return enumConstant(valueType.getClassLoader(), valueType.getName(), mapped);
        } catch (ReflectiveOperationException e) {
            return null;
        }
    }

    private static String formatGraphicsLabel(String enumName) {
        return switch (enumName) {
            case "FAST" -> "快速";
            case "FANCY" -> "fancy";
            case "FABULOUS" -> "极佳";
            default -> enumName.toLowerCase(Locale.ROOT);
        };
    }

    private static Object parseEnumValue(Class<?> enumType, String value) {
        if (!enumType.isEnum()) return value;
        String upper = value.toUpperCase(Locale.ROOT);
        for (Object constant : enumType.getEnumConstants()) {
            if (constant.toString().equalsIgnoreCase(value) || ((Enum<?>) constant).name().equalsIgnoreCase(upper)) {
                return constant;
            }
        }
        return null;
    }

    private static Object enumConstant(ClassLoader loader, String className, String constantName)
            throws ReflectiveOperationException {
        Class<?> clazz = Class.forName(className, true, loader);
        if (!clazz.isEnum()) throw new ClassNotFoundException(className);
        @SuppressWarnings({"unchecked", "rawtypes"})
        Object value = Enum.valueOf((Class<? extends Enum>) clazz, constantName);
        return value;
    }

    private static boolean invokeSetValue(Object option, Object value) {
        if (option == null || value == null) return false;
        Method best = null;
        Object bestArg = null;
        int bestScore = -1;
        for (Method method : option.getClass().getMethods()) {
            if (method.getParameterCount() != 1) continue;
            if (!"setValue".equals(method.getName()) && !"method_41748".equals(method.getName())) continue;
            Class<?> param = method.getParameterTypes()[0];
            Object converted = coerceValue(value, param);
            Object arg = converted != null ? converted : value;
            int score;
            if (param.isInstance(arg)) {
                score = param.isPrimitive() ? 80 : 100;
            } else if (converted != null) {
                score = 60;
            } else {
                continue;
            }
            if (score > bestScore) {
                bestScore = score;
                best = method;
                bestArg = arg;
            }
        }
        if (best == null) {
            AgentLog.error("invokeSetValue 无匹配方法: " + option.getClass().getName() + " <- " + value);
            return false;
        }
        try {
            best.invoke(option, bestArg);
            return true;
        } catch (ReflectiveOperationException e) {
            AgentLog.error("invokeSetValue 失败: " + option.getClass().getName() + " <- " + bestArg
                    + " (" + e.getMessage() + ")");
            return false;
        }
    }

    private static Object coerceValue(Object value, Class<?> target) {
        if (value == null) return null;
        if (target.isInstance(value)) return value;
        if (target.isEnum() && value instanceof String s) {
            return parseEnumValue(target, s);
        }
        if ((target == int.class || target == Integer.class) && value instanceof Number n) {
            return n.intValue();
        }
        if ((target == double.class || target == Double.class) && value instanceof Number n) {
            return n.doubleValue();
        }
        if ((target == float.class || target == Float.class) && value instanceof Number n) {
            return n.floatValue();
        }
        if ((target == boolean.class || target == Boolean.class) && value instanceof Boolean b) {
            return b;
        }
        return null;
    }

    private static boolean isSimpleOption(Class<?> clazz) {
        String name = clazz.getName();
        return name.contains("SimpleOption") || name.contains("class_7172");
    }

    private static String normalizeKey(String key) {
        if (key == null) return "";
        return key.trim().toLowerCase(Locale.ROOT);
    }

    private static String languageCodeToLabel(String code) {
        if (code == null) return "";
        return switch (code.toLowerCase(Locale.ROOT)) {
            case "zh_cn" -> "简体中文";
            case "zh_tw" -> "繁體中文";
            case "en_us" -> "English";
            case "ja_jp" -> "日本語";
            case "ko_kr" -> "한국어";
            case "de_de" -> "Deutsch";
            case "fr_fr" -> "Français";
            default -> code;
        };
    }

    private static String languageLabelToCode(String label) {
        return switch (label) {
            case "简体中文" -> "zh_cn";
            case "繁體中文" -> "zh_tw";
            case "English" -> "en_us";
            case "日本語" -> "ja_jp";
            case "한국어" -> "ko_kr";
            case "Deutsch" -> "de_de";
            case "Français" -> "fr_fr";
            default -> null;
        };
    }

    private static Field findField(Class<?> clazz, String named, String intermediary) {
        Class<?> current = clazz;
        while (current != null && current != Object.class) {
            try {
                Field field = current.getDeclaredField(named);
                field.setAccessible(true);
                return field;
            } catch (NoSuchFieldException ignored) {
            }
            try {
                Field field = current.getDeclaredField(intermediary);
                field.setAccessible(true);
                return field;
            } catch (NoSuchFieldException ignored) {
            }
            current = current.getSuperclass();
        }
        return null;
    }

    private static void appendJsonValue(StringBuilder sb, String key, String value) {
        if (value.equals("true") || value.equals("false")) {
            sb.append("\"").append(key).append("\":").append(value);
            return;
        }
        if (value.matches("-?\\d+(\\.\\d+)?")) {
            sb.append("\"").append(key).append("\":").append(value);
            return;
        }
        sb.append("\"").append(key).append("\":\"").append(escapeJson(value)).append("\"");
    }

    private static String escapeJson(String value) {
        return value.replace("\\", "\\\\").replace("\"", "\\\"");
    }
}
