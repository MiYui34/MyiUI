package com.myiui.agent;

import java.io.File;
import java.lang.reflect.Constructor;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.Collections;

/**
 * Version-agnostic multiplayer join. Tries Mojang QuickPlay first, then MultiplayerScreen.connect,
 * then static ConnectScreen.connect discovered by parameter types.
 */
public final class VanillaServerConnect {
    private VanillaServerConnect() {}

    public static void connect(Object client, String name, String address) throws Exception {
        SharedState.beginVanillaConnect();
        VideoBackground.suspendForGameScreen();

        Class<?> screenClass = GameActions.findClassForBridge(
                "net.minecraft.client.gui.screen.Screen", "net.minecraft.class_437");
        Class<?> clientClass = GameActions.findClassForBridge(
                "net.minecraft.client.MinecraftClient", "net.minecraft.class_310");
        Class<?> addressClass = GameActions.findClassForBridge(
                "net.minecraft.client.network.ServerAddress", "net.minecraft.class_639");
        Class<?> infoClass = GameActions.findClassForBridge(
                "net.minecraft.client.network.ServerInfo", "net.minecraft.class_642");

        if (screenClass == null || clientClass == null || addressClass == null || infoClass == null) {
            throw new IllegalStateException("Required Minecraft classes not loaded");
        }

        Object serverInfo = createServerInfo(infoClass, name, address);
        syncVanillaServerList(client, serverInfo, infoClass);

        if (tryQuickPlayConnect(client, clientClass, address)) {
            AgentLog.info("VANILLA_CONNECT via QuickPlay: " + name + " -> " + address);
            return;
        }

        Class<?> mpClass = GameActions.findClassForBridge(
                "net.minecraft.client.gui.screen.multiplayer.MultiplayerScreen", "net.minecraft.class_500");
        if (mpClass != null && tryMultiplayerScreenConnect(client, screenClass, mpClass, infoClass, name, address)) {
            AgentLog.info("VANILLA_CONNECT via MultiplayerScreen: " + name + " -> " + address);
            return;
        }

        Class<?> connectClass = GameActions.findClassForBridge(
                "net.minecraft.client.gui.screen.multiplayer.ConnectScreen", "net.minecraft.class_412");
        if (connectClass == null) {
            throw new IllegalStateException("ConnectScreen class not loaded");
        }

        Object serverAddress = parseServerAddress(addressClass, address);
        Object titleParent = resolveTitleScreen(client, screenClass);
        Object mpParent = mpClass != null
                ? mpClass.getConstructor(screenClass).newInstance(titleParent)
                : titleParent;
        Object cookieStorage = resolveCookieStorage(client);
        if (cookieStorage == null) {
            Class<?> cookieClass = GameActions.findClassForBridge(
                    "net.minecraft.client.network.CookieStorage", "net.minecraft.class_9112");
            if (cookieClass != null) {
                cookieStorage = cookieClass.getConstructor(java.util.Map.class)
                        .newInstance(Collections.emptyMap());
            }
        }

        Method connect = findConnectMethod(connectClass, screenClass, clientClass, addressClass, infoClass);
        Object[] args = buildConnectArgs(connect.getParameterTypes(), screenClass, clientClass, addressClass,
                infoClass, mpParent, client, serverAddress, serverInfo, cookieStorage);
        connect.invoke(null, args);
        AgentLog.info("VANILLA_CONNECT via ConnectScreen: " + name + " -> " + address + " via "
                + connect.getName() + " " + connect.getParameterTypes().length + " params");
    }

    private static boolean tryQuickPlayConnect(Object client, Class<?> clientClass, String address) {
        try {
            Class<?> quickPlayClass = GameActions.findClassForBridge(
                    "net.minecraft.client.QuickPlay", "net.minecraft.class_8496");
            if (quickPlayClass == null) {
                return false;
            }
            Method start = findQuickPlayMultiplayer(quickPlayClass, clientClass);
            if (start == null) {
                return false;
            }
            start.setAccessible(true);
            start.invoke(null, client, address);
            return true;
        } catch (Throwable t) {
            AgentLog.info("QuickPlay connect skipped: " + t.getMessage());
            return false;
        }
    }

    private static Method findQuickPlayMultiplayer(Class<?> quickPlayClass, Class<?> clientClass) {
        Method named = null;
        Method fallback = null;
        for (Method method : quickPlayClass.getDeclaredMethods()) {
            if (!Modifier.isStatic(method.getModifiers())) {
                continue;
            }
            Class<?>[] params = method.getParameterTypes();
            if (params.length != 2 || !clientClass.isAssignableFrom(params[0]) || params[1] != String.class) {
                continue;
            }
            if ("startMultiplayer".equals(method.getName()) || "method_51263".equals(method.getName())) {
                named = method;
                break;
            }
            fallback = method;
        }
        return named != null ? named : fallback;
    }

    private static boolean tryMultiplayerScreenConnect(Object client, Class<?> screenClass, Class<?> mpClass,
                                                       Class<?> infoClass, String name, String address)
            throws ReflectiveOperationException {
        Object titleParent = resolveTitleScreen(client, screenClass);
        Object mpScreen = mpClass.getConstructor(screenClass).newInstance(titleParent);
        GameActions.invokeSetScreen(client, mpScreen);
        Object serverInfo = createServerInfo(infoClass, name, address);
        scheduleMultiplayerConnect(client, mpClass, infoClass, serverInfo, 0);
        return true;
    }

    private static void scheduleMultiplayerConnect(Object client, Class<?> mpClass, Class<?> infoClass,
                                                   Object serverInfo, int attempt) throws ReflectiveOperationException {
        Method execute = ReflectUtil.findInstanceMethod(client.getClass(), "execute", "method_1514", Runnable.class);
        execute.invoke(client, (Runnable) () -> {
            try {
                Object current = ReflectUtil.getCurrentScreen(client);
                if (!mpClass.isInstance(current)) {
                    if (attempt < 120) {
                        scheduleMultiplayerConnect(client, mpClass, infoClass, serverInfo, attempt + 1);
                    } else {
                        AgentLog.error("MultiplayerScreen not active after wait");
                        SharedState.endVanillaConnect();
                    }
                    return;
                }
                Method connect = findMultiplayerConnectMethod(mpClass, infoClass);
                connect.invoke(current, serverInfo);
            } catch (Throwable t) {
                AgentLog.error("MultiplayerScreen.connect failed", t);
                SharedState.endVanillaConnect();
            }
        });
    }

    private static Method findMultiplayerConnectMethod(Class<?> mpClass, Class<?> infoClass)
            throws NoSuchMethodException {
        Method best = null;
        for (Method method : mpClass.getDeclaredMethods()) {
            if (Modifier.isStatic(method.getModifiers())) {
                continue;
            }
            Class<?>[] params = method.getParameterTypes();
            if (params.length != 1 || !infoClass.isAssignableFrom(params[0])) {
                continue;
            }
            if ("connect".equals(method.getName()) || method.getName().startsWith("method_")) {
                best = method;
                if ("connect".equals(method.getName())) {
                    break;
                }
            }
        }
        if (best == null) {
            throw new NoSuchMethodException("MultiplayerScreen.connect(ServerInfo) not found");
        }
        best.setAccessible(true);
        return best;
    }

    private static Object parseServerAddress(Class<?> addressClass, String address) throws ReflectiveOperationException {
        ReflectiveOperationException last = null;
        for (Method method : addressClass.getDeclaredMethods()) {
            if (!Modifier.isStatic(method.getModifiers())) {
                continue;
            }
            Class<?>[] params = method.getParameterTypes();
            if (params.length == 1 && params[0] == String.class) {
                try {
                    method.setAccessible(true);
                    return method.invoke(null, address);
                } catch (ReflectiveOperationException e) {
                    last = e;
                }
            }
        }
        Method parse = ReflectUtil.findStaticMethod(addressClass, "parse", "method_2950", String.class);
        return parse.invoke(null, address);
    }

    private static Method findConnectMethod(Class<?> connectClass, Class<?> screenClass, Class<?> clientClass,
                                            Class<?> addressClass, Class<?> infoClass) throws NoSuchMethodException {
        Method best = null;
        int bestScore = -1;
        for (Method method : connectClass.getDeclaredMethods()) {
            if (!Modifier.isStatic(method.getModifiers())) {
                continue;
            }
            Class<?>[] params = method.getParameterTypes();
            if (params.length < 3) {
                continue;
            }
            int score = 0;
            boolean hasClient = false;
            boolean hasAddress = false;
            boolean hasInfo = false;
            for (Class<?> param : params) {
                if (screenClass.isAssignableFrom(param)) {
                    score += 4;
                } else if (clientClass.isAssignableFrom(param)) {
                    hasClient = true;
                    score += 8;
                } else if (addressClass.isAssignableFrom(param)) {
                    hasAddress = true;
                    score += 8;
                } else if (infoClass.isAssignableFrom(param)) {
                    hasInfo = true;
                    score += 8;
                } else if (param == boolean.class || param == Boolean.class) {
                    score += 1;
                } else if (param.getName().contains("CookieStorage") || param.getName().contains("class_9112")) {
                    score += 4;
                }
            }
            if (!hasClient || !hasAddress || !hasInfo) {
                continue;
            }
            if (score > bestScore) {
                bestScore = score;
                best = method;
            }
        }
        if (best == null) {
            throw new NoSuchMethodException("ConnectScreen.connect not found on " + connectClass.getName());
        }
        best.setAccessible(true);
        return best;
    }

    private static Object[] buildConnectArgs(Class<?>[] params, Class<?> screenClass, Class<?> clientClass,
                                             Class<?> addressClass, Class<?> infoClass, Object mpParent,
                                             Object client, Object serverAddress, Object serverInfo,
                                             Object cookieStorage) {
        Object[] args = new Object[params.length];
        for (int i = 0; i < params.length; i++) {
            Class<?> param = params[i];
            if (screenClass.isAssignableFrom(param)) {
                args[i] = mpParent;
            } else if (clientClass.isAssignableFrom(param)) {
                args[i] = client;
            } else if (addressClass.isAssignableFrom(param)) {
                args[i] = serverAddress;
            } else if (infoClass.isAssignableFrom(param)) {
                args[i] = serverInfo;
            } else if (param == boolean.class || param == Boolean.class) {
                args[i] = Boolean.FALSE;
            } else if (cookieStorage != null && param.isInstance(cookieStorage)) {
                args[i] = cookieStorage;
            } else if (param.getName().contains("CookieStorage") || param.getName().contains("class_9112")) {
                args[i] = cookieStorage;
            } else {
                throw new IllegalStateException("Unsupported ConnectScreen param: " + param.getName());
            }
        }
        return args;
    }

    private static Object resolveTitleScreen(Object client, Class<?> screenClass) throws ReflectiveOperationException {
        Object current = ReflectUtil.getCurrentScreen(client);
        if (ClassUtil.isTitleScreenInstance(current)) {
            return current;
        }
        Class<?> titleClass = GameActions.findClassForBridge(
                "net.minecraft.client.gui.screen.TitleScreen", "net.minecraft.class_442");
        Constructor<?> ctor = titleClass.getDeclaredConstructor();
        ctor.setAccessible(true);
        return ctor.newInstance();
    }

    private static Object createServerInfo(Class<?> infoClass, String name, String address)
            throws ReflectiveOperationException {
        ReflectiveOperationException last = null;
        for (Constructor<?> ctor : infoClass.getConstructors()) {
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
        if (last != null) {
            throw last;
        }
        throw new NoSuchMethodException("ServerInfo constructor not found");
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

    private static void syncVanillaServerList(Object client, Object serverInfo, Class<?> infoClass) {
        try {
            Class<?> listClass = GameActions.findClassForBridge(
                    "net.minecraft.client.network.ServerList", "net.minecraft.class_641");
            if (listClass == null) {
                return;
            }
            Object runDir = ReflectUtil.getRunDirectory(client);
            File dat = new File(runDir instanceof File f ? f : new File(String.valueOf(runDir)), "servers.dat");

            Constructor<?> ctor = null;
            for (Constructor<?> candidate : listClass.getConstructors()) {
                Class<?>[] params = candidate.getParameterTypes();
                if (params.length == 2 && File.class.isAssignableFrom(params[0]) && params[1] == boolean.class) {
                    ctor = candidate;
                    break;
                }
            }
            if (ctor == null) {
                return;
            }
            Object list = ctor.newInstance(dat, false);
            invokeIfPresent(list, "load", "method_3734");
            if (!containsServer(list, infoClass, serverInfo)) {
                invokeAdd(list, serverInfo);
            }
            invokeIfPresent(list, "save", "method_3735");
        } catch (Throwable t) {
            AgentLog.info("ServerList sync skipped: " + t.getMessage());
        }
    }

    private static boolean containsServer(Object list, Class<?> infoClass, Object serverInfo) {
        try {
            for (Method method : list.getClass().getMethods()) {
                if (method.getParameterCount() != 0 || method.getReturnType() == void.class) {
                    continue;
                }
                if (!java.util.List.class.isAssignableFrom(method.getReturnType())
                        && !method.getReturnType().isArray()) {
                    continue;
                }
                Object result = method.invoke(list);
                if (result instanceof java.util.List<?> entries) {
                    for (Object entry : entries) {
                        if (entry != null && infoClass.isInstance(entry) && sameAddress(entry, serverInfo)) {
                            return true;
                        }
                    }
                }
            }
        } catch (ReflectiveOperationException ignored) {
        }
        return false;
    }

    private static boolean sameAddress(Object a, Object b) {
        try {
            Method getAddress = a.getClass().getMethod("address");
            Object addrA = getAddress.invoke(a);
            Object addrB = getAddress.invoke(b);
            return addrA != null && addrA.equals(addrB);
        } catch (ReflectiveOperationException ignored) {
            return false;
        }
    }

    private static void invokeAdd(Object list, Object serverInfo) throws ReflectiveOperationException {
        for (Method method : list.getClass().getMethods()) {
            if (method.getParameterCount() != 1) {
                continue;
            }
            if (!method.getParameterTypes()[0].isInstance(serverInfo)) {
                continue;
            }
            if ("add".equals(method.getName()) || method.getName().startsWith("method_")) {
                method.invoke(list, serverInfo);
                return;
            }
        }
        ReflectUtil.findInstanceMethod(list.getClass(), "add", "method_3732", serverInfo.getClass())
                .invoke(list, serverInfo);
    }

    private static void invokeIfPresent(Object target, String named, String intermediary)
            throws ReflectiveOperationException {
        Method method = ReflectUtil.findInstanceMethod(target.getClass(), named, intermediary);
        method.invoke(target);
    }
}
