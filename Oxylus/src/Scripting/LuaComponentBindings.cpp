#include "LuaComponentBindings.hpp"

#include <sol/state.hpp>

#include "Scene/Components.hpp"
#include "Assets/PBRMaterial.hpp"
#include "LuaHelpers.hpp"

namespace ox {
void LuaBindings::bind_components(const Shared<sol::state>& state) {
  REGISTER_COMPONENT(state, TagComponent, FIELD(TagComponent, tag), FIELD(TagComponent, enabled));
#define TC TransformComponent
  REGISTER_COMPONENT(state, TC, FIELD(TC, position), FIELD(TC, rotation), FIELD(TC, scale));
  bind_mesh_component(state);
  bind_camera_component(state);
}

void LuaBindings::bind_light_component(const Shared<sol::state>& state) {
  REGISTER_COMPONENT(state, LightComponent, FIELD(LightComponent, color), FIELD(LightComponent, intensity)); // TODO: Rest
}

void LuaBindings::bind_mesh_component(const Shared<sol::state>& state) {
  auto material = state->new_usertype<PBRMaterial>("PBRMaterial");
  SET_TYPE_FUNCTION(material, PBRMaterial, set_color);
  SET_TYPE_FUNCTION(material, PBRMaterial, set_emissive);
  SET_TYPE_FUNCTION(material, PBRMaterial, set_roughness);
  SET_TYPE_FUNCTION(material, PBRMaterial, set_metallic);
  SET_TYPE_FUNCTION(material, PBRMaterial, set_reflectance);
  const std::initializer_list<std::pair<sol::string_view, PBRMaterial::AlphaMode>> alpha_mode = {
    {"Opaque", PBRMaterial::AlphaMode::Opaque},
    {"Blend", PBRMaterial::AlphaMode::Blend},
    {"Mask", PBRMaterial::AlphaMode::Mask},
  };
  state->new_enum<PBRMaterial::AlphaMode, true>("AlphaMode", alpha_mode);
  SET_TYPE_FUNCTION(material, PBRMaterial, set_alpha_mode);
  SET_TYPE_FUNCTION(material, PBRMaterial, set_alpha_cutoff);
  SET_TYPE_FUNCTION(material, PBRMaterial, set_double_sided);
  SET_TYPE_FUNCTION(material, PBRMaterial, is_opaque);
  SET_TYPE_FUNCTION(material, PBRMaterial, alpha_mode_to_string);

  material.set_function("new", [](const std::string& name) -> Shared<PBRMaterial> { return create_shared<PBRMaterial>(name); });

#define MC MeshComponent
  REGISTER_COMPONENT(state, MC, FIELD(MC, mesh_base), FIELD(MC, stationary), FIELD(MC, cast_shadows), FIELD(MC, materials), FIELD(MC, aabb));
}

void LuaBindings::bind_camera_component(const Shared<sol::state>& state) {
  auto camera_type = state->new_usertype<Camera>("Camera");
  SET_TYPE_FIELD(camera_type, Camera, set_yaw);
  SET_TYPE_FIELD(camera_type, Camera, set_pitch);
  SET_TYPE_FIELD(camera_type, Camera, get_yaw);
  SET_TYPE_FIELD(camera_type, Camera, get_pitch);
  SET_TYPE_FIELD(camera_type, Camera, set_near);
  SET_TYPE_FIELD(camera_type, Camera, set_far);
  SET_TYPE_FIELD(camera_type, Camera, get_near);
  SET_TYPE_FIELD(camera_type, Camera, get_far);
  SET_TYPE_FIELD(camera_type, Camera, get_fov);
  SET_TYPE_FIELD(camera_type, Camera, set_fov);
  SET_TYPE_FIELD(camera_type, Camera, get_aspect);
  SET_TYPE_FIELD(camera_type, Camera, get_forward);
  SET_TYPE_FIELD(camera_type, Camera, get_right);
  SET_TYPE_FIELD(camera_type, Camera, get_screen_ray);

  REGISTER_COMPONENT(state, CameraComponent, FIELD(CameraComponent, camera));
}
} // namespace ox
