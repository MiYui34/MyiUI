package com.myiui.agent.netease;

import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;
import com.myiui.agent.AgentLog;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URI;
import java.net.URL;
import java.net.URLEncoder;
import java.nio.charset.StandardCharsets;
import java.util.Map;
import java.util.StringJoiner;

/**
 * 网易云 API HTTP 客户端：封装对 api-enhanced 服务的 GET/POST 调用。
 * 所有网络调用阻塞，调用方必须在独立线程执行（绝不在渲染线程调用）。
 */
public final class NetEaseClient {
    private NetEaseClient() {}

    /** GET 请求，返回顶层 JsonObject（去除 api-enhanced 的 {code, msg, ...} 外壳失败时返回 null）。 */
    public static JsonObject get(String endpoint, Map<String, String> params) throws IOException {
        return request("GET", endpoint, params, false);
    }

    /** GET 请求但不检查 API code 字段（用于 login/qr/check 等特殊接口，code 是业务状态码而非成功码）。 */
    public static JsonObject getRaw(String endpoint, Map<String, String> params) throws IOException {
        return requestRaw("GET", endpoint, params, false);
    }

    /** POST 请求。 */
    public static JsonObject post(String endpoint, Map<String, String> params) throws IOException {
        return request("POST", endpoint, params, true);
    }

    /** GET 但返回原始字节数组（用于下载封面/二维码图片）。 */
    public static byte[] getBytes(String url) throws IOException {
        HttpURLConnection conn = open(url, "GET", false);
        try {
            int code = conn.getResponseCode();
            if (code < 200 || code >= 300) {
                throw new IOException("HTTP " + code + " for " + url);
            }
            try (InputStream in = conn.getInputStream()) {
                return in.readAllBytes();
            }
        } finally {
            conn.disconnect();
        }
    }

    /** 直接 GET 一个完整 URL（用于下载网易云封面/二维码绝对 URL）。 */
    public static JsonObject getAbsolute(String absoluteUrl) throws IOException {
        HttpURLConnection conn = open(absoluteUrl, "GET", false);
        try {
            return readJsonResponse(conn);
        } finally {
            conn.disconnect();
        }
    }

    private static JsonObject request(String method, String endpoint, Map<String, String> params, boolean isPost)
            throws IOException {
        String url = buildRequestUrl(endpoint, params, isPost);
        HttpURLConnection conn = open(url, method, isPost);
        try {
            if (isPost && params != null && !params.isEmpty()) {
                conn.setDoOutput(true);
                byte[] body = encodeFormBody(params).getBytes(StandardCharsets.UTF_8);
                try (OutputStream os = conn.getOutputStream()) {
                    os.write(body);
                }
            }
            LoginManager.saveCookieFromHeaders(conn);
            return readJsonResponse(conn);
        } finally {
            conn.disconnect();
        }
    }

    /** 与 request 相同但不检查 API code 字段，直接返回解析后的 JSON。 */
    private static JsonObject requestRaw(String method, String endpoint, Map<String, String> params, boolean isPost)
            throws IOException {
        String url = buildRequestUrl(endpoint, params, isPost);
        HttpURLConnection conn = open(url, method, isPost);
        try {
            if (isPost && params != null && !params.isEmpty()) {
                conn.setDoOutput(true);
                byte[] body = encodeFormBody(params).getBytes(StandardCharsets.UTF_8);
                try (OutputStream os = conn.getOutputStream()) {
                    os.write(body);
                }
            }
            LoginManager.saveCookieFromHeaders(conn);
            // 读取原始响应，不检查 code 字段
            int code = conn.getResponseCode();
            InputStream stream = code >= 200 && code < 300 ? conn.getInputStream() : conn.getErrorStream();
            String body;
            try (InputStream in = stream) {
                body = in == null ? "" : new String(in.readAllBytes(), StandardCharsets.UTF_8);
            }
            if (body.isEmpty()) throw new IOException("HTTP " + code + " empty body");
            return JsonParser.parseString(body).getAsJsonObject();
        } finally {
            conn.disconnect();
        }
    }

    private static HttpURLConnection open(String url, String method, boolean isPost) throws IOException {
        HttpURLConnection conn = (HttpURLConnection) URI.create(url).toURL().openConnection();
        conn.setRequestMethod(method);
        conn.setConnectTimeout(NetEaseConfig.requestTimeoutMs());
        conn.setReadTimeout(NetEaseConfig.requestTimeoutMs());
        conn.setRequestProperty("User-Agent", "MyiUI-NetEase/1.0");
        conn.setRequestProperty("Accept", "application/json");
        // 注入 cookie（登录后）
        String cookie = LoginManager.cookieHeader();
        if (cookie != null && !cookie.isEmpty()) {
            conn.setRequestProperty("Cookie", cookie);
        }
        if (isPost) {
            conn.setRequestProperty("Content-Type", "application/x-www-form-urlencoded");
        }
        conn.setInstanceFollowRedirects(true);
        return conn;
    }

    private static JsonObject readJsonResponse(HttpURLConnection conn) throws IOException {
        int code = conn.getResponseCode();
        InputStream stream = code >= 200 && code < 300 ? conn.getInputStream() : conn.getErrorStream();
        String body;
        try (InputStream in = stream) {
            body = in == null ? "" : new String(in.readAllBytes(), StandardCharsets.UTF_8);
        }
        if (body.isEmpty()) {
            throw new IOException("HTTP " + code + " empty body");
        }
        try {
            JsonElement root = JsonParser.parseString(body);
            if (!root.isJsonObject()) {
                throw new IOException("non-object response: " + truncate(body));
            }
            JsonObject obj = root.getAsJsonObject();
            // api-enhanced 通常返回 {code:200, msg:"ok", ...真实数据...}
            int apiCode = obj.has("code") ? obj.get("code").getAsInt() : 200;
            if (apiCode != 200) {
                String msg = obj.has("msg") ? obj.get("msg").getAsString() : (obj.has("message") ? obj.get("message").getAsString() : "");
                throw new IOException("api code=" + apiCode + " msg=" + msg);
            }
            return obj;
        } catch (Throwable t) {
            throw new IOException("parse failed (" + code + "): " + truncate(body), t);
        }
    }

    private static String buildRequestUrl(String endpoint, Map<String, String> params, boolean isPost) {
        String url = NetEaseConfig.baseUrl() + "/" + endpoint.replaceFirst("^/+", "");
        if (!isPost && params != null && !params.isEmpty()) {
            url += "?" + encodeFormBody(params);
        }
        return url;
    }

    private static String encodeFormBody(Map<String, String> params) {
        if (params == null || params.isEmpty()) return "";
        StringJoiner sj = new StringJoiner("&");
        for (Map.Entry<String, String> e : params.entrySet()) {
            sj.add(URLEncoder.encode(e.getKey(), StandardCharsets.UTF_8) + "="
                    + URLEncoder.encode(e.getValue() == null ? "" : e.getValue(), StandardCharsets.UTF_8));
        }
        return sj.toString();
    }

    private static String truncate(String s) {
        return s.length() <= 200 ? s : s.substring(0, 200) + "...";
    }
}
