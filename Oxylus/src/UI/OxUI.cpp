#include "OxUI.hpp"

#include "Assets/AssetManager.hpp"
#include "Utils/FileDialogs.hpp"
#include "Utils/StringUtils.hpp"

#include <fmt/format.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

#include "ImGuiLayer.hpp"

#include "Assets/Texture.hpp"
#include "Core/App.hpp"
#include "imgui.h"

namespace ox {
inline static int ui_context_id = 0;
inline static int s_counter = 0;
char ui::id_buffer[16];

void ui::push_id() {
  ++ui_context_id;
  ImGui::PushID(ui_context_id);
  s_counter = 0;
}

void ui::pop_id() {
  ImGui::PopID();
  --ui_context_id;
}

bool ui::begin_properties(const ImGuiTableFlags flags, bool fixed_width, float width) {
  id_buffer[0] = '#';
  id_buffer[1] = '#';
  memset(id_buffer + 2, 0, 14);
  ++s_counter;
  const std::string buffer = fmt::format("##{}", s_counter);
  std::memcpy(&id_buffer, buffer.data(), 16);

  if (ImGui::BeginTable(id_buffer, 2, flags)) {
    ImGui::TableSetupColumn("Name");
    if (fixed_width)
      ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, ImGui::GetWindowWidth() * width);
    else
      ImGui::TableSetupColumn("Property");
    return true;
  }

  return false;
}

void ui::end_properties() { ImGui::EndTable(); }

void ui::begin_property_grid(const char* label, const char* tooltip, const bool align_text_right) {
  push_id();

  push_frame_style();

  ImGui::TableNextRow();
  if (align_text_right)
    ImGui::SetNextItemWidth(-1.0f);
  ImGui::TableNextColumn();

  ImGui::PushID(label);
  if (align_text_right) {
    const auto posX = ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - ImGui::CalcTextSize(label).x - ImGui::GetScrollX();
    if (posX > ImGui::GetCursorPosX())
      ImGui::SetCursorPosX(posX);
  }
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(label);

  tooltip_hover(tooltip);

  ImGui::TableNextColumn();
  ImGui::SetNextItemWidth(-1.0f);

  id_buffer[0] = '#';
  id_buffer[1] = '#';
  memset(id_buffer + 2, 0, 14);
  ++s_counter;
  const std::string buffer = fmt::format("##{}", s_counter);
  std::memcpy(&id_buffer, buffer.data(), 16);
}

void ui::end_property_grid() {
  ImGui::PopID();
  pop_frame_style();
  pop_id();
}

void ui::push_frame_style(bool on) { ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, on ? 1.f : 0.f); }
void ui::pop_frame_style() { ImGui::PopStyleVar(); }

bool ui::button(const char* label, const ImVec2& size, const char* tooltip) {
  push_frame_style();
  bool changed = ImGui::Button(label, size);
  tooltip_hover(tooltip);
  pop_frame_style();
  return changed;
}

bool ui::checkbox(const char* label, bool* v) {
  push_frame_style();
  bool changed = ImGui::Checkbox(label, v);
  pop_frame_style();
  return changed;
}

bool ui::combo(const char* label, int* value, const char** dropdown_strings, int count, const char* tooltip) {
  push_frame_style();

  bool modified = false;
  const char* current = dropdown_strings[*value];

  if (ImGui::BeginCombo(id_buffer, current)) {
    for (int i = 0; i < count; i++) {
      const bool is_selected = current == dropdown_strings[i];
      if (ImGui::Selectable(dropdown_strings[i], is_selected)) {
        current = dropdown_strings[i];
        *value = i;
        modified = true;
      }

      if (is_selected)
        ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }

  tooltip_hover(tooltip);

  pop_frame_style();

  return modified;
}

bool ui::input_text(const char* label, std::string* str, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data) {
  push_frame_style();
  bool changed = ImGui::InputText(label, str, flags, callback, user_data);
  pop_frame_style();
  return changed;
}

void ui::text(const char* text1, const char* text2, const char* tooltip) {
  begin_property_grid(text1, tooltip);
  ImGui::Text("%s", text2);
  end_property_grid();
}

bool ui::property(const char* label, bool* flag, const char* tooltip) {
  begin_property_grid(label, tooltip);
  const bool modified = ImGui::Checkbox(id_buffer, flag);
  end_property_grid();
  return modified;
}

bool ui::property(const char* label, std::string* text, const ImGuiInputFlags flags, const char* tooltip) {
  begin_property_grid(label, tooltip);
  const bool modified = ImGui::InputText(id_buffer, text, flags);
  end_property_grid();
  return modified;
}

bool ui::property(const char* label, int* value, const char** dropdown_strings, const int count, const char* tooltip) {
  begin_property_grid(label, tooltip);

  bool modified = false;
  const char* current = dropdown_strings[*value];

  if (ImGui::BeginCombo(id_buffer, current)) {
    for (int i = 0; i < count; i++) {
      const bool is_selected = current == dropdown_strings[i];
      if (ImGui::Selectable(dropdown_strings[i], is_selected)) {
        current = dropdown_strings[i];
        *value = i;
        modified = true;
      }

      if (is_selected)
        ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }

  end_property_grid();

  return modified;
}

void ui::tooltip_hover(const char* text) {
  if (text && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay)) {
    ImGui::BeginTooltip();
    ImGui::TextUnformatted(text);
    ImGui::EndTooltip();
  }
}

bool ui::property(const char* label, Shared<Texture>& texture, const char* tooltip, bool linear_sampling) {
  begin_property_grid(label, tooltip);
  bool changed = false;

  const float frame_height = ImGui::GetFrameHeight();
  const float button_size = frame_height * 3.0f;
  const ImVec2 x_button_size = {button_size / 4.0f, button_size};
  const float tooltip_size = frame_height * 11.0f;

  ImGui::SetCursorPos({ImGui::GetContentRegionMax().x - button_size - x_button_size.x, ImGui::GetCursorPosY() + ImGui::GetStyle().FramePadding.y});
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{0, 0});
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.25f, 0.25f, 0.25f, 1.0f});
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.35f, 0.35f, 0.35f, 1.0f});
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.25f, 0.25f, 0.25f, 1.0f});

  auto texture_load_func = [](Shared<Texture>& texture, bool& changed) {
    const auto& path = App::get_system<FileDialogs>()->open_file({{"Texture file", "png,jpg"}});
    if (!path.empty()) {
      texture = AssetManager::get_texture_asset({path});
      changed = true;
    }
  };

  if (texture) {
    if (ImGui::ImageButton(label, App::get()->get_imgui_layer()->add_image(*texture->get_view(), linear_sampling), {button_size, button_size})) {
      texture_load_func(texture, changed);
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay)) {
      ImGui::BeginTooltip();
      ImGui::TextUnformatted(texture->get_path().c_str());
      ImGui::Spacing();
      ImGui::Image(App::get()->get_imgui_layer()->add_image(*texture->get_view(), linear_sampling), {tooltip_size, tooltip_size});
      ImGui::EndTooltip();
    }
  } else {
    ImGui::PushFont(ImGuiLayer::bold_font);
    if (ImGui::Button("NO\nTEXTURE", {button_size, button_size})) {
      texture_load_func(texture, changed);
    }
    ImGui::PopFont();
  }
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
      const auto path = get_path_from_imgui_payload(payload);
      texture = AssetManager::get_texture_asset({path});
      changed = true;
    }
    ImGui::EndDragDropTarget();
  }
  ImGui::PopStyleColor(3);

  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.2f, 0.2f, 0.2f, 1.0f});
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.3f, 0.3f, 0.3f, 1.0f});
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.2f, 0.2f, 0.2f, 1.0f});
  if (ImGui::Button("x", x_button_size)) {
    texture = nullptr;
    changed = true;
  }
  ImGui::PopStyleColor(3);
  ImGui::PopStyleVar();

  end_property_grid();
  return changed;
}

void ui::image(const Texture& texture, ImVec2 size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& tint_col, const ImVec4& border_col) {
  ImGui::Image(App::get()->get_imgui_layer()->add_image(*texture.get_view()), size, uv0, uv1, tint_col, border_col);
}

void ui::image(const vuk::Value<vuk::ImageAttachment>& attch,
               ImVec2 size,
               const ImVec2& uv0,
               const ImVec2& uv1,
               const ImVec4& tint_col,
               const ImVec4& border_col) {
  ImGui::Image(App::get()->get_imgui_layer()->add_attachment(attch), size, uv0, uv1, tint_col, border_col);
}

void ui::image(const vuk::ImageView& view,
               const ImVec2 size,
               const ImVec2& uv0,
               const ImVec2& uv1,
               const ImVec4& tint_col,
               const ImVec4& border_col) {
  ImGui::Image(App::get()->get_imgui_layer()->add_image(view), size, uv0, uv1, tint_col, border_col);
}

bool ui::image_button(const char* id,
                      const vuk::ImageView& view,
                      const ImVec2 size,
                      const ImVec2& uv0,
                      const ImVec2& uv1,
                      const ImVec4& tint_col,
                      const ImVec4& bg_col) {
  return ImGui::ImageButton(id, App::get()->get_imgui_layer()->add_image(view), size, uv0, uv1, bg_col, tint_col);
}

bool ui::draw_vec2_control(const char* label, glm::vec2& values, const char* tooltip, const float reset_value) {
  bool changed = false;

  begin_property_grid(label, tooltip);

  push_frame_style(false);

  const float x = ImGui::GetContentRegionAvail().x;
  ImGui::PushMultiItemsWidths(2, x);

  const float frame_height = ImGui::GetFrameHeight();
  const ImVec2 button_size = {2.f, frame_height};

  // X
  {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{0, 0});
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{1.0f, 1.0f, 1.0f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.8f, 0.1f, 0.15f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.9f, 0.2f, 0.2f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.8f, 0.1f, 0.15f, 1.0f});
    if (ImGui::Button("##", button_size)) {
      values.x = reset_value;
    }
    ImGui::PopStyleColor(4);

    ImGui::SameLine();
    if (ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f")) {
      changed = true;
    }
    ImGui::PopItemWidth();
    ImGui::PopStyleVar();
  }

  ImGui::SameLine();

  // Y
  {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{0, 0});
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{1.0f, 1.0f, 1.0f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.2f, 0.7f, 0.2f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.3f, 0.8f, 0.3f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.2f, 0.7f, 0.2f, 1.0f});
    if (ImGui::Button("##", button_size)) {
      values.y = reset_value;
    }
    ImGui::PopStyleColor(4);

    ImGui::SameLine();
    if (ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f")) {
      changed = true;
    }
    ImGui::PopItemWidth();
    ImGui::PopStyleVar();
  }

  end_property_grid();

  pop_frame_style();

  return changed;
}

bool ui::draw_vec3_control(const char* label, glm::vec3& values, const char* tooltip, const float reset_value) {
  bool changed = false;

  begin_property_grid(label, tooltip);

  push_frame_style(false);

  ImGui::PushMultiItemsWidths(3, ImGui::GetWindowWidth() - 150.0f);

  const float frame_height = ImGui::GetFrameHeight();
  const ImVec2 button_size = {2.f, frame_height};

  // X
  {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{0, 0});
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{1.0f, 1.0f, 1.0f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.8f, 0.1f, 0.15f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.9f, 0.2f, 0.2f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.8f, 0.1f, 0.15f, 1.0f});
    if (ImGui::Button("##", button_size)) {
      values.x = reset_value;
    }
    ImGui::PopStyleColor(4);

    ImGui::SameLine();
    if (ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f")) {
      changed = true;
    }
    ImGui::PopItemWidth();
    ImGui::PopStyleVar();
  }

  ImGui::SameLine();

  // Y
  {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{0, 0});
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{1.0f, 1.0f, 1.0f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.2f, 0.7f, 0.2f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.3f, 0.8f, 0.3f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.2f, 0.7f, 0.2f, 1.0f});
    if (ImGui::Button("##", button_size)) {
      values.y = reset_value;
    }
    ImGui::PopStyleColor(4);

    ImGui::SameLine();
    if (ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f")) {
      changed = true;
    }
    ImGui::PopItemWidth();
    ImGui::PopStyleVar();
  }

  ImGui::SameLine();

  // Z
  {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{0, 0});
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{1.0f, 1.0f, 1.0f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.1f, 0.25f, 0.8f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.2f, 0.35f, 0.9f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.1f, 0.25f, 0.8f, 1.0f});
    if (ImGui::Button("##", button_size)) {
      values.z = reset_value;
    }
    ImGui::PopStyleColor(4);

    ImGui::SameLine();
    if (ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f")) {
      changed = true;
    }
    ImGui::PopItemWidth();
    ImGui::PopStyleVar();
  }

  end_property_grid();

  pop_frame_style();

  return changed;
}

bool ui::toggle_button(const char* label,
                       const bool state,
                       const ImVec2 size,
                       const float alpha,
                       const float pressed_alpha,
                       const ImGuiButtonFlags button_flags) {
  if (state) {
    ImVec4 color = ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];

    color.w = pressed_alpha;
    ImGui::PushStyleColor(ImGuiCol_Button, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);
  } else {
    ImVec4 color = ImGui::GetStyle().Colors[ImGuiCol_Button];
    ImVec4 hovered_color = ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered];
    color.w = alpha;
    hovered_color.w = pressed_alpha;
    ImGui::PushStyleColor(ImGuiCol_Button, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hovered_color);
    color.w = pressed_alpha;
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);
  }

  const bool clicked = ImGui::ButtonEx(label, size, button_flags);

  ImGui::PopStyleColor(3);

  return clicked;
}

ImVec2 ui::get_icon_button_size(const char8_t* icon, const char* label) {
  const float line_height = ImGui::GetTextLineHeight();
  const ImVec2 padding = ImGui::GetStyle().FramePadding;

  float width = ImGui::CalcTextSize(StringUtils::from_char8_t(icon)).x;
  width += ImGui::CalcTextSize(label).x;
  width += padding.x * 2.0f;

  return {width, line_height + padding.y * 2.0f};
}

bool ui::icon_button(const char8_t* icon, const char* label, const ImVec4 icon_color) {
  ImGui::PushID(label);

  const float line_height = ImGui::GetTextLineHeight();
  const ImVec2 padding = ImGui::GetStyle().FramePadding;

  float width = ImGui::CalcTextSize(StringUtils::from_char8_t(icon)).x;
  width += ImGui::CalcTextSize(label).x;
  width += padding.x * 2.0f;
  float height = line_height + padding.y * 2.0f;

  const float cursor_pos_x = ImGui::GetCursorPosX();
  const bool clicked = ImGui::Button("##", {width, height});
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0, 0});
  ImGui::SameLine();
  ImGui::SetCursorPosX(cursor_pos_x);
  ImGui::TextColored(icon_color, "%s", StringUtils::from_char8_t(icon));
  ImGui::SameLine();
  ImGui::TextUnformatted(label);
  ImGui::PopStyleVar();
  ImGui::PopID();

  return clicked;
}

void ui::clipped_text(const ImVec2& pos_min,
                      const ImVec2& pos_max,
                      const char* text,
                      const char* text_end,
                      const ImVec2* text_size_if_known,
                      const ImVec2& align,
                      const ImRect* clip_rect,
                      const float wrap_width) {
  // Hide anything after a '##' string
  const char* text_display_end = ImGui::FindRenderedTextEnd(text, text_end);
  const int text_len = static_cast<int>(text_display_end - text);
  if (text_len == 0)
    return;

  const ImGuiContext& g = *GImGui;
  const ImGuiWindow* window = g.CurrentWindow;
  clipped_text(window->DrawList, pos_min, pos_max, text, text_display_end, text_size_if_known, align, clip_rect, wrap_width);
  if (g.LogEnabled)
    ImGui::LogRenderedText(&pos_min, text, text_display_end);
}

void ui::clipped_text(ImDrawList* draw_list,
                      const ImVec2& pos_min,
                      const ImVec2& pos_max,
                      const char* text,
                      const char* text_display_end,
                      const ImVec2* text_size_if_known,
                      const ImVec2& align,
                      const ImRect* clip_rect,
                      const float wrap_width) {
  // Perform CPU side clipping for single clipped element to avoid ui::using scissor state
  ImVec2 pos = pos_min;
  const ImVec2 text_size = text_size_if_known ? *text_size_if_known : ImGui::CalcTextSize(text, text_display_end, false, wrap_width);

  const ImVec2* clip_min = clip_rect ? &clip_rect->Min : &pos_min;
  const ImVec2* clip_max = clip_rect ? &clip_rect->Max : &pos_max;

  // Align whole block. We should defer that to the better rendering function when we'll have support for individual line alignment.
  if (align.x > 0.0f)
    pos.x = ImMax(pos.x, pos.x + (pos_max.x - pos.x - text_size.x) * align.x);

  if (align.y > 0.0f)
    pos.y = ImMax(pos.y, pos.y + (pos_max.y - pos.y - text_size.y) * align.y);

  // Render
  const ImVec4 fine_clip_rect(clip_min->x, clip_min->y, clip_max->x, clip_max->y);
  draw_list->AddText(nullptr, 0.0f, pos, ImGui::GetColorU32(ImGuiCol_Text), text, text_display_end, wrap_width, &fine_clip_rect);
}

void ui::spacing(const uint32_t count) {
  for (uint32_t i = 0; i < count; i++)
    ImGui::Spacing();
}
void ui::align_right(float item_width) {
  const auto posX = ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - item_width - ImGui::GetScrollX();
  if (posX > ImGui::GetCursorPosX())
    ImGui::SetCursorPosX(posX);
}

std::string ui::get_path_from_imgui_payload(const ImGuiPayload* payload) { return std::string(static_cast<const char*>(payload->Data)); }

void ui::draw_gradient_shadow_bottom(const float scale) {
  const auto draw_list = ImGui::GetWindowDrawList();
  const auto pos = ImGui::GetWindowPos();
  const auto window_height = ImGui::GetWindowHeight();
  const auto window_width = ImGui::GetWindowWidth();

  const ImRect bb(0, scale, pos.x + window_width, window_height + pos.y);
  draw_list
    ->AddRectFilledMultiColor(bb.Min, bb.Max, IM_COL32(20, 20, 20, 0), IM_COL32(20, 20, 20, 0), IM_COL32(20, 20, 20, 255), IM_COL32(20, 20, 20, 255));
}

void ui::center_next_window() {
  const auto center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
}

void ui::draw_framerate_overlay(const ImVec2 work_pos, const ImVec2 work_size, const ImVec2 padding, bool* visible) {
  static int corner = 1;
  ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize |
                                  ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
  if (corner != -1) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 window_pos, window_pos_pivot;
    window_pos.x = corner & 1 ? work_pos.x + work_size.x - padding.x : work_pos.x + padding.x;
    window_pos.y = corner & 2 ? work_pos.y + work_size.y - padding.y : work_pos.y + padding.y;
    window_pos_pivot.x = corner & 1 ? 1.0f : 0.0f;
    window_pos_pivot.y = corner & 2 ? 1.0f : 0.0f;
    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
    ImGui::SetNextWindowViewport(viewport->ID);
    window_flags |= ImGuiWindowFlags_NoMove;
  }
  ImGui::SetNextWindowBgAlpha(0.35f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 3.0f);
  if (ImGui::Begin("Performance Overlay", nullptr, window_flags)) {
    ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
  }
  if (ImGui::BeginPopupContextWindow()) {
    if (ImGui::MenuItem("Custom", nullptr, corner == -1))
      corner = -1;
    if (ImGui::MenuItem("Top-left", nullptr, corner == 0))
      corner = 0;
    if (ImGui::MenuItem("Top-right", nullptr, corner == 1))
      corner = 1;
    if (ImGui::MenuItem("Bottom-left", nullptr, corner == 2))
      corner = 2;
    if (ImGui::MenuItem("Bottom-right", nullptr, corner == 3))
      corner = 3;
    if (visible && *visible && ImGui::MenuItem("Close"))
      *visible = false;
    ImGui::EndPopup();
  }
  ImGui::End();
  ImGui::PopStyleVar();
}
} // namespace ox
