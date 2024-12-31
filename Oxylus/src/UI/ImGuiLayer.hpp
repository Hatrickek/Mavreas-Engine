#pragma once

#include <plf_colony.h>
#include <vuk/Value.hpp>

#include <imgui.h>

#include "Assets/Texture.hpp"
#include "Core/Layer.hpp"

namespace ox {
class ImGuiLayer : public Layer {
public:
  struct ImGuiImage {
    bool global;
    vuk::ImageView view;
    uint32_t attachment_index;
    bool linear_sampling = true;
  };

  struct ImGuiData {
    Shared<Texture> font_texture = nullptr;
    ImGuiImage font_image = {};
  };

  static ImFont* bold_font;
  static ImFont* regular_font;
  static ImFont* small_font;

  inline static ImVec4 header_selected_color;
  inline static ImVec4 header_hovered_color;
  inline static ImVec4 window_bg_color;
  inline static ImVec4 window_bg_alternative_color;
  inline static ImVec4 asset_icon_color;
  inline static ImVec4 text_color;
  inline static ImVec4 text_disabled_color;

  inline static ImVec2 ui_frame_padding;
  inline static ImVec2 popup_item_spacing;

  ImGuiLayer();
  ~ImGuiLayer() override = default;

  void on_attach(EventDispatcher& dispatcher) override;
  void on_detach() override;

  void begin();
  void end();

  [[nodiscard]] vuk::Value<vuk::ImageAttachment> render_draw_data(vuk::Allocator& allocator,
                                                                  vuk::Compiler& compiler,
                                                                  vuk::Value<vuk::ImageAttachment> target) const;

  ImGuiImage* add_image(const vuk::ImageView& view, bool linear_sampling = true);
  ImGuiImage* add_attachment(const vuk::Value<vuk::ImageAttachment>& attach, bool linear_sampling = true);

  static void apply_theme(bool dark = true);
  static void set_style();

  plf::colony<ImGuiImage> sampled_images;
  std::vector<vuk::Value<vuk::ImageAttachment>> sampled_attachments;

private:
  ImGuiData imgui_data;
  ImDrawData* draw_data = nullptr;

  void init_for_vulkan();
  void add_icon_font(float font_size);
  void imgui_impl_vuk_init(vuk::Allocator& allocator);
};
} // namespace ox
