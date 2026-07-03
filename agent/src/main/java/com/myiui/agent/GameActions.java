package com.myiui.agent;

import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

public final class GameActions {
    private static volatile Object minecraftClient;
    private static volatile ClassLoader gameLoader;
    private static JvmtiInstrumentation instrumentation;

    private GameActions() {}

    public static void init(JvmtiInstrumentation inst) {
        refreshGameLoader(inst);
    }

    public static void refreshGameLoader(JvmtiInstrumentation inst) {
        if (inst == null) return;
        instrumentation = inst;
        ClassLoader found = AgentExposure.resolveGameLoader(inst);
        if (found == null) return;
        if (found != gameLoader) {
            gameLoader = found;
            minecraftClient = null;
            AgentLog.info("Game ClassLoader refreshed: " + found.getClass().getName());
        }
    }

    public static void ensureReady() {
        JvmtiInstrumentation inst = instrumentation != null ? instrumentation : UiAgentHolder.getJvmtiInstrumentation();
        refreshGameLoader(inst);
        ensureGameLoader();
    }

    private static void ensureGameLoader() {
        if (gameLoader != null) return;
        JvmtiInstrumentation inst = instrumentation != null ? instrumentation : UiAgentHolder.getJvmtiInstrumentation();
        if (inst != null) {
            refreshGameLoader(inst);
        }
    }

    public static boolean openSingleplayer() {
        return runOnClient(client -> {
            Class<?> parentClass = findClass("net.minecraft.client.gui.screen.Screen", "net.minecraft.class_437");
            Object parent = requireCurrentScreen(client);
            openSelectWorldScreen(client, parent, parentClass);
        });
    }

    public static boolean openMultiplayer() {
        return runOnClient(client -> {
            Class<?> parentClass = findClass("net.minecraft.client.gui.screen.Screen", "net.minecraft.class_437");
            Object parent = requireCurrentScreen(client);
            openMultiplayerScreen(client, parent, parentClass);
        });
    }

    public static boolean openOptions() {
        return runOnClient(client -> {
            Class<?> parentClass = findClass("net.minecraft.client.gui.screen.Screen", "net.minecraft.class_437");
            Class<?> optionsClass = findClass("net.minecraft.client.option.GameOptions", "net.minecraft.class_315");
            Class<?> screenClass = findClass(
                    "net.minecraft.client.gui.screen.option.OptionsScreen", "net.minecraft.class_429");
            Object parent = requireCurrentScreen(client);
            Object options = getGameOptionsInternal(client);
            Constructor<?> ctor = screenClass.getConstructor(parentClass, optionsClass);
            invokeSetScreen(client, ctor.newInstance(parent, options));
        });
    }

    public static boolean openVideoOptions() {
        return runOnClient(client -> {
            Class<?> parentClass = findClass("net.minecraft.client.gui.screen.Screen", "net.minecraft.class_437");
            Object current = ReflectUtil.getCurrentScreen(client);
            if (current != null && current.getClass().getName().contains("SodiumOptionsGUI")) {
                AgentLog.info("OPEN_VIDEO_OPTIONS: already on Sodium screen");
                return;
            }

            Object returnScreen = resolveReturnToMenuScreen(client);
            VideoBackground.suspendForGameScreen();

            Object screen = tryCreateSodiumOptionsScreen(returnScreen, parentClass);
            if (screen != null) {
                patchSodiumPrevScreen(screen, returnScreen, parentClass);
                Object patchedPrev = readSodiumPrevScreen(screen);
                AgentLog.info("Sodium prevScreen after patch: "
                        + (patchedPrev == null ? "null" : patchedPrev.getClass().getName()));
                invokeSetScreen(client, screen);
                AgentLog.info("OPEN_VIDEO_OPTIONS: Sodium via createScreen");
                return;
            }

            if (tryOpenSodiumViaOptionsButton(client, returnScreen, parentClass)) {
                AgentLog.info("OPEN_VIDEO_OPTIONS: Sodium via OptionsScreen");
                return;
            }

            invokeSetScreen(client, createVanillaVideoOptionsScreen(client, returnScreen, parentClass));
            AgentLog.info("OPEN_VIDEO_OPTIONS: vanilla");
        });
    }

    private static Object createOptionsScreenInstance(Object parent, Class<?> parentClass, Object client)
            throws ReflectiveOperationException {
        Class<?> optionsClass = findClass("net.minecraft.client.option.GameOptions", "net.minecraft.class_315");
        Class<?> optionsScreenClass = findClass(
                "net.minecraft.client.gui.screen.option.OptionsScreen", "net.minecraft.class_429");
        Object options = getGameOptionsInternal(client);
        return optionsScreenClass.getConstructor(parentClass, optionsClass).newInstance(parent, options);
    }

    private static boolean tryOpenSodiumViaOptionsButton(Object client, Object returnScreen, Class<?> parentClass) {
        try {
            Object optionsScreen = createOptionsScreenInstance(returnScreen, parentClass, client);
            invokeSetScreen(client, optionsScreen);

            Class<?> optionsScreenClass = optionsScreen.getClass();
            for (Method method : optionsScreenClass.getDeclaredMethods()) {
                if (!"method_19828".equals(method.getName())) {
                    continue;
                }
                method.setAccessible(true);
                Object[] args = buildButtonCallbackArgs(method.getParameterTypes(), optionsScreen, client, parentClass);
                if (args == null) {
                    continue;
                }
                method.invoke(optionsScreen, args);
                Object after = ReflectUtil.getCurrentScreen(client);
                if (after != null && isSodiumOrVideoScreen(after)) {
                    patchSodiumPrevScreen(after, returnScreen, parentClass);
                    return true;
                }
            }
        } catch (Throwable t) {
            AgentLog.info("Sodium via OptionsScreen failed: " + t.getMessage());
        }
        return false;
    }

    private static boolean isSodiumOrVideoScreen(Object screen) {
        String name = screen.getClass().getName();
        return name.contains("SodiumOptionsGUI") || name.contains("VideoOptionsScreen") || name.contains("class_446");
    }

    private static Object[] buildButtonCallbackArgs(Class<?>[] params, Object optionsScreen, Object client,
            Class<?> parentClass) {
        Object[] args = new Object[params.length];
        for (int i = 0; i < params.length; i++) {
            Class<?> param = params[i];
            if (param == boolean.class || param == Boolean.class) {
                args[i] = false;
            } else if (param == int.class || param == Integer.class) {
                args[i] = 0;
            } else if (param == double.class || param == Double.class) {
                args[i] = 0.0;
            } else if (param == float.class || param == Float.class) {
                args[i] = 0.f;
            } else if (param.isAssignableFrom(optionsScreen.getClass())) {
                args[i] = optionsScreen;
            } else if (param.getName().contains("Button") || param.getName().contains("class_4185")) {
                args[i] = null;
            } else if (parentClass.isAssignableFrom(param) || isScreenType(param)) {
                args[i] = optionsScreen;
            } else if (param.getName().contains("MinecraftClient") || param.getName().contains("class_310")) {
                args[i] = client;
            } else {
                return null;
            }
        }
        return args;
    }

    public static boolean quit() {
        return runOnClient(client -> {
            ReflectiveOperationException last = null;
            for (String[] methods : new String[][]{
                    {"scheduleStop", "method_1592"},
                    {"stop", "method_1490"},
                    {"close", "method_1493"},
            }) {
                try {
                    invokeNoArg(client, methods[0], methods[1]);
                    AgentLog.info("退出游戏: " + methods[0]);
                    return;
                } catch (ReflectiveOperationException e) {
                    last = e;
                }
            }
            if (last != null) {
                throw last;
            }
        });
    }

    public static Object resolveClientForBridge() {
        try {
            return resolveClient();
        } catch (Throwable e) {
            return null;
        }
    }

    public static Object getGameOptions(Object client) throws ReflectiveOperationException {
        return getGameOptionsInternal(client);
    }

    public static boolean applyOnGameOptions(java.util.function.Function<Object, Boolean> action) {
        return writeOnGameOptions(action).success();
    }

    public static final class OptionsWriteResult {
        private final boolean scheduledOnMainThread;
        private final boolean valueWritten;

        public OptionsWriteResult(boolean scheduledOnMainThread, boolean valueWritten) {
            this.scheduledOnMainThread = scheduledOnMainThread;
            this.valueWritten = valueWritten;
        }

        public boolean scheduledOnMainThread() {
            return scheduledOnMainThread;
        }

        public boolean valueWritten() {
            return valueWritten;
        }

        public boolean success() {
            return scheduledOnMainThread && valueWritten;
        }
    }

    public static OptionsWriteResult writeOnGameOptions(java.util.function.Function<Object, Boolean> action) {
        AtomicBoolean written = new AtomicBoolean(false);
        boolean ranOnMainThread = runOnClient(client -> {
            Object options = getGameOptionsInternal(client);
            if (Boolean.TRUE.equals(action.apply(options))) {
                writeOptions(options);
                written.set(true);
            }
        }, true);
        return new OptionsWriteResult(ranOnMainThread, written.get());
    }

    public static <T> T readOnGameOptions(java.util.function.Function<Object, T> action, T fallback) {
        java.util.concurrent.atomic.AtomicReference<T> result =
                new java.util.concurrent.atomic.AtomicReference<>(fallback);
        runOnClient(client -> {
            try {
                Object options = getGameOptionsInternal(client);
                T value = action.apply(options);
                if (value != null) {
                    result.set(value);
                }
            } catch (Throwable e) {
                AgentLog.error("readOnGameOptions failed", e);
            }
        }, true);
        return result.get();
    }

    public static void writeOptions(Object options) {
        try {
            Method write = ReflectUtil.findInstanceMethod(options.getClass(), "write", "method_1640");
            write.invoke(options);
        } catch (Throwable e) {
            AgentLog.error("writeOptions failed", e);
        }
    }

    public static boolean joinWorld(String worldName) {
        return runOnClient(client -> {
            Object loader = getIntegratedServerLoader(client);
            if (loader == null) {
                throw new IllegalStateException("IntegratedServerLoader unavailable");
            }
            Method start = findIntegratedStartMethod(loader.getClass());
            start.invoke(loader, worldName, (Runnable) () -> AgentLog.info("JOIN_WORLD cancelled: " + worldName));
            AgentLog.info("JOIN_WORLD started: " + worldName);
        });
    }

    public static boolean openCreateWorld() {
        return runOnClient(client -> {
            Object parent = requireCurrentScreen(client);
            Class<?> parentClass = findClass("net.minecraft.client.gui.screen.Screen", "net.minecraft.class_437");
            Object screen = tryOpenScreen(client, parent, parentClass, new String[]{
                    "net.minecraft.client.gui.screen.world.CreateWorldScreen",
                    "net.minecraft.class_525",
            });
            if (screen == null) {
                openSelectWorldScreen(client, parent, parentClass);
            } else {
                invokeSetScreen(client, screen);
            }
        });
    }

    public static boolean connectServer(String id) {
        return runOnClient(client -> {
            ServerBridge.ServerEntry entry = ServerBridge.findServer(id);
            if (entry == null) {
                throw new IllegalStateException("Server not found: " + id);
            }
            Object parent = requireCurrentScreen(client);
            Class<?> parentClass = findClass("net.minecraft.client.gui.screen.Screen", "net.minecraft.class_437");
            Class<?> addressClass = findClass("net.minecraft.client.network.ServerAddress", "net.minecraft.class_639");
            Class<?> infoClass = findClass("net.minecraft.client.network.ServerInfo", "net.minecraft.class_642");
            Class<?> connectClass = findClass("net.minecraft.client.gui.screen.multiplayer.ConnectScreen",
                    "net.minecraft.class_412");
            Class<?> mpClass = findClass("net.minecraft.client.gui.screen.multiplayer.MultiplayerScreen",
                    "net.minecraft.class_500");

            Object address = ReflectUtil.findStaticMethod(addressClass, "parse", "method_2950", String.class)
                    .invoke(null, entry.address);

            Object info = createServerInfo(infoClass, entry.name, entry.address);
            Class<?> cookieClass = findClass("net.minecraft.client.network.CookieStorage", "net.minecraft.class_9112");
            Class<?> screenClass = findClass("net.minecraft.client.gui.screen.Screen", "net.minecraft.class_437");
            Class<?> clientClass = findClass("net.minecraft.client.MinecraftClient", "net.minecraft.class_310");

            Object cookieStorage = resolveCookieStorage(client);
            if (cookieStorage == null) {
                cookieStorage = createEmptyCookieStorage(cookieClass);
            }

            Object connectParent = parent;
            if (!mpClass.isInstance(parent)) {
                java.lang.reflect.Constructor<?> mpCtor = mpClass.getConstructor(parentClass);
                connectParent = mpCtor.newInstance(parent);
            }

            boolean connected = false;
            try {
                Method connect = ReflectUtil.findStaticMethod(connectClass, "connect", "method_36877", screenClass,
                        clientClass, addressClass, infoClass, boolean.class, cookieClass);
                connect.invoke(null, connectParent, client, address, info, false, cookieStorage);
                connected = true;
            } catch (ReflectiveOperationException ignored) {
            }
            if (!connected) {
                Method connect = ReflectUtil.findStaticMethod(connectClass, "connect", "method_2130", clientClass,
                        addressClass, infoClass, cookieClass);
                connect.invoke(null, client, address, info, cookieStorage);
            }
            AgentLog.info("CONNECT_SERVER: " + entry.name + " -> " + entry.address);
        });
    }

    public static boolean openAddServer() {
        return runOnClient(client -> {
            Object parent = requireCurrentScreen(client);
            Class<?> parentClass = findClass("net.minecraft.client.gui.screen.Screen", "net.minecraft.class_437");
            Object screen = tryOpenScreen(client, parent, parentClass, new String[]{
                    "net.minecraft.client.gui.screen.multiplayer.AddServerScreen",
                    "net.minecraft.class_422",
            });
            if (screen == null) {
                openMultiplayerScreen(client, parent, parentClass);
            } else {
                invokeSetScreen(client, screen);
            }
        });
    }

    public static boolean submitCreateWorld(String name, String mode, String seed) {
        return runOnClient(client -> {
            Object parent = requireCurrentScreen(client);
            Class<?> createWorldScreenClass = findClass(
                    "net.minecraft.client.gui.screen.world.CreateWorldScreen", "net.minecraft.class_525");
            invokeCreateWorldShow(client, parent, createWorldScreenClass);
            scheduleDirectWorldCreation(client, createWorldScreenClass, name, mode, seed, 0);
            AgentLog.info("CREATE_WORLD_SUBMIT name=" + name + " mode=" + mode + " seed=" + seed);
        });
    }

    private static void invokeCreateWorldShow(Object client, Object parent, Class<?> createWorldScreenClass)
            throws ReflectiveOperationException {
        Class<?> clientClass = findClass("net.minecraft.client.MinecraftClient", "net.minecraft.class_310");
        Class<?> screenClass = findClass("net.minecraft.client.gui.screen.Screen", "net.minecraft.class_437");
        Method show = null;
        for (Method method : createWorldScreenClass.getDeclaredMethods()) {
            if (!Modifier.isStatic(method.getModifiers())) {
                continue;
            }
            if (!"show".equals(method.getName()) && !"method_31130".equals(method.getName())) {
                continue;
            }
            Class<?>[] params = method.getParameterTypes();
            if (params.length == 2 && params[0].isAssignableFrom(clientClass)
                    && params[1].isAssignableFrom(screenClass)) {
                show = method;
                break;
            }
        }
        if (show == null) {
            throw new NoSuchMethodException("CreateWorldScreen.show(MinecraftClient, Screen)");
        }
        show.setAccessible(true);
        show.invoke(null, client, parent);
    }

    private static void scheduleDirectWorldCreation(Object client, Class<?> createWorldScreenClass, String name,
            String mode, String seed, int attempt) {
        try {
            Object screen = ReflectUtil.getCurrentScreen(client);
            if (createWorldScreenClass.isInstance(screen)) {
                configureWorldCreatorAndCreate(screen, createWorldScreenClass, name, mode, seed);
                AgentLog.info("CREATE_WORLD_STARTED name=" + name);
                return;
            }
            if (attempt >= 1200) {
                throw new IllegalStateException("CreateWorldScreen not ready, current="
                        + (screen == null ? "null" : screen.getClass().getName()));
            }
            Method execute = ReflectUtil.findInstanceMethod(client.getClass(), "execute", "method_1514", Runnable.class);
            final int nextAttempt = attempt + 1;
            execute.invoke(client, (Runnable) () -> scheduleDirectWorldCreation(client, createWorldScreenClass, name,
                    mode, seed, nextAttempt));
        } catch (Throwable e) {
            AgentLog.error("CREATE_WORLD schedule failed", e);
        }
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    private static void configureWorldCreatorAndCreate(Object screen, Class<?> createWorldScreenClass, String name,
            String mode, String seed) throws ReflectiveOperationException {
        Class<?> creatorClass = findClass("net.minecraft.client.gui.screen.world.WorldCreator",
                "net.minecraft.class_8100");
        Class<?> modeClass = findClass("net.minecraft.client.gui.screen.world.WorldCreator$Mode",
                "net.minecraft.class_8100$class_4539");

        Object creator = ReflectUtil.findInstanceMethod(createWorldScreenClass, "getWorldCreator", "method_48657")
                .invoke(screen);
        ReflectUtil.findInstanceMethod(creatorClass, "setWorldName", "method_48710", String.class).invoke(creator, name);

        String modeName = mode != null && mode.equalsIgnoreCase("hardcore") ? "HARDCORE" : mapWorldCreatorMode(mode);
        Object gameMode = Enum.valueOf((Class<Enum>) modeClass, modeName);
        ReflectUtil.findInstanceMethod(creatorClass, "setGameMode", "method_48704", modeClass).invoke(creator, gameMode);

        if (seed != null && !seed.isEmpty()) {
            ReflectUtil.findInstanceMethod(creatorClass, "setSeed", "method_48716", String.class).invoke(creator, seed);
        }

        ReflectUtil.findInstanceMethod(createWorldScreenClass, "createLevel", "method_2736").invoke(screen);
    }

    private static Object buildLevelInfo(String name, String mode) throws ReflectiveOperationException {
        Class<?> levelInfoClass = findClass("net.minecraft.world.level.LevelInfo", "net.minecraft.class_1940");
        Class<?> gameModeClass = findClass("net.minecraft.world.GameMode", "net.minecraft.class_1934");
        Class<?> difficultyClass = findClass("net.minecraft.world.Difficulty", "net.minecraft.class_1267");
        Class<?> gameRulesClass = findClass("net.minecraft.world.GameRules", "net.minecraft.class_1928");
        Class<?> dataConfigClass = findClass("net.minecraft.resource.DataConfiguration", "net.minecraft.class_7712");

        boolean hardcore = mode != null && mode.equalsIgnoreCase("hardcore");
        String gameModeName = hardcore ? "SURVIVAL" : mapWorldCreatorMode(mode);
        Object gameMode = Enum.valueOf((Class<Enum>) gameModeClass, gameModeName);
        Object difficulty = Enum.valueOf((Class<Enum>) difficultyClass, hardcore ? "HARD" : "NORMAL");
        Object gameRules = createDefaultGameRules(gameRulesClass);
        Object dataConfig = resolveDefaultDataConfiguration(dataConfigClass, null);
        boolean allowCommands = hardcore || "CREATIVE".equals(gameModeName);

        Constructor<?> ctor = levelInfoClass.getConstructor(String.class, gameModeClass, boolean.class,
                difficultyClass, boolean.class, gameRulesClass, dataConfigClass);
        return ctor.newInstance(name, gameMode, hardcore, difficulty, allowCommands, gameRules, dataConfig);
    }

    private static Object resolveDefaultDataConfiguration(Class<?> dataConfigClass, Object client)
            throws ReflectiveOperationException {
        if (client != null) {
            try {
                Object fromClient = resolveDataConfigurationFromClient(client, dataConfigClass);
                if (fromClient != null) {
                    return fromClient;
                }
            } catch (Throwable ignored) {
            }
        }

        for (Field field : dataConfigClass.getFields()) {
            if (!Modifier.isStatic(field.getModifiers())) {
                continue;
            }
            if (!dataConfigClass.isAssignableFrom(field.getType())) {
                continue;
            }
            try {
                Object value = field.get(null);
                if (value != null) {
                    return value;
                }
            } catch (Throwable ignored) {
            }
        }

        for (String[] candidate : new String[][]{
                {"DEFAULT", "field_42998"},
                {"SAFE_MODE", "field_42999"},
                {"DEFAULT", "field_25392"},
                {"SAFE_MODE", "field_25393"},
        }) {
            try {
                return ReflectUtil.getStaticField(dataConfigClass, candidate[0], candidate[1]);
            } catch (ReflectiveOperationException ignored) {
            }
        }

        return constructDataConfiguration(dataConfigClass, client);
    }

    private static Object resolveDataConfigurationFromClient(Object client, Class<?> dataConfigClass)
            throws ReflectiveOperationException {
        Class<?> dataPackSettingsClass = findClass("net.minecraft.resource.DataPackSettings",
                "net.minecraft.class_5359");
        Class<?> featureSetClass = findClass("net.minecraft.resource.featuretoggle.FeatureSet",
                "net.minecraft.class_7699");
        Class<?> featureFlagsClass = findClass("net.minecraft.resource.featuretoggle.FeatureFlags",
                "net.minecraft.class_7701");

        Method getManager = ReflectUtil.findInstanceMethod(client.getClass(), "getResourcePackManager", "method_1520");
        Object manager = getManager.invoke(client);
        if (manager == null) {
            return null;
        }

        ReflectUtil.findInstanceMethod(manager.getClass(), "scanPacks", "method_14445").invoke(manager);
        Object options = getGameOptionsInternal(client);
        if (options != null) {
            try {
                Method addProfiles = ReflectUtil.findInstanceMethod(options.getClass(),
                        "addResourcePackProfilesToManager", "method_1627", manager.getClass());
                addProfiles.invoke(options, manager);
            } catch (ReflectiveOperationException ignored) {
            }
        }

        java.util.List<String> enabled = new java.util.ArrayList<>();
        java.util.List<String> disabled = new java.util.ArrayList<>();
        java.util.Set<String> enabledSet = new java.util.LinkedHashSet<>();

        Method getEnabled = ReflectUtil.findInstanceMethod(manager.getClass(), "getEnabledProfiles", "method_14444");
        Object enabledProfiles = getEnabled.invoke(manager);
        if (enabledProfiles instanceof Iterable<?> iterable) {
            for (Object profile : iterable) {
                String id = readResourcePackProfileId(profile);
                if (id != null && !id.isEmpty()) {
                    enabled.add(id);
                    enabledSet.add(id);
                }
            }
        }

        Method getProfiles = ReflectUtil.findInstanceMethod(manager.getClass(), "getProfiles", "method_14441");
        Object allProfiles = getProfiles.invoke(manager);
        if (allProfiles instanceof Iterable<?> iterable) {
            for (Object profile : iterable) {
                String id = readResourcePackProfileId(profile);
                if (id != null && !id.isEmpty() && !enabledSet.contains(id)) {
                    disabled.add(id);
                }
            }
        }

        if (enabled.isEmpty()) {
            return null;
        }

        Object dataPackSettings = dataPackSettingsClass.getConstructor(List.class, List.class)
                .newInstance(enabled, disabled);
        Object featureSet = resolveDefaultFeatureSet(featureSetClass, featureFlagsClass);
        for (Constructor<?> ctor : dataConfigClass.getConstructors()) {
            Class<?>[] params = ctor.getParameterTypes();
            if (params.length == 2 && params[0].isInstance(dataPackSettings) && params[1].isInstance(featureSet)) {
                return ctor.newInstance(dataPackSettings, featureSet);
            }
        }
        return null;
    }

    private static String readResourcePackProfileId(Object profile) {
        try {
            Method getId = ReflectUtil.findInstanceMethod(profile.getClass(), "getId", "method_14463");
            Object id = getId.invoke(profile);
            return id != null ? String.valueOf(id) : null;
        } catch (ReflectiveOperationException e) {
            return null;
        }
    }

    private static Object constructDataConfiguration(Class<?> dataConfigClass)
            throws ReflectiveOperationException {
        return constructDataConfiguration(dataConfigClass, null);
    }

    private static Object constructDataConfiguration(Class<?> dataConfigClass, Object client)
            throws ReflectiveOperationException {
        Class<?> dataPackSettingsClass = findClass("net.minecraft.resource.DataPackSettings",
                "net.minecraft.class_5359");
        Class<?> featureSetClass = findClass("net.minecraft.resource.featuretoggle.FeatureSet",
                "net.minecraft.class_7699");
        Class<?> featureFlagsClass = findClass("net.minecraft.resource.featuretoggle.FeatureFlags",
                "net.minecraft.class_7701");

        Object dataPackSettings = resolveDefaultDataPackSettings(dataPackSettingsClass, client);
        Object featureSet = resolveDefaultFeatureSet(featureSetClass, featureFlagsClass);

        for (Constructor<?> ctor : dataConfigClass.getConstructors()) {
            Class<?>[] params = ctor.getParameterTypes();
            if (params.length == 2 && params[0].isInstance(dataPackSettings) && params[1].isInstance(featureSet)) {
                return ctor.newInstance(dataPackSettings, featureSet);
            }
        }
        throw new NoSuchMethodException("DataConfiguration(DataPackSettings, FeatureSet)");
    }

    private static Object resolveDefaultDataPackSettings(Class<?> dataPackSettingsClass, Object client)
            throws ReflectiveOperationException {
        if (client != null) {
            try {
                Object fromClient = resolveDataConfigurationFromClient(client,
                        findClass("net.minecraft.resource.DataConfiguration", "net.minecraft.class_7712"));
                if (fromClient != null) {
                    Method dataPacks = ReflectUtil.findInstanceMethod(fromClient.getClass(), "dataPacks", "comp_1010");
                    Object settings = dataPacks.invoke(fromClient);
                    if (settings != null) {
                        return settings;
                    }
                }
            } catch (Throwable ignored) {
            }
        }

        for (String[] candidate : new String[][]{
                {"DEFAULT", "field_25392"},
                {"SAFE_MODE", "field_25393"},
                {"SAFE_MODE", "field_42999"},
        }) {
            try {
                return ReflectUtil.getStaticField(dataPackSettingsClass, candidate[0], candidate[1]);
            } catch (ReflectiveOperationException ignored) {
            }
        }

        for (Field field : dataPackSettingsClass.getFields()) {
            if (!Modifier.isStatic(field.getModifiers())) {
                continue;
            }
            if (!dataPackSettingsClass.isAssignableFrom(field.getType())) {
                continue;
            }
            if ("CODEC".equals(field.getName())) {
                continue;
            }
            try {
                Object value = field.get(null);
                if (value != null) {
                    return value;
                }
            } catch (Throwable ignored) {
            }
        }

        return dataPackSettingsClass.getConstructor(List.class, List.class)
                .newInstance(List.of("vanilla"), List.of());
    }

    private static Object resolveDefaultFeatureSet(Class<?> featureSetClass, Class<?> featureFlagsClass)
            throws ReflectiveOperationException {
        try {
            return ReflectUtil.getStaticField(featureFlagsClass, "DEFAULT_ENABLED_FEATURES", "field_40183");
        } catch (ReflectiveOperationException ignored) {
        }
        for (String[] method : new String[][]{
                {"empty", "method_45397"},
        }) {
            try {
                return ReflectUtil.findStaticMethod(featureSetClass, method[0], method[1]).invoke(null);
            } catch (ReflectiveOperationException ignored) {
            }
        }
        try {
            return ReflectUtil.getStaticField(featureSetClass, "EMPTY", "field_40173");
        } catch (ReflectiveOperationException ignored) {
        }
        throw new NoSuchMethodException("default FeatureSet");
    }

    private static Object buildGeneratorOptions(String seed) throws ReflectiveOperationException {
        Class<?> genOptClass = findClass("net.minecraft.world.gen.GeneratorOptions", "net.minecraft.class_5285");
        Method createRandom = ReflectUtil.findStaticMethod(genOptClass, "createRandom", "method_45541");
        Object options = createRandom.invoke(null);
        if (seed == null || seed.isEmpty()) {
            return options;
        }
        Method parseSeed = ReflectUtil.findStaticMethod(genOptClass, "parseSeed", "method_46720", String.class);
        Object optionalLong = parseSeed.invoke(null, seed);
        Method withSeed = ReflectUtil.findInstanceMethod(genOptClass, "withSeed", "method_28024", optionalLong.getClass());
        return withSeed.invoke(options, optionalLong);
    }

    private static Object resolveDefaultDimensions(Object lookup) throws ReflectiveOperationException {
        Class<?> worldPresetsClass = findClass("net.minecraft.world.gen.WorldPresets", "net.minecraft.class_5317");
        Class<?> registryKeysClass = findClass("net.minecraft.registry.RegistryKeys", "net.minecraft.class_7924");
        Class<?> worldPresetClass = findClass("net.minecraft.world.gen.WorldPreset", "net.minecraft.class_7145");

        Object defaultPresetKey = ReflectUtil.getStaticField(worldPresetsClass, "DEFAULT", "field_25050");
        Object presetRegistryKey = ReflectUtil.getStaticField(registryKeysClass, "WORLD_PRESET", "field_41250");

        Object presetRegistry = invokeLookupRegistry(lookup, presetRegistryKey);
        if (presetRegistry != null) {
            Object presetEntry = invokeRegistryOptional(presetRegistry, defaultPresetKey);
            if (presetEntry != null) {
                Object preset = invokeRegistryEntryValue(presetEntry);
                if (preset != null) {
                    Method createHolder = ReflectUtil.findInstanceMethod(worldPresetClass, "createDimensionsRegistryHolder",
                            "method_45546");
                    return createHolder.invoke(preset);
                }
            }
        }

        Method createDemo = ReflectUtil.findStaticMethod(worldPresetsClass, "createDemoOptions", "method_41598",
                lookup.getClass());
        return createDemo.invoke(null, lookup);
    }

    private static Object invokeLookupRegistry(Object lookup, Object registryKey) {
        for (String[] method : new String[][]{
                {"getOptional", "method_46762"},
                {"getRegistryOrThrow", "method_46771"},
                {"getWrapperOrThrow", "method_46771"},
        }) {
            try {
                Method m = ReflectUtil.findInstanceMethod(lookup.getClass(), method[0], method[1], registryKey.getClass());
                Object result = m.invoke(lookup, registryKey);
                if (result instanceof java.util.Optional<?> optional) {
                    return optional.orElse(null);
                }
                return result;
            } catch (ReflectiveOperationException ignored) {
            }
        }
        return null;
    }

    private static Object invokeRegistryOptional(Object registry, Object key) {
        for (String[] method : new String[][]{
                {"getOptional", "method_40269"},
                {"getEntry", "method_40269"},
        }) {
            try {
                Method m = ReflectUtil.findInstanceMethod(registry.getClass(), method[0], method[1], key.getClass());
                Object result = m.invoke(registry, key);
                if (result instanceof java.util.Optional<?> optional) {
                    return optional.orElse(null);
                }
                return result;
            } catch (ReflectiveOperationException ignored) {
            }
        }
        return null;
    }

    private static Object invokeRegistryEntryValue(Object entry) {
        for (String[] method : new String[][]{
                {"value", "comp_349"},
                {"getValue", "comp_349"},
        }) {
            try {
                Method m = ReflectUtil.findInstanceMethod(entry.getClass(), method[0], method[1]);
                return m.invoke(entry);
            } catch (ReflectiveOperationException ignored) {
            }
        }
        return null;
    }

    private static String mapWorldCreatorMode(String mode) {
        if (mode == null) return "SURVIVAL";
        return switch (mode.toLowerCase()) {
            case "creative" -> "CREATIVE";
            case "hardcore" -> "HARDCORE";
            default -> "SURVIVAL";
        };
    }

    private static Object getIntegratedServerLoader(Object client) throws ReflectiveOperationException {
        for (String[] method : new String[][]{
                {"createIntegratedServerLoader", "method_41735"},
                {"createIntegratedServerLoader", "method_41886"},
                {"method_41735", "createIntegratedServerLoader"},
                {"method_41886", "createIntegratedServerLoader"},
        }) {
            try {
                Method m = ReflectUtil.findInstanceMethod(client.getClass(), method[0], method[1]);
                Object loader = m.invoke(client);
                if (loader != null) {
                    return loader;
                }
            } catch (ReflectiveOperationException ignored) {
            }
        }
        for (Method method : client.getClass().getMethods()) {
            if (method.getParameterCount() != 0) {
                continue;
            }
            String returnName = method.getReturnType().getName();
            if (!returnName.contains("IntegratedServerLoader") && !returnName.contains("class_7196")) {
                continue;
            }
            try {
                Object loader = method.invoke(client);
                if (loader != null) {
                    return loader;
                }
            } catch (ReflectiveOperationException ignored) {
            }
        }
        throw new NoSuchMethodException("createIntegratedServerLoader on MinecraftClient");
    }

    private static Method findIntegratedStartMethod(Class<?> loaderClass) throws ReflectiveOperationException {
        for (String[] method : new String[][]{
                {"start", "method_57784"},
                {"start", "method_57785"},
                {"method_57784", "start"},
                {"method_57785", "start"},
        }) {
            try {
                return ReflectUtil.findInstanceMethod(loaderClass, method[0], method[1], String.class, Runnable.class);
            } catch (ReflectiveOperationException ignored) {
            }
        }
        for (Method method : loaderClass.getMethods()) {
            if (!"start".equals(method.getName()) || method.getParameterCount() != 2) {
                continue;
            }
            Class<?>[] params = method.getParameterTypes();
            if (params[0] == String.class && Runnable.class.isAssignableFrom(params[1])) {
                return method;
            }
        }
        throw new NoSuchMethodException("IntegratedServerLoader.start");
    }

    private static Object createDefaultGameRules(Class<?> gameRulesClass) throws ReflectiveOperationException {
        Class<?> featureSetClass = findClass("net.minecraft.resource.featuretoggle.FeatureSet",
                "net.minecraft.class_7699");
        Object featureSet = ReflectUtil.findStaticMethod(featureSetClass, "empty", "method_45397").invoke(null);
        return gameRulesClass.getConstructor(featureSetClass).newInstance(featureSet);
    }

    private static Object createEmptyCookieStorage(Class<?> cookieClass) throws ReflectiveOperationException {
        return cookieClass.getConstructor(java.util.Map.class).newInstance(java.util.Collections.emptyMap());
    }

    private static Object resolveCookieStorage(Object client) {
        for (String[] candidate : new String[][]{
                {"cookieStorage", "field_52223"},
                {"field_52223", "cookieStorage"},
        }) {
            try {
                return ReflectUtil.getField(client, candidate[0], candidate[1]);
            } catch (ReflectiveOperationException ignored) {
            }
        }
        return null;
    }

    private static Object createServerInfo(Class<?> infoClass, String name, String address)
            throws ReflectiveOperationException {
        ReflectiveOperationException last = null;
        for (java.lang.reflect.Constructor<?> ctor : infoClass.getConstructors()) {
            Class<?>[] params = ctor.getParameterTypes();
            try {
                if (params.length == 2 && params[0] == String.class && params[1] == String.class) {
                    return ctor.newInstance(name, address);
                }
                if (params.length == 3 && params[0] == String.class && params[1] == String.class
                        && params[2].isEnum()) {
                    Object serverType = Enum.valueOf((Class<Enum>) params[2], "OTHER");
                    return ctor.newInstance(name, address, serverType);
                }
                if (params.length == 3 && params[0] == String.class && params[1] == String.class
                        && (params[2] == boolean.class || params[2] == Boolean.class)) {
                    return ctor.newInstance(name, address, false);
                }
            } catch (ReflectiveOperationException e) {
                last = e;
            }
        }

        try {
            Class<?> serverTypeClass = findClass("net.minecraft.client.network.ServerInfo$ServerType",
                    "net.minecraft.class_642$class_8678");
            Object otherType = Enum.valueOf((Class<Enum>) serverTypeClass, "OTHER");
            Constructor<?> ctor = infoClass.getConstructor(String.class, String.class, serverTypeClass);
            return ctor.newInstance(name, address, otherType);
        } catch (ReflectiveOperationException e) {
            last = e;
        }

        if (last != null) {
            throw last;
        }
        throw new NoSuchMethodException("ServerInfo constructor not found");
    }

    private static void setServerInfoField(Object info, String named, String intermediary, String value) {
        try {
            java.lang.reflect.Field field = info.getClass().getDeclaredField(named);
            field.setAccessible(true);
            field.set(info, value);
        } catch (ReflectiveOperationException ignored) {
            try {
                java.lang.reflect.Field field = info.getClass().getDeclaredField(intermediary);
                field.setAccessible(true);
                field.set(info, value);
            } catch (ReflectiveOperationException ignored2) {
            }
        }
    }

    private static void openSelectWorldScreen(Object client, Object parent, Class<?> parentClass)
            throws ReflectiveOperationException {
        Class<?> screenClass = findClass(
                "net.minecraft.client.gui.screen.world.SelectWorldScreen", "net.minecraft.class_526");
        Constructor<?> ctor = screenClass.getConstructor(parentClass);
        invokeSetScreen(client, ctor.newInstance(parent));
    }

    private static void openMultiplayerScreen(Object client, Object parent, Class<?> parentClass)
            throws ReflectiveOperationException {
        Class<?> screenClass = findClass(
                "net.minecraft.client.gui.screen.multiplayer.MultiplayerScreen", "net.minecraft.class_500");
        Constructor<?> ctor = screenClass.getConstructor(parentClass);
        invokeSetScreen(client, ctor.newInstance(parent));
    }

    static void ensureTitleScreenAndActivateMenu(Object client) {
        try {
            Object current = ReflectUtil.getCurrentScreen(client);
            if (ClassUtil.isTitleScreenInstance(current)) {
                SharedState.onTitleScreenOpened(current);
                return;
            }
            Object title = resolveReturnToMenuScreen(client);
            invokeSetScreen(client, title);
            SharedState.onTitleScreenOpened(title);
            AgentLog.info("Forced return to TitleScreen");
        } catch (Throwable t) {
            AgentLog.error("ensureTitleScreenAndActivateMenu failed", t);
        }
    }

    private static Object resolveReturnToMenuScreen(Object client) throws ReflectiveOperationException {
        Object screen = ReflectUtil.getCurrentScreen(client);
        if (ClassUtil.isTitleScreenInstance(screen)) {
            return screen;
        }
        if (screen != null && screen.getClass().getName().contains("SodiumOptionsGUI")) {
            Object prev = readSodiumPrevScreen(screen);
            if (prev != null) {
                return prev;
            }
        }
        if (screen == null) {
            Class<?> titleClass = findClass("net.minecraft.client.gui.screen.TitleScreen", "net.minecraft.class_442");
            java.lang.reflect.Constructor<?> ctor = titleClass.getDeclaredConstructor();
            ctor.setAccessible(true);
            Object title = ctor.newInstance();
            AgentLog.info("Created TitleScreen return target (currentScreen was null)");
            return title;
        }
        Class<?> titleClass = findClass("net.minecraft.client.gui.screen.TitleScreen", "net.minecraft.class_442");
        java.lang.reflect.Constructor<?> ctor = titleClass.getDeclaredConstructor();
        ctor.setAccessible(true);
        Object title = ctor.newInstance();
        AgentLog.info("Created TitleScreen return target (from " + screen.getClass().getName() + ")");
        return title;
    }

    private static Object readSodiumPrevScreen(Object sodiumScreen) {
        for (String fieldName : new String[]{"prevScreen", "parent", "lastScreen"}) {
            try {
                java.lang.reflect.Field field = sodiumScreen.getClass().getDeclaredField(fieldName);
                field.setAccessible(true);
                return field.get(sodiumScreen);
            } catch (ReflectiveOperationException ignored) {
            }
        }
        Class<?> clazz = sodiumScreen.getClass();
        while (clazz != null && clazz != Object.class) {
            for (java.lang.reflect.Field field : clazz.getDeclaredFields()) {
                if (!isScreenType(field.getType())) {
                    continue;
                }
                try {
                    field.setAccessible(true);
                    return field.get(sodiumScreen);
                } catch (ReflectiveOperationException ignored) {
                }
            }
            clazz = clazz.getSuperclass();
        }
        return null;
    }

    private static void patchSodiumPrevScreen(Object sodiumScreen, Object returnScreen, Class<?> parentClass) {
        for (String fieldName : new String[]{"prevScreen", "parent", "lastScreen"}) {
            try {
                java.lang.reflect.Field field = sodiumScreen.getClass().getDeclaredField(fieldName);
                if (!parentClass.isAssignableFrom(field.getType()) && !isScreenType(field.getType())) {
                    continue;
                }
                field.setAccessible(true);
                field.set(sodiumScreen, returnScreen);
                AgentLog.info("Sodium prevScreen patched (" + fieldName + ")");
                return;
            } catch (ReflectiveOperationException ignored) {
            }
        }

        Class<?> clazz = sodiumScreen.getClass();
        while (clazz != null && clazz != Object.class) {
            for (java.lang.reflect.Field field : clazz.getDeclaredFields()) {
                if (!parentClass.isAssignableFrom(field.getType()) && !isScreenType(field.getType())) {
                    continue;
                }
                try {
                    field.setAccessible(true);
                    field.set(sodiumScreen, returnScreen);
                    AgentLog.info("Sodium prevScreen patched (" + field.getName() + ")");
                    return;
                } catch (ReflectiveOperationException ignored) {
                }
            }
            clazz = clazz.getSuperclass();
        }
    }

    private static Object tryCreateSodiumOptionsScreen(Object parent, Class<?> parentClass) {
        for (String className : new String[]{
                "net.caffeinemc.mods.sodium.client.gui.SodiumOptionsGUI",
                "me.jellysquid.mods.sodium.client.gui.SodiumOptionsGUI",
        }) {
            try {
                Class<?> guiClass = findClass(className, className);
                try {
                    Method createScreen = ReflectUtil.findStaticMethod(guiClass, "createScreen", "createScreen",
                            parentClass);
                    Object screen = createScreen.invoke(null, parent);
                    if (screen != null) {
                        return screen;
                    }
                } catch (ReflectiveOperationException ignored) {
                }

                for (Method method : guiClass.getDeclaredMethods()) {
                    if (!Modifier.isStatic(method.getModifiers())) {
                        continue;
                    }
                    if (!"createScreen".equals(method.getName()) || method.getParameterCount() != 1) {
                        continue;
                    }
                    if (!method.getParameterTypes()[0].isAssignableFrom(parentClass)) {
                        continue;
                    }
                    method.setAccessible(true);
                    Object screen = method.invoke(null, parent);
                    if (screen != null) {
                        return screen;
                    }
                }

                for (Constructor<?> ctor : guiClass.getDeclaredConstructors()) {
                    if (ctor.getParameterCount() != 1) {
                        continue;
                    }
                    if (!ctor.getParameterTypes()[0].isAssignableFrom(parentClass)) {
                        continue;
                    }
                    ctor.setAccessible(true);
                    Object screen = ctor.newInstance(parent);
                    if (screen != null) {
                        return screen;
                    }
                }
            } catch (Throwable t) {
                AgentLog.info("Sodium GUI unavailable (" + className + "): " + t.getMessage());
            }
        }
        return null;
    }

    private static Object createVanillaVideoOptionsScreen(Object client, Object parent, Class<?> parentClass)
            throws ReflectiveOperationException {
        Class<?> clientClass = findClass("net.minecraft.client.MinecraftClient", "net.minecraft.class_310");
        Class<?> optionsClass = findClass("net.minecraft.client.option.GameOptions", "net.minecraft.class_315");
        Class<?> videoClass = findClass(
                "net.minecraft.client.gui.screen.option.VideoOptionsScreen", "net.minecraft.class_446");
        Object options = getGameOptionsInternal(client);
        Constructor<?> ctor = videoClass.getConstructor(parentClass, clientClass, optionsClass);
        return ctor.newInstance(parent, client, options);
    }

    private static Object tryOpenScreen(Object client, Object parent, Class<?> parentClass, String[] classNames) {
        for (String className : classNames) {
            try {
                Class<?> screenClass = findClass(className, className);
                for (Constructor<?> ctor : screenClass.getConstructors()) {
                    Object instance = tryConstruct(ctor, parent, parentClass, client);
                    if (instance != null) {
                        return instance;
                    }
                }
            } catch (Throwable ignored) {
            }
        }
        return null;
    }

    private static Object tryConstruct(Constructor<?> ctor, Object parent, Class<?> parentClass, Object client) {
        Class<?>[] params = ctor.getParameterTypes();
        Object[] args = new Object[params.length];
        for (int i = 0; i < params.length; i++) {
            Class<?> param = params[i];
            if (param.isAssignableFrom(parentClass) || isScreenType(param)) {
                args[i] = parent;
            } else if (param == String.class) {
                args[i] = "New World";
            } else if (param.getName().contains("WorldCreator")) {
                Object creator = newWorldCreator(param);
                if (creator == null) return null;
                args[i] = creator;
            } else if (param.getName().contains("ServerInfo")) {
                Object info = newInstanceNoArg(param);
                if (info == null) return null;
                args[i] = info;
            } else if (param == boolean.class || param == Boolean.class) {
                args[i] = false;
            } else if (param.isInterface() && param.getName().contains("Consumer")) {
                args[i] = (java.util.function.Consumer<Object>) (ignored) -> {};
            } else if (isScreenType(param)) {
                args[i] = parent;
            } else {
                return null;
            }
        }
        try {
            return ctor.newInstance(args);
        } catch (Throwable ignored) {
            return null;
        }
    }

    private static boolean isScreenType(Class<?> param) {
        String name = param.getName();
        return name.contains("Screen") || name.contains("class_437");
    }

    private static Object newWorldCreator(Class<?> creatorClass) {
        try {
            return creatorClass.getConstructor(String.class).newInstance("New World");
        } catch (Throwable ignored) {
        }
        return newInstanceNoArg(creatorClass);
    }

    private static Object newInstanceNoArg(Class<?> type) {
        try {
            return type.getConstructor().newInstance();
        } catch (Throwable ignored) {
            return null;
        }
    }

    private static Object requireCurrentScreen(Object client) throws ReflectiveOperationException {
        Object screen = ReflectUtil.getCurrentScreen(client);
        if (screen == null) {
            throw new IllegalStateException("currentScreen is null");
        }
        return screen;
    }

    private static Object getGameOptionsInternal(Object client) throws ReflectiveOperationException {
        for (String[] candidate : new String[][]{
                {"options", "field_1690"},
                {"field_1690", "options"},
                {"gameOptions", "field_1690"},
        }) {
            try {
                return ReflectUtil.getField(client, candidate[0], candidate[1]);
            } catch (ReflectiveOperationException ignored) {
            }
        }
        throw new NoSuchFieldException("GameOptions on MinecraftClient");
    }

    public static boolean runOnMainThread(ClientBooleanAction action) {
        return runOnClient(client -> {
            if (!action.run(client)) {
                throw new IllegalStateException("main thread action returned false");
            }
        });
    }

    private static boolean runOnClient(ClientAction action) {
        return runOnClient(action, false);
    }

    private static boolean runOnClient(ClientAction action, boolean trackMainThreadExecution) {
        try {
            Object client = resolveClient();
            if (client == null) {
                AgentLog.error("MinecraftClient not found.");
                return false;
            }
            AtomicBoolean ok = new AtomicBoolean(false);
            AtomicBoolean ranOnMain = new AtomicBoolean(false);
            CountDownLatch latch = new CountDownLatch(1);
            Method execute = ReflectUtil.findInstanceMethod(client.getClass(), "execute", "method_1514", Runnable.class);
            execute.invoke(client, (Runnable) () -> {
                ranOnMain.set(true);
                try {
                    action.run(client);
                    ok.set(true);
                    AgentLog.info("游戏操作已完成");
                } catch (Throwable e) {
                    AgentLog.error("游戏操作失败", e);
                } finally {
                    latch.countDown();
                }
            });
            if (!latch.await(10, TimeUnit.SECONDS)) {
                AgentLog.error("runOnClient 等待主线程超时");
                return trackMainThreadExecution ? ranOnMain.get() : false;
            }
            return trackMainThreadExecution ? ranOnMain.get() : ok.get();
        } catch (Throwable e) {
            AgentLog.error("runOnClient 执行失败", e);
            return false;
        }
    }

    private static Object resolveClient() throws ReflectiveOperationException {
        ensureGameLoader();
        if (minecraftClient != null) return minecraftClient;
        JvmtiInstrumentation inst = instrumentation != null ? instrumentation : UiAgentHolder.getJvmtiInstrumentation();
        if (inst != null) {
            for (Class<?> c : inst.getAllLoadedClasses()) {
                String name = c.getName();
                if (!"net.minecraft.client.MinecraftClient".equals(name) && !"net.minecraft.class_310".equals(name)) {
                    continue;
                }
                if (gameLoader == null) {
                    gameLoader = c.getClassLoader();
                }
                for (String[] getter : new String[][]{{"getInstance", "method_1551"}}) {
                    try {
                        Method getInstance = ReflectUtil.findStaticMethod(c, getter[0], getter[1]);
                        minecraftClient = getInstance.invoke(null);
                        if (minecraftClient != null) return minecraftClient;
                    } catch (ReflectiveOperationException ignored) {
                    }
                }
            }
        }

        Class<?> mc = findClass("net.minecraft.client.MinecraftClient", "net.minecraft.class_310");
        Method getInstance = ReflectUtil.findStaticMethod(mc, "getInstance", "method_1551");
        minecraftClient = getInstance.invoke(null);
        return minecraftClient;
    }

    public static java.nio.file.Path getSavesDirectory() {
        try {
            Object client = resolveClient();
            Object runDir = ReflectUtil.getRunDirectory(client);
            if (runDir instanceof java.io.File file) {
                return file.toPath().resolve("saves");
            }
            if (runDir instanceof java.nio.file.Path path) {
                return path.resolve("saves");
            }
        } catch (Throwable e) {
            AgentLog.error("getSavesDirectory failed", e);
        }
        String appData = System.getenv("APPDATA");
        if (appData == null) return null;
        return java.nio.file.Paths.get(appData, ".minecraft", "saves");
    }

    private static void invokeSetScreen(Object client, Object screen) throws ReflectiveOperationException {
        Class<?> screenClass = findClass("net.minecraft.client.gui.screen.Screen", "net.minecraft.class_437");
        Method setScreen = ReflectUtil.findInstanceMethod(client.getClass(), "setScreen", "method_1507", screenClass);
        setScreen.invoke(client, screen);
    }

    private static void invokeNoArg(Object target, String named, String intermediary) throws ReflectiveOperationException {
        Method m = ReflectUtil.findInstanceMethod(target.getClass(), named, intermediary);
        m.invoke(target);
    }

    public static Class<?> findClassForBridge(String named, String intermediary) {
        try {
            return findClass(named, intermediary);
        } catch (ClassNotFoundException e) {
            return null;
        }
    }

    private static Class<?> findClass(String named, String intermediary) throws ClassNotFoundException {
        ensureGameLoader();
        ClassLoader loader = gameLoader != null ? gameLoader : Thread.currentThread().getContextClassLoader();
        try {
            return Class.forName(named, true, loader);
        } catch (ClassNotFoundException e) {
            try {
                return Class.forName(intermediary, true, loader);
            } catch (ClassNotFoundException e2) {
                JvmtiInstrumentation inst = instrumentation != null ? instrumentation : UiAgentHolder.getJvmtiInstrumentation();
                if (inst != null) {
                    for (Class<?> c : inst.getAllLoadedClasses()) {
                        if (named.equals(c.getName()) || intermediary.equals(c.getName())) {
                            return c;
                        }
                    }
                }
                throw e2;
            }
        }
    }

    @FunctionalInterface
    private interface ClientAction {
        void run(Object client) throws Exception;
    }

    @FunctionalInterface
    public interface ClientBooleanAction {
        boolean run(Object client) throws Exception;
    }
}
