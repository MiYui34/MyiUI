package com.myiui.agent;

public final class GameDataBridge {
    private GameDataBridge() {}

    public static String dispatchQuery(String cmd) {
        try {
        if (cmd.equals("GET_WORLDS")) {
            String json = WorldBridge.getWorldsJson();
            return json != null ? "OK " + json : "ERR worlds";
        }
        if (cmd.equals("GET_SERVERS")) {
            String json = ServerBridge.getServersJson();
            return json != null ? "OK " + json : "ERR servers";
        }
        if (cmd.startsWith("GET_OPTIONS:")) {
            String category = cmd.substring("GET_OPTIONS:".length()).trim();
            if ("options_controls".equals(category)) {
                String keybindJson = ControlsBridge.getKeybindsJson();
                String optionJson = OptionsBridge.getOptionsJson(category);
                String merged = mergeControlsJson(keybindJson, optionJson);
                return merged != null ? "OK " + merged : "ERR keybinds";
            }
            if ("options_resource_packs".equals(category)) {
                String json = ResourcePackBridge.getPacksJson();
                return json != null ? "OK " + json : "ERR packs";
            }
            String json = OptionsBridge.getOptionsJson(category);
            return json != null ? "OK " + json : "ERR options";
        }
        if (cmd.equals("GET_KEYBINDS")) {
            String json = ControlsBridge.getKeybindsJson();
            return json != null ? "OK " + json : "ERR keybinds";
        }
        if (cmd.equals("GET_RESOURCE_PACKS")) {
            return "OK " + ResourcePackBridge.getPacksJson();
        }
        if (cmd.equals("GET_PLAYER")) {
            String json = PlayerBridge.getPlayerJson();
            return json != null ? "OK " + json : "ERR player";
        }
        if (cmd.equals("GET_DISCONNECT_REASON")) {
            String reason = SharedState.consumeDisconnectReason();
            return reason != null ? "OK " + reason : "OK";
        }
        if (cmd.equals("ISLAND_SLOT_CONFIG")) {
            return "OK " + IslandManager.slotsConfigJson();
        }
        if (cmd.startsWith("NE_")) {
            return com.myiui.agent.netease.NetEaseBridge.dispatch(cmd);
        }
        AgentLog.info("dispatchQuery: falling back to dispatchAction for [" + cmd + "]");
        String actionResult = dispatchAction(cmd);
        return actionResult != null ? actionResult : "ERR unknown:" + cmd;
        } catch (Throwable t) {
            AgentLog.error("dispatchQuery threw for [" + cmd + "]", t);
            return "ERR query_exception:" + t.getClass().getSimpleName() + ":" + t.getMessage();
        }
    }

    public static String dispatchAction(String cmd) {
        if (cmd.startsWith("JOIN_WORLD:")) {
            return WorldBridge.joinWorld(cmd.substring("JOIN_WORLD:".length()).trim()) ? "OK" : "ERR join";
        }
        if (cmd.startsWith("DELETE_WORLD:")) {
            return WorldBridge.deleteWorld(cmd.substring("DELETE_WORLD:".length()).trim()) ? "OK" : "ERR delete";
        }
        if (cmd.equals("CREATE_WORLD")) {
            return WorldBridge.createWorld() ? "OK" : "ERR create";
        }
        if (cmd.startsWith("CONNECT_SERVER:")) {
            return ServerBridge.connectServer(cmd.substring("CONNECT_SERVER:".length()).trim()) ? "OK" : "ERR connect";
        }
        if (cmd.equals("ADD_SERVER")) {
            return ServerBridge.addServer() ? "OK" : "ERR add";
        }
        if (cmd.startsWith("ADD_SERVER_SUBMIT:")) {
            String rest = cmd.substring("ADD_SERVER_SUBMIT:".length());
            int sep = rest.indexOf(':');
            if (sep < 0) return "ERR add";
            String name = rest.substring(0, sep).trim();
            String address = rest.substring(sep + 1).trim();
            if (name.isEmpty() || address.isEmpty()) return "ERR add";
            return ServerBridge.addServerEntry(name, address) ? "OK" : "ERR add";
        }
        if (cmd.startsWith("EDIT_SERVER_SUBMIT:")) {
            String rest = cmd.substring("EDIT_SERVER_SUBMIT:".length());
            String[] parts = rest.split(":", 3);
            if (parts.length < 3) return "ERR edit";
            String id = parts[0].trim();
            String name = parts[1].trim();
            String address = parts[2].trim();
            if (id.isEmpty() || name.isEmpty() || address.isEmpty()) return "ERR edit";
            return ServerBridge.editServerEntry(id, name, address) ? "OK" : "ERR edit";
        }
        if (cmd.startsWith("DELETE_SERVER:")) {
            return ServerBridge.deleteServerEntry(cmd.substring("DELETE_SERVER:".length()).trim()) ? "OK" : "ERR delete";
        }
        if (cmd.startsWith("CREATE_WORLD_SUBMIT:")) {
            String rest = cmd.substring("CREATE_WORLD_SUBMIT:".length());
            String[] parts = rest.split(":", 3);
            if (parts.length < 2) return "ERR create";
            String name = parts[0].trim();
            String mode = parts[1].trim();
            String seed = parts.length > 2 ? parts[2].trim() : "";
            if (name.isEmpty()) return "ERR create";
            return WorldBridge.submitCreateWorld(name, mode, seed) ? "OK" : "ERR create";
        }
        if (cmd.equals("QUIT")) {
            return GameActions.quit() ? "OK" : "ERR quit";
        }
        if (cmd.equals("OPEN_SINGLEPLAYER")) {
            GameActions.openSingleplayer();
            return "OK";
        }
        if (cmd.equals("OPEN_MULTIPLAYER")) {
            GameActions.openMultiplayer();
            return "OK";
        }
        if (cmd.equals("OPEN_OPTIONS")) {
            GameActions.openOptions();
            return "OK";
        }
        if (cmd.equals("OPEN_VIDEO_OPTIONS")) {
            GameActions.openVideoOptions();
            return "OK";
        }
        if (cmd.startsWith("SET_BG_VIDEO:")) {
            String path = cmd.substring("SET_BG_VIDEO:".length());
            VideoBackground.reload(path);
            return "OK";
        }
        if (cmd.equals("RELOAD_BG")) {
            VideoBackground.reload(null);
            return "OK";
        }
        if (cmd.equals("OVERLAY_READY")) {
            SharedState.setOverlayAck(true);
            return "OK";
        }
        if (cmd.equals("OVERLAY_SUSPEND")) {
            SharedState.setOverlayAck(false);
            return "OK";
        }
        if (cmd.startsWith("SET_OPTION:")) {
            String rest = cmd.substring("SET_OPTION:".length());
            int eq = rest.indexOf('=');
            if (eq <= 0) return "ERR option";
            return OptionsBridge.setOption(rest.substring(0, eq), rest.substring(eq + 1)) ? "OK" : "ERR option";
        }
        if (cmd.startsWith("SET_KEYBIND:")) {
            String rest = cmd.substring("SET_KEYBIND:".length());
            int eq = rest.indexOf('=');
            if (eq <= 0) return "ERR keybind";
            return ControlsBridge.setKeybind(rest.substring(0, eq), rest.substring(eq + 1)) ? "OK" : "ERR keybind";
        }
        if (cmd.startsWith("SET_PACK_ORDER:")) {
            String rest = cmd.substring("SET_PACK_ORDER:".length());
            String[] parts = rest.split(":", 2);
            if (parts.length < 2) return "ERR pack";
            return ResourcePackBridge.setPackOrder(parts[0], parts[1]) ? "OK" : "ERR pack";
        }
        if (cmd.startsWith("SET_PACK_TOGGLE:")) {
            String ref = cmd.substring("SET_PACK_TOGGLE:".length()).trim();
            if (ref.isEmpty()) return "ERR pack";
            return ResourcePackBridge.togglePack(ref) ? "OK" : "ERR pack";
        }
        if (cmd.equals("OPEN_RESOURCE_PACKS_FOLDER")) {
            return ResourcePackBridge.openResourcePacksFolder() ? "OK" : "ERR folder";
        }
        if (cmd.equals("ISLAND_CLEAR")) {
            IslandManager.clear();
            return "OK";
        }
        if (cmd.equals("ISLAND_DEMO_CYCLE")) {
            IslandManager.startDemoCycle();
            return "OK";
        }
        if (cmd.startsWith("ISLAND_NOTIFY:")) {
            String rest = cmd.substring("ISLAND_NOTIFY:".length());
            String[] parts = rest.split("\\|", 4);
            if (parts.length < 4) {
                return "ERR island";
            }
            try {
                long duration = Long.parseLong(parts[2].trim());
                int slot = Integer.parseInt(parts[3].trim());
                IslandManager.addNotification(parts[0].trim(), parts[1].trim(), duration, slot);
                return "OK";
            } catch (NumberFormatException e) {
                return "ERR island";
            }
        }
        if (cmd.startsWith("NE_")) {
            return com.myiui.agent.netease.NetEaseBridge.dispatch(cmd);
        }
        AgentLog.info("dispatchAction: unknown cmd=[" + cmd + "]");
        return "ERR unknown_action:" + cmd;
    }

    private static String mergeControlsJson(String keybindJson, String optionJson) {
        if (keybindJson == null || keybindJson.isEmpty()) {
            return optionJson != null ? optionJson : "{}";
        }
        if (optionJson == null || optionJson.length() <= 2) {
            return keybindJson;
        }
        String inner = optionJson.substring(1, optionJson.length() - 1).trim();
        if (inner.isEmpty()) {
            return keybindJson;
        }
        int close = keybindJson.lastIndexOf('}');
        if (close < 0) {
            return keybindJson;
        }
        String prefix = keybindJson.substring(0, close);
        if (!prefix.endsWith("{") && !prefix.endsWith(",")) {
            prefix += ",";
        }
        return prefix + inner + "}";
    }
}
