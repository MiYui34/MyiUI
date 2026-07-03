#include "ui/screens/menu_screens.h"

#include <cctype>
#include <cmath>
#include <set>
#include <string>
#include <vector>

#include "ui/async_pipe.h"
#include "ui/fonts.h"
#include "ui/glass_panel.h"
#include "ui/menu_data.h"
#include "ui/screen_id.h"
#include "ui/ui_scale.h"
#include "ui/strings_zh.h"
#include "ui/widgets/animated_toggle.h"
#include "ui/widgets/yc_segment_selector.h"
#include "ui/widgets/yc_slider.h"
#include "ui/widgets/screen_shell.h"
#include "ui/widgets/glass_button.h"

using myiui::ui::ColorFromRGBA;

static std::string StripMcFormatting(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        if (static_cast<unsigned char>(value[i]) == 0xC2 && i + 1 < value.size() &&
            static_cast<unsigned char>(value[i + 1]) == 0xA7) {
            i += 2;
            continue;
        }
        if (static_cast<unsigned char>(value[i]) == 0xA7) {
            ++i;
            continue;
        }
        out.push_back(value[i]);
    }
    return out.empty() ? value : out;
}

static std::string RowValue(const SettingRowSpec& row, const std::string& optionsJson) {
    std::string val = GetOptionValueFromJson(optionsJson, row.key);
    if (val.empty()) val = row.default_val;
    return val;
}

static float SafeStof(const std::string& s, float fallback) {
    if (s.empty()) return fallback;
    try {
        size_t idx = 0;
        const float v = std::stof(s, &idx);
        if (idx == 0) return fallback;
        return v;
    } catch (...) {
        return fallback;
    }
}

static float SliderUiValue(const SettingRowSpec& row, const std::string& optionsJson) {
    const std::string val = RowValue(row, optionsJson);
    const float fallback = SafeStof(row.default_val, row.min_val);
    float v = SafeStof(val, fallback);
    if (row.max_val > 1.f && v > 0.f && v <= 1.f && (row.max_val - row.min_val) > 1.f) {
        v = row.min_val + v * (row.max_val - row.min_val);
    }
    return (std::max)(row.min_val, (std::min)(row.max_val, std::round(v)));
}

static float RowTextY(float rowY, float rowH, ImFont* font) {
    const float fontSize = font ? font->FontSize : 16.f;
    return rowY + (rowH - fontSize) * 0.5f;
}

static int FindEnumIndex(const std::vector<std::string>& options, const std::string& val) {
    for (size_t i = 0; i < options.size(); ++i) {
        if (options[i] == val) return static_cast<int>(i);
    }
    for (size_t i = 0; i < options.size(); ++i) {
        if (options[i].size() == val.size()) {
            bool same = true;
            for (size_t j = 0; j < val.size(); ++j) {
                if (std::tolower(static_cast<unsigned char>(options[i][j])) !=
                    std::tolower(static_cast<unsigned char>(val[j]))) {
                    same = false;
                    break;
                }
            }
            if (same) return static_cast<int>(i);
        }
    }
    return 0;
}

static bool IsReadOnlyOption(const std::string& key) {
    static const std::set<std::string> kReadOnly = {"output_device", "date_format", "skin_source", "upload_skin"};
    return kReadOnly.count(key) > 0;
}

static void QueueOptionChange(MenuRenderContext& ctx, const std::string& key, const std::string& value) {
    if (IsReadOnlyOption(key)) {
        ctx.state.pending_toast = "\xe8\xae\xbe\xe7\xbd\xae\xe9\xa1\xb9\xe6\x9a\x82\xe4\xb8\x8d\xe6\x94\xaf\xe6\x8c\x81";
        ctx.state.pending_toast_error = true;
        ctx.state.pending_toast_ms = 3200.f;
        return;
    }
    SetOptionValueInJson(ctx.state.data.options_json, key, value);

    std::string baseline = GetOptionValueFromJson(ctx.state.data.options_baseline_json, key);
    if (baseline.empty()) {
        for (const auto& row : ctx.state.options_spec.rows) {
            if (row.key == key) {
                baseline = row.default_val;
                break;
            }
        }
    }
    if (baseline == value) {
        ctx.state.pending_option_changes.erase(key);
    } else {
        ctx.state.pending_option_changes[key] = value;
    }
}

static void FlushPendingOptions(MenuRenderContext& ctx) {
    for (const auto& entry : ctx.state.slider_drafts) {
        QueueOptionChange(ctx, entry.first, std::to_string(static_cast<int>(entry.second)));
    }
    ctx.state.slider_drafts.clear();

    if (ctx.state.pending_option_changes.empty()) {
        ctx.state.pending_toast = myiui::strings::kToastOptionsNoChanges;
        ctx.state.pending_toast_error = false;
        ctx.state.pending_toast_ms = 2200.f;
        return;
    }

    for (const auto& entry : ctx.state.pending_option_changes) {
        if (IsReadOnlyOption(entry.first)) continue;
        MenuAppRunPipeAction(ctx.state, "SET_OPTION:" + entry.first + "=" + entry.second, false);
    }
    ctx.state.pending_option_changes.clear();
    ctx.state.data.options_baseline_json = ctx.state.data.options_json;
    ctx.state.pending_toast = myiui::strings::kToastOptionsApplied;
    ctx.state.pending_toast_error = false;
    ctx.state.pending_toast_ms = 2200.f;
}

static void AdvanceScrollRow(float y, float rowW, float rowH) {
    ImGui::SetCursorPos(ImVec2(0.f, y + rowH));
}

static void AppendScrollBottomPad(float y, float rowW, float scale) {
    ImGui::SetCursorPos(ImVec2(0.f, y));
    ImGui::Dummy(ImVec2(rowW, Px(20.f, scale)));
}

static bool HasOptionsContent(const MenuAppState& state) {
    if (!state.data.keybinds.empty() || !state.data.packs.empty()) return true;
    if (!state.options_spec.rows.empty()) return true;
    if (!state.data.options_json.empty()) return true;
    return false;
}

static void EnsureOptionsLoaded(MenuRenderContext& ctx, ScreenId id, float deltaMs) {
    const char* jsonName = OptionsScreenJsonName(id);
    if (!jsonName) return;

    if (ctx.state.data.category != jsonName) {
        LoadOptionsScreenSpec(ctx.cfg.root_path, jsonName, ctx.state.options_spec);
    } else {
        for (const auto& row : ctx.state.options_spec.rows) {
            if (row.type == SettingRowType::Enum && row.options.empty()) {
                LoadOptionsScreenSpec(ctx.cfg.root_path, jsonName, ctx.state.options_spec);
                break;
            }
        }
    }
    if (ctx.state.data.category == jsonName && ctx.state.data.options_fetch_done) return;

    if (ctx.state.data.options_loading) {
        ctx.state.data.options_load_ms += deltaMs;
        if (ctx.state.data.options_load_ms > 6500.f) {
            ctx.state.data.options_loading = false;
            ctx.state.data.options_fetch_done = true;
        }
        return;
    }

    ctx.state.data.options_loading = true;
    ctx.state.data.options_load_ms = 0.f;
    ctx.state.data.options_fetch_done = false;
    ctx.state.data.category = jsonName;
    ctx.state.slider_drafts.clear();
    ctx.state.pending_option_changes.clear();
    ctx.state.data.options_json.clear();
    ctx.state.data.options_baseline_json.clear();
    ctx.state.data.keybinds.clear();
    ctx.state.data.packs.clear();

    MenuAppStartPipeLoad(ctx.state, PipeLoadKind::Options, std::string("GET_OPTIONS:") + jsonName, jsonName);
}

static void RenderOptionsRows(MenuRenderContext& ctx, ScreenRouter& router, ScreenId id, ImDrawList* dl,
                              const ScreenShellLayout& layout, const UiFonts& fonts, float scale, float alpha,
                              float rowW, float rowH, float gap) {
    float y = 0.f;

    if (id == ScreenId::OptionsResourcePacks) {
        if (ctx.state.data.options_loading) {
            DrawTextStyled(dl, fonts.regular, ImVec2(0.f, y), ColorFromRGBA(ctx.cfg.theme.text_dim, alpha),
                           "\xe5\x8a\xa0\xe8\xbd\xbd\xe4\xb8\xad...");
            AppendScrollBottomPad(y + rowH, rowW, scale);
            return;
        }
        if (ctx.state.data.packs.empty()) {
            DrawTextStyled(dl, fonts.regular, ImVec2(0.f, y), ColorFromRGBA(ctx.cfg.theme.text_dim, alpha),
                           "\xe6\x9a\x82\xe6\x97\xa0\xe5\x8f\xaf\xe7\x94\xa8\xe8\xb5\x84\xe6\xba\x90\xe5\x8c\x85");
            AppendScrollBottomPad(y + rowH, rowW, scale);
            return;
        }
        int pi = 0;
        for (const auto& pack : ctx.state.data.packs) {
            ImGui::PushID(pi);
            ImGui::SetCursorPos(ImVec2(0.f, y));
            const ImVec2 screenPos = ImGui::GetCursorScreenPos();
            const ImVec2 size(rowW, rowH);
            const ImVec2 rectMax(screenPos.x + size.x, screenPos.y + size.y);
            const int* fill = pack.enabled ? ctx.cfg.theme.accent_hover_bg : ctx.cfg.theme.glass_tint;
            myiui::ui::DrawGlassSurface(dl, screenPos, rectMax, fill, ctx.cfg.theme.border_color, Px(8.f, scale), 2.f, alpha);

            const std::string displayName = StripMcFormatting(pack.name);
            DrawTextStyled(dl, fonts.regular, ImVec2(screenPos.x + Px(12.f, scale), RowTextY(screenPos.y, rowH, fonts.regular)),
                           ColorFromRGBA(ctx.cfg.theme.text_primary, alpha), displayName.c_str());
            if (pack.enabled) {
                DrawTextStyled(dl, fonts.caption, ImVec2(screenPos.x + Px(12.f, scale), screenPos.y + Px(36.f, scale)),
                               ColorFromRGBA(ctx.cfg.theme.accent, alpha), "\xe5\xb7\xb2\xe5\x90\xaf\xe7\x94\xa8");
            }

            const ImVec2 btnSize(Px(52.f, scale), Px(28.f, scale));
            const float btnY = screenPos.y + (rowH - btnSize.y) * 0.5f;
            const float toggleX = rectMax.x - Px(58.f, scale);
            const float downX = toggleX - btnSize.x - Px(6.f, scale);
            const float upX = downX - btnSize.x - Px(6.f, scale);
            const std::string packArg = pack.id.empty() ? std::to_string(pi) : pack.id;
            const bool canInteract = !pack.locked && !router.IsTransitioning();
            bool pressed = false;

            if (pack.enabled) {
                float hoverUp = ctx.state.hover_anim[40 + pi * 3];
                if (GlassButton("##pack_up", ImVec2(upX, btnY), btnSize, "\xe4\xb8\x8a\xe7\xa7\xbb", ctx.cfg, hoverUp,
                                pressed, fonts.caption, GlassButtonStyle::Default, MenuIcon::None, scale,
                                canInteract)) {
                    MenuAppRunPipeAction(ctx.state, "SET_PACK_ORDER:up:" + packArg, false);
                }
                ctx.state.hover_anim[40 + pi * 3] = hoverUp;

                float hoverDown = ctx.state.hover_anim[41 + pi * 3];
                if (GlassButton("##pack_down", ImVec2(downX, btnY), btnSize, "\xe4\xb8\x8b\xe7\xa7\xbb", ctx.cfg,
                                hoverDown, pressed, fonts.caption, GlassButtonStyle::Default, MenuIcon::None, scale,
                                canInteract)) {
                    MenuAppRunPipeAction(ctx.state, "SET_PACK_ORDER:down:" + packArg, false);
                }
                ctx.state.hover_anim[41 + pi * 3] = hoverDown;
            }

            float hoverToggle = ctx.state.hover_anim[42 + pi * 3];
            const char* toggleLabel = pack.enabled ? "\xe7\xa6\x81\xe7\x94\xa8" : "\xe5\x90\xaf\xe7\x94\xa8";
            const GlassButtonStyle toggleStyle = pack.enabled ? GlassButtonStyle::Danger : GlassButtonStyle::Primary;
            if (GlassButton("##pack_toggle", ImVec2(toggleX, btnY), btnSize, toggleLabel, ctx.cfg, hoverToggle, pressed,
                            fonts.caption, toggleStyle, MenuIcon::None, scale, canInteract)) {
                MenuAppRunPipeAction(ctx.state, "SET_PACK_TOGGLE:" + packArg, false);
            }
            ctx.state.hover_anim[42 + pi * 3] = hoverToggle;

            AdvanceScrollRow(y, rowW, rowH);
            y += rowH + gap;
            ++pi;
            ImGui::PopID();
        }
        AppendScrollBottomPad(y, rowW, scale);
        return;
    }

    for (const auto& row : ctx.state.options_spec.rows) {
        if (id == ScreenId::OptionsControls && row.type == SettingRowType::Keybind) {
            continue;
        }
        ImGui::PushID(row.key.c_str());
        ImGui::SetCursorPos(ImVec2(0.f, y));
        const ImVec2 screenPos = ImGui::GetCursorScreenPos();
        const ImVec2 size(rowW, rowH);
        const ImVec2 rectMax(screenPos.x + size.x, screenPos.y + size.y);
        const std::string val = RowValue(row, ctx.state.data.options_json);
        const float textY = RowTextY(screenPos.y, rowH, fonts.regular);

        if (row.type == SettingRowType::Keybind) {
            myiui::ui::DrawGlassSurface(dl, screenPos, rectMax, ctx.cfg.theme.glass_tint, ctx.cfg.theme.border_color,
                                        Px(8.f, scale), 2.f, alpha);
            DrawTextStyled(dl, fonts.regular, ImVec2(screenPos.x + Px(12.f, scale), textY),
                           ColorFromRGBA(ctx.cfg.theme.text_primary, alpha), row.label.c_str());
            DrawTextStyled(dl, fonts.regular, ImVec2(rectMax.x - Px(80.f, scale), textY),
                           ColorFromRGBA(ctx.cfg.theme.accent, alpha), val.empty() ? "-" : val.c_str());
            if (row.key == "upload_skin") {
                ImGui::SetCursorPos(ImVec2(0.f, y));
                if (ImGui::InvisibleButton("upload_skin_btn", size) && !router.IsTransitioning()) {
                    ctx.state.pending_toast = "\xe8\xaf\xa5\xe5\x8a\x9f\xe8\x83\xbd\xe6\x9a\x82\xe6\x9c\xaa\xe5\xae\x9e\xe7\x8e\xb0";
                    ctx.state.pending_toast_error = true;
                    ctx.state.pending_toast_ms = 2800.f;
                }
            }
        } else if (row.type == SettingRowType::Slider) {
            const float labelW = rowW * 0.28f;
            const float valueW = Px(52.f, scale);
            const float sliderW = rowW - labelW - valueW - Px(24.f, scale);
            myiui::ui::DrawGlassSurface(dl, screenPos, rectMax, ctx.cfg.theme.glass_tint, ctx.cfg.theme.border_color,
                                        Px(8.f, scale), 2.f, alpha);
            DrawTextStyled(dl, fonts.regular, ImVec2(screenPos.x + Px(12.f, scale), textY),
                           ColorFromRGBA(ctx.cfg.theme.text_primary, alpha), row.label.c_str());
            float v = SliderUiValue(row, ctx.state.data.options_json);
            if (const auto draft = ctx.state.slider_drafts.find(row.key); draft != ctx.state.slider_drafts.end()) {
                v = draft->second;
            }
            ImGui::SetCursorScreenPos(
                ImVec2(screenPos.x + labelW + Px(4.f, scale), screenPos.y + (rowH - Px(32.f, scale)) * 0.5f));
            const auto sliderStyle = myiui::ui::YcSlider::StyleFromTheme(ctx.cfg.theme.accent, alpha);
            myiui::ui::YcSlider::Draw(row.key.c_str(), &v, row.min_val, row.max_val, sliderW / scale, scale,
                                      sliderStyle);
            v = (std::max)(row.min_val, (std::min)(row.max_val, std::round(v)));
            if (ImGui::IsItemActive()) {
                ctx.state.slider_drafts[row.key] = v;
                SetOptionValueInJson(ctx.state.data.options_json, row.key, std::to_string(static_cast<int>(v)));
            } else if (ImGui::IsItemDeactivated()) {
                const float finalV = ctx.state.slider_drafts.count(row.key) ? ctx.state.slider_drafts[row.key] : v;
                QueueOptionChange(ctx, row.key, std::to_string(static_cast<int>(finalV)));
                ctx.state.slider_drafts.erase(row.key);
                v = finalV;
            }
            char valBuf[16]{};
            snprintf(valBuf, sizeof(valBuf), "%.0f", v);
            DrawTextStyled(dl, fonts.regular, ImVec2(rectMax.x - valueW + Px(4.f, scale), textY),
                           ColorFromRGBA(ctx.cfg.theme.text_dim, alpha), valBuf);
        } else if (row.type == SettingRowType::Toggle) {
            myiui::ui::DrawGlassSurface(dl, screenPos, rectMax, ctx.cfg.theme.glass_tint, ctx.cfg.theme.border_color,
                                        Px(8.f, scale), 2.f, alpha);
            DrawTextStyled(dl, fonts.regular, ImVec2(screenPos.x + Px(12.f, scale), textY),
                           ColorFromRGBA(ctx.cfg.theme.text_primary, alpha), row.label.c_str());
            bool on = val == "true" || val == "1";
            const float switchW = Px(48.f, scale);
            const float switchH = Px(28.f, scale);
            ImGui::SetCursorScreenPos(
                ImVec2(rectMax.x - switchW - Px(12.f, scale), screenPos.y + (rowH - switchH) * 0.5f));
            if (myiui::ui::AnimatedToggle::Draw((std::string("sw_") + row.key).c_str(), &on, scale,
                                                ctx.cfg.theme.accent, nullptr, alpha)) {
                QueueOptionChange(ctx, row.key, on ? "true" : "false");
            }
        } else if (row.type == SettingRowType::Enum) {
            myiui::ui::DrawGlassSurface(dl, screenPos, rectMax, ctx.cfg.theme.glass_tint, ctx.cfg.theme.border_color,
                                        Px(8.f, scale), 2.f, alpha);
            DrawTextStyled(dl, fonts.regular, ImVec2(screenPos.x + Px(12.f, scale), textY),
                           ColorFromRGBA(ctx.cfg.theme.text_primary, alpha), row.label.c_str());
            const size_t optCount = row.options.size();
            if (IsReadOnlyOption(row.key)) {
                const char* displayVal = val.empty() ? "-" : val.c_str();
                const ImVec2 valSize =
                    fonts.regular->CalcTextSizeA(fonts.regular->FontSize, FLT_MAX, 0.f, displayVal);
                DrawTextStyled(dl, fonts.regular, ImVec2(rectMax.x - valSize.x - Px(14.f, scale), textY),
                               ColorFromRGBA(ctx.cfg.theme.text_dim, alpha), displayVal);
            } else if (optCount >= 2 && optCount <= 8) {
                int current = FindEnumIndex(row.options, val);
                std::vector<const char*> labels;
                labels.reserve(optCount);
                for (const auto& opt : row.options) labels.push_back(opt.c_str());
                const float labelW = rowW * 0.28f;
                const float segW = rowW - labelW - Px(12.f, scale);
                const float segH = Px(optCount > 4 ? 32.f : 36.f, scale);
                ImGui::SetCursorScreenPos(
                    ImVec2(screenPos.x + labelW, screenPos.y + (rowH - segH) * 0.5f));
                const auto segStyle =
                    myiui::ui::YcSegmentSelector::StyleFromTheme(ctx.cfg.theme.accent, ctx.cfg.theme.glass_tint, alpha);
                if (myiui::ui::YcSegmentSelector::Draw((std::string("seg_") + row.key).c_str(), &current,
                                                       labels.data(), static_cast<int>(optCount), segW / scale, scale,
                                                       segStyle)) {
                    QueueOptionChange(ctx, row.key, row.options[static_cast<size_t>(current)]);
                }
            } else {
                ImGui::SetCursorPos(ImVec2(0.f, y));
                ImGui::InvisibleButton((std::string("enum_") + row.key).c_str(), size);
                const char* displayVal = val.empty() ? "-" : val.c_str();
                const ImVec2 valSize =
                    fonts.regular->CalcTextSizeA(fonts.regular->FontSize, FLT_MAX, 0.f, displayVal);
                DrawTextStyled(dl, fonts.regular, ImVec2(rectMax.x - valSize.x - Px(14.f, scale), textY),
                               ColorFromRGBA(ctx.cfg.theme.text_dim, alpha), displayVal);
                if (ImGui::IsItemClicked() && !row.options.empty()) {
                    size_t idx = 0;
                    for (size_t i = 0; i < row.options.size(); ++i) {
                        if (row.options[i] == val) {
                            idx = i;
                            break;
                        }
                    }
                    const size_t next = (idx + 1) % row.options.size();
                    QueueOptionChange(ctx, row.key, row.options[next]);
                }
            }
        }
        AdvanceScrollRow(y, rowW, rowH);
        y += rowH + gap;
        ImGui::PopID();
    }

    if (id == ScreenId::OptionsControls && !ctx.state.data.keybinds.empty()) {
        int ki = 0;
        for (const auto& kb : ctx.state.data.keybinds) {
            ImGui::PushID(ki);
            ImGui::SetCursorPos(ImVec2(0.f, y));
            const ImVec2 size(rowW, rowH);
            const ImVec2 screenPos = ImGui::GetCursorScreenPos();
            const ImVec2 rectMax(screenPos.x + size.x, screenPos.y + size.y);
            myiui::ui::DrawGlassSurface(dl, screenPos, rectMax, ctx.cfg.theme.glass_tint, ctx.cfg.theme.border_color,
                                        Px(8.f, scale), 2.f, alpha);
            DrawTextStyled(dl, fonts.regular, ImVec2(screenPos.x + Px(12.f, scale), RowTextY(screenPos.y, rowH, fonts.regular)),
                           ColorFromRGBA(ctx.cfg.theme.text_primary, alpha), kb.label.c_str());
            DrawTextStyled(dl, fonts.regular, ImVec2(rectMax.x - Px(80.f, scale), RowTextY(screenPos.y, rowH, fonts.regular)),
                           ColorFromRGBA(ctx.cfg.theme.accent, alpha), kb.key.c_str());
            ImGui::SetCursorPos(ImVec2(rowW - Px(72.f, scale), y));
            if (ImGui::InvisibleButton("##kb", ImVec2(Px(72.f, scale), rowH))) {
                ctx.state.keybind_capture_index = ki;
            }
            if (ctx.state.keybind_capture_index == ki) {
                for (int k = static_cast<int>(ImGuiKey_Tab); k < static_cast<int>(ImGuiKey_GamepadRStickDown); ++k) {
                    if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(k))) {
                        MenuAppRunPipeAction(ctx.state, "SET_KEYBIND:" + kb.id + "=" + std::to_string(k));
                        ctx.state.keybind_capture_index = -1;
                        ctx.state.data.options_fetch_done = false;
                        break;
                    }
                }
            }
            AdvanceScrollRow(y, rowW, rowH);
            y += rowH + gap;
            ++ki;
            ImGui::PopID();
        }
    }

    AppendScrollBottomPad(y, rowW, scale);
}

void RenderOptionsDetailScreen(MenuRenderContext& ctx, ScreenRouter& router) {
    if (ctx.state.show_manager) return;
    const ScreenId id = router.Current();
    const float deltaMs = ImGui::GetIO().DeltaTime * 1000.f;
    EnsureOptionsLoaded(ctx, id, deltaMs);

    const float scale = ctx.scale;
    const float alpha = ctx.state.transition.ContentAlpha();
    const ScreenShellLayout layout = CalcScreenShellLayout(ctx.cfg, scale);
    auto* dl = ImGui::GetWindowDrawList();
    DrawScreenShellBackground(dl, layout, ctx.cfg, scale, alpha);

    const UiFonts& fonts = GetUiFonts();
    const char* title = ctx.state.options_spec.title.empty() ? "\xe8\xae\xbe\xe7\xbd\xae" : ctx.state.options_spec.title.c_str();
    if (ScreenShellHeader("opt_det", layout, ctx.cfg, fonts, scale, title, "", !router.IsTransitioning(),
                           ctx.state.back_hover)) {
        router.Pop();
        return;
    }

    const bool usesDeferredApply =
        id != ScreenId::OptionsResourcePacks && !ctx.state.options_spec.rows.empty();
    if (id == ScreenId::OptionsResourcePacks) {
        const float pad = Px(ctx.cfg.components.content_padding, scale);
        const ImVec2 btnSize(Px(88.f, scale), Px(28.f, scale));
        const ImVec2 btnPos(layout.panel_pos.x + layout.panel_size.x - pad - btnSize.x,
                            layout.panel_pos.y + Px(12.f, scale));
        ImGui::SetCursorScreenPos(btnPos);
        bool pressed = false;
        float folderHover = ctx.state.apply_hover;
        if (GlassButton("opt_pack_folder", btnPos, btnSize, "\xe6\x89\x93\xe5\xbc\x80\xe6\x96\x87\xe4\xbb\xb6\xe5\xa4\xb9",
                        ctx.cfg, folderHover, pressed, fonts.caption, GlassButtonStyle::Default, MenuIcon::None, scale,
                        !router.IsTransitioning())) {
            MenuAppRunPipeAction(ctx.state, "OPEN_RESOURCE_PACKS_FOLDER", true);
        }
        ctx.state.apply_hover = folderHover;
    } else if (usesDeferredApply &&
        ScreenShellApplyButton("opt_det", layout, ctx.cfg, fonts, scale, !ctx.state.pending_option_changes.empty() ||
                                                                 !ctx.state.slider_drafts.empty(),
                               ctx.state.apply_hover)) {
        FlushPendingOptions(ctx);
    }

    ScreenContentClipGuard clip;
    if (ctx.state.data.options_loading) {
        clip.Begin(dl, layout);
        DrawTextStyled(dl, fonts.regular, layout.content_pos, ColorFromRGBA(ctx.cfg.theme.text_dim, alpha),
                       "\xe5\x8a\xa0\xe8\xbd\xbd\xe4\xb8\xad...");
        clip.End();
        return;
    }

    if (!HasOptionsContent(ctx.state)) {
        clip.Begin(dl, layout);
        DrawTextStyled(dl, fonts.regular, layout.content_pos, ColorFromRGBA(ctx.cfg.theme.text_dim, alpha),
                       "\xe6\x97\xa0\xe6\xb3\x95\xe5\x8a\xa0\xe8\xbd\xbd\xe9\x80\x89\xe9\xa1\xb9\xef\xbc\x8c\xe8\xaf\xb7\xe7\xa1\xae\xe8\xae\xa4 Agent \xe5\xb7\xb2\xe6\xb3\xa8\xe5\x85\xa5");
        clip.End();
        return;
    }

    const float rowH = Px(ctx.cfg.components.setting_row_h, scale);
    const float gap = Px(8.f, scale);
    const float rowW = layout.content_size.x;

    if (!BeginScreenContentScroll("opt_rows_scroll", layout)) {
        return;
    }
    RenderOptionsRows(ctx, router, id, ImGui::GetWindowDrawList(), layout, fonts, scale, alpha, rowW, rowH, gap);
    EndScreenContentScroll();
}
