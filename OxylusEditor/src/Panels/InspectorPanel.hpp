#pragma once

#include "Assets/PBRMaterial.hpp"
#include "Assets/SpriteMaterial.hpp"
#include "EditorPanel.hpp"
#include "Scene/Entity.hpp"

namespace ox {
class InspectorPanel : public EditorPanel {
public:
  InspectorPanel();

  void on_imgui_render() override;

  static void draw_pbr_material_properties(Shared<PBRMaterial>& material);
  static void draw_sprite_material_properties(Shared<SpriteMaterial>& material);

private:
  void draw_components(Entity entity);

  template <typename Component>
  static void draw_add_component(entt::registry& reg, Entity entity, const char* name);

  Entity selected_entity = entt::null;
  Scene* context;
  bool debug_mode = false;
};
} // namespace ox
