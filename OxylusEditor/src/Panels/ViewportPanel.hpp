#pragma once

#include <vuk/Value.hpp>

#include "EditorPanel.hpp"

#include "Render/Camera.hpp"
#include "Scene/Scene.hpp"
#include "SceneHierarchyPanel.hpp"

#include "UI/OxUI.hpp"

namespace ox {
class ViewportPanel : public EditorPanel {
public:
  Camera m_camera;
  bool performance_overlay_visible = true;
  bool fullscreen_viewport = false;
  bool is_viewport_focused = {};
  bool is_viewport_hovered = {};

  ViewportPanel();
  ~ViewportPanel() override = default;

  void on_imgui_render() override;

  void set_context(const Shared<Scene>& scene, SceneHierarchyPanel& scene_hierarchy_panel);

  void on_update() override;

private:
  void draw_performance_overlay();
  void draw_gizmos();
  template <typename T>
  void show_component_gizmo(const float width,
                            const float height,
                            const float xpos,
                            const float ypos,
                            const Mat4& view_proj,
                            const Frustum& frustum,
                            Scene* scene) {
    if (gizmo_image_map[typeid(T).hash_code()]) {
      auto view = scene->registry.view<TransformComponent, T>();

      for (const auto&& [entity, transform, component] : view.each()) {
        Vec3 pos = eutil::get_world_transform(scene, entity)[3];

        const auto inside = frustum.is_inside(pos);

        if (inside == (uint32_t)Intersection::Outside)
          continue;

        const Vec2 screen_pos = math::world_to_screen(pos, view_proj, width, height, xpos, ypos);
        ImGui::SetCursorPos({screen_pos.x - ImGui::GetFontSize() * 0.5f, screen_pos.y - ImGui::GetFontSize() * 0.5f});
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.7f, 0.7f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.1f, 0.1f, 0.1f, 0.1f));

        if (ui::image_button("##", *gizmo_image_map[typeid(T).hash_code()]->get_view(), {40.f, 40.f})) {
          m_scene_hierarchy_panel->set_selected_entity(entity);
        }

        ImGui::PopStyleColor(2);

        auto name_s = std::string(entt::type_id<T>().name());

        ui::tooltip_hover(name_s.c_str());
      }
    }
  }

  Shared<Scene> context = {};
  Entity hovered_entity = {};
  SceneHierarchyPanel* m_scene_hierarchy_panel = nullptr;

  Vec2 m_viewport_size = {};
  Vec2 viewport_bounds[2] = {};
  Vec2 viewport_panel_size = {};
  Vec2 viewport_position = {};
  Vec2 viewport_offset = {};
  Vec2 m_gizmo_position = Vec2(1.0f, 1.0f);
  int m_gizmo_type = -1;
  int m_gizmo_mode = 0;

  ankerl::unordered_dense::map<size_t, Shared<Texture>> gizmo_image_map;

  std::vector<vuk::Unique<vuk::Buffer>> id_buffers = {};

  // Camera
  bool m_lock_aspect_ratio = true;
  float m_translation_dampening = 0.6f;
  float m_rotation_dampening = 0.3f;
  bool m_use_editor_camera = true;
  bool m_using_editor_camera = false;
  Vec2 m_locked_mouse_position = Vec2(0.0f);
  Vec3 m_translation_velocity = Vec3(0);
  Vec2 m_rotation_velocity = Vec2(0);
};
} // namespace ox
