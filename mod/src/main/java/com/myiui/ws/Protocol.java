package com.myiui.ws;

import com.google.gson.Gson;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;

/**
 * Shared JSON helpers for the MyiUI WebSocket protocol.
 *
 * Push:  { "type": "hud", "data": { ... } }
 * Req:   { "id": "1", "type": "action", "cmd": "QUIT", "data": {} }
 * Res:   { "id": "1", "ok": true, "data": { ... } }
 */
public final class Protocol {
    public static final Gson GSON = new Gson();

    private Protocol() {}

    public static JsonObject push(String type, JsonObject data) {
        JsonObject root = new JsonObject();
        root.addProperty("type", type);
        root.add("data", data == null ? new JsonObject() : data);
        return root;
    }

    public static JsonObject push(String type, ObjBuilder data) {
        return push(type, data == null ? new JsonObject() : data.build());
    }

    public static JsonObject response(String id, boolean ok, JsonElement data) {
        JsonObject root = new JsonObject();
        root.addProperty("id", id);
        root.addProperty("ok", ok);
        root.add("data", data == null ? new JsonObject() : data);
        return root;
    }

    public static JsonObject responseError(String id, String message) {
        JsonObject data = new JsonObject();
        data.addProperty("error", message);
        return response(id, false, data);
    }

    public static JsonObject parse(String text) {
        return JsonParser.parseString(text).getAsJsonObject();
    }

    public static ObjBuilder obj() {
        return new ObjBuilder();
    }

    public static final class ObjBuilder {
        private final JsonObject obj = new JsonObject();

        public ObjBuilder add(String key, String value) {
            if (value == null) {
                obj.add(key, null);
            } else {
                obj.addProperty(key, value);
            }
            return this;
        }

        public ObjBuilder add(String key, Number value) {
            obj.addProperty(key, value);
            return this;
        }

        public ObjBuilder add(String key, Boolean value) {
            obj.addProperty(key, value);
            return this;
        }

        public ObjBuilder add(String key, JsonElement value) {
            obj.add(key, value);
            return this;
        }

        public JsonObject build() {
            return obj;
        }

        /** Implicit conversion helper used as Protocol.obj().add(...). */
        public JsonObject toJson() {
            return obj;
        }
    }
}
