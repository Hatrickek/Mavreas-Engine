#pragma once

#include "Scene/Entity.hpp"
#include "Scene/Scene.hpp"
#include "EditorPanel.hpp"
#include "imgui_internal.h"

namespace ox {
class SceneHierarchyPanel : public EditorPanel {
public:
  SceneHierarchyPanel();
  void on_update() override;
  void on_imgui_render() override;

  ImRect draw_entity_node(Entity entity, uint32_t depth = 0, bool force_expand_tree = false, bool is_part_of_prefab = false);
  void set_context(const Shared<Scene>& scene);

  void clear_selection_context();
  Entity get_selected_entity() const;
  void set_selected_entity(Entity entity);
  const Shared<Scene>& get_scene() const { return context; }

  void drag_drop_target() const;

private:
  Shared<Scene> context = nullptr;
  ImGuiTextFilter m_filter;
  bool m_table_hovered = false;
  bool m_window_hovered = false;
  std::vector<Entity> selected_entities = {};
  Entity renaming_entity = entt::null;
  Entity dragged_entity = entt::null;
  Entity dragged_entity_target = entt::null;
  Entity deleted_entity = entt::null;

  void draw_context_menu();
  Entity get_selected_entity_front() const;
};
}
