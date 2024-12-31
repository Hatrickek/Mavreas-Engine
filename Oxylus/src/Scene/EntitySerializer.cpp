#include "EntitySerializer.hpp"

#include <filesystem>
#include <fstream>

#include "Core/Base.hpp"
#include "Core/FileSystem.hpp"
#include "Core/Project.hpp"
#include "Core/Systems/SystemManager.hpp"
#include "Scene.hpp"
#include "Scene/Components.hpp"
#include "SceneRenderer.hpp"

#include "Assets/AssetManager.hpp"

#include "Core/App.hpp"
#include "Core/FileSystem.hpp"

#include "Entity.hpp"
#include "Utils/Archive.hpp"

#include "Utils/Log.hpp"
#include "Utils/Toml.hpp"

namespace ox {
#define GET_STRING(node, component, name) component.name = node->as_table()->get(#name)->as_string()->get()
#define GET_STRING2(node, name) node->as_table()->get(name)->as_string()->get()
#define GET_FLOAT(node, component, name) component.name = (float)node->as_table()->get(#name)->as_floating_point()->get()
#define GET_FLOAT2(node, name) (float)node->as_table()->get(name)->as_floating_point()->get()
#define GET_UINT32(node, component, name) component.name = (uint32_t)node->as_table()->get(#name)->as_integer()->get()
#define GET_UINT322(node, name) (uint32_t) node->as_table()->get(name)->as_integer()->get()
#define GET_BOOL(node, component, name) component.name = node->as_table()->get(#name)->as_boolean()->get()
#define GET_BOOL2(node, name) node->as_table()->get(name)->as_boolean()->get()
#define GET_ARRAY(node, name) node->as_table()->get(name)->as_array()

#define TBL_FIELD(c, field) {#field, c.field}
#define TBL_FIELD_ARR(c, field) {#field, get_toml_array(c.field)}

void EntitySerializer::serialize_entity(toml::array* entities, Scene* scene, Entity entity) {
  entities->push_back(toml::table{{"uuid", std::to_string((uint64_t)eutil::get_uuid(scene->registry, entity))}});

  if (scene->registry.all_of<TagComponent>(entity)) {
    const auto& tag = scene->registry.get<TagComponent>(entity);

    const auto table = toml::table{
      TBL_FIELD(tag, tag),
      TBL_FIELD(tag, enabled),
    };

    entities->push_back(toml::table{{"tag_component", table}});
  }

  if (scene->registry.all_of<RelationshipComponent>(entity)) {
    const auto& [parent, children] = scene->registry.get<RelationshipComponent>(entity);

    toml::array children_array = {};
    for (auto child : children)
      children_array.push_back(std::to_string((uint64_t)child));

    const auto table = toml::table{
      {"parent", std::to_string((uint64_t)parent)},
      {"children", children_array},
    };

    entities->push_back(toml::table{{"relationship_component", table}});
  }

  if (scene->registry.all_of<TransformComponent>(entity)) {
    const auto& tc = scene->registry.get<TransformComponent>(entity);

    const auto table = toml::table{
      TBL_FIELD_ARR(tc, position),
      TBL_FIELD_ARR(tc, rotation),
      TBL_FIELD_ARR(tc, scale),
    };

    entities->push_back(toml::table{{"transform_component", table}});
  }

  if (scene->registry.all_of<MeshComponent>(entity)) {
    const auto& mrc = scene->registry.get<MeshComponent>(entity);

    const auto table = toml::table{{"mesh_path", mrc.mesh_base->get_path()}, TBL_FIELD(mrc, stationary), TBL_FIELD(mrc, cast_shadows)};

    entities->push_back(toml::table{{"mesh_component", table}});
  }

  if (scene->registry.all_of<LightComponent>(entity)) {
    const auto& light = scene->registry.get<LightComponent>(entity);

    const auto table = toml::table{
      {"type", (int)light.type},
      TBL_FIELD(light, color_temperature_mode),
      TBL_FIELD(light, temperature),
      TBL_FIELD_ARR(light, color),
      TBL_FIELD(light, intensity),
      TBL_FIELD(light, range),
      TBL_FIELD(light, radius),
      TBL_FIELD(light, length),
      TBL_FIELD(light, outer_cone_angle),
      TBL_FIELD(light, inner_cone_angle),
      TBL_FIELD(light, cast_shadows),
      TBL_FIELD(light, shadow_map_res),
    };

    entities->push_back(toml::table{{"light_component", table}});
  }

  if (scene->registry.all_of<PostProcessProbe>(entity)) {
    const auto& probe = scene->registry.get<PostProcessProbe>(entity);

    const auto table = toml::table{
      TBL_FIELD(probe, vignette_enabled),
      TBL_FIELD(probe, vignette_intensity),
      TBL_FIELD(probe, film_grain_enabled),
      TBL_FIELD(probe, film_grain_intensity),
      TBL_FIELD(probe, chromatic_aberration_enabled),
      TBL_FIELD(probe, chromatic_aberration_intensity),
      TBL_FIELD(probe, sharpen_enabled),
      TBL_FIELD(probe, sharpen_intensity),
    };

    entities->push_back(toml::table{{"post_process_probe", table}});
  }

  if (scene->registry.all_of<CameraComponent>(entity)) {
    const auto& camera = scene->registry.get<CameraComponent>(entity);

    // TODO: serialize the rest
    const auto table = toml::table{
      {"projection", (uint32)camera.camera->get_projection()},
      {"fov", camera.camera->get_fov()},
      {"near", camera.camera->get_near()},
      {"far", camera.camera->get_far()},
      {"zoom", camera.camera->get_zoom()},
    };

    entities->push_back(toml::table{{"camera_component", table}});
  }

  // Physics
  if (scene->registry.all_of<RigidbodyComponent>(entity)) {
    const auto& rb = scene->registry.get<RigidbodyComponent>(entity);

    const auto table = toml::table{
      {"allowed_dofs", (int)rb.allowed_dofs},
      {"type", (int)rb.type},
      TBL_FIELD(rb, mass),
      TBL_FIELD(rb, linear_drag),
      TBL_FIELD(rb, angular_drag),
      TBL_FIELD(rb, gravity_scale),
      TBL_FIELD(rb, allow_sleep),
      TBL_FIELD(rb, awake),
      TBL_FIELD(rb, continuous),
      TBL_FIELD(rb, interpolation),
      TBL_FIELD(rb, is_sensor),
    };

    entities->push_back(toml::table{{"rigidbody_component", table}});
  }

  if (scene->registry.all_of<BoxColliderComponent>(entity)) {
    const auto& bc = scene->registry.get<BoxColliderComponent>(entity);

    const auto table = toml::table{
      TBL_FIELD_ARR(bc, size),
      TBL_FIELD_ARR(bc, offset),
      TBL_FIELD(bc, density),
      TBL_FIELD(bc, friction),
      TBL_FIELD(bc, restitution),
    };

    entities->push_back(toml::table{{"box_collider_component", table}});
  }

  if (scene->registry.all_of<SphereColliderComponent>(entity)) {
    const auto& sc = scene->registry.get<SphereColliderComponent>(entity);

    const auto table = toml::table{
      TBL_FIELD(sc, radius),
      TBL_FIELD_ARR(sc, offset),
      TBL_FIELD(sc, density),
      TBL_FIELD(sc, friction),
      TBL_FIELD(sc, restitution),
    };

    entities->push_back(toml::table{{"sphere_collider_component", table}});
  }

  if (scene->registry.all_of<CapsuleColliderComponent>(entity)) {
    const auto& cc = scene->registry.get<CapsuleColliderComponent>(entity);
    const auto table = toml::table{
      TBL_FIELD(cc, height),
      TBL_FIELD(cc, radius),
      TBL_FIELD_ARR(cc, offset),
      TBL_FIELD(cc, density),
      TBL_FIELD(cc, friction),
      TBL_FIELD(cc, restitution),
    };

    entities->push_back(toml::table{{"capsule_collider_component", table}});
  }

  if (scene->registry.all_of<TaperedCapsuleColliderComponent>(entity)) {
    const auto& tcc = scene->registry.get<TaperedCapsuleColliderComponent>(entity);

    const auto table = toml::table{
      TBL_FIELD(tcc, height),
      TBL_FIELD(tcc, top_radius),
      TBL_FIELD_ARR(tcc, offset),
      TBL_FIELD(tcc, density),
      TBL_FIELD(tcc, friction),
      TBL_FIELD(tcc, restitution),
    };

    entities->push_back(toml::table{{"tapered_capsule_collider_component", table}});
  }

  if (scene->registry.all_of<CylinderColliderComponent>(entity)) {
    const auto& cc = scene->registry.get<CylinderColliderComponent>(entity);
    const auto table = toml::table{
      TBL_FIELD(cc, height),
      TBL_FIELD(cc, radius),
      TBL_FIELD_ARR(cc, offset),
      TBL_FIELD(cc, density),
      TBL_FIELD(cc, friction),
      TBL_FIELD(cc, restitution),
    };

    entities->push_back(toml::table{{"cylinder_collider_component", table}});
  }

  if (scene->registry.all_of<MeshColliderComponent>(entity)) {
    const auto& mc = scene->registry.get<MeshColliderComponent>(entity);
    const auto table = toml::table{
      TBL_FIELD_ARR(mc, offset),
      TBL_FIELD(mc, friction),
      TBL_FIELD(mc, restitution),
    };

    entities->push_back(toml::table{{"mesh_collider_component", table}});
  }

  if (scene->registry.all_of<CharacterControllerComponent>(entity)) {
    const auto& component = scene->registry.get<CharacterControllerComponent>(entity);
    const auto table = toml::table{
      TBL_FIELD(component, character_height_standing),
      TBL_FIELD(component, character_radius_standing),
      TBL_FIELD(component, character_radius_crouching),
      TBL_FIELD(component, character_height_crouching),
      TBL_FIELD(component, control_movement_during_jump),
      TBL_FIELD(component, jump_force),
      TBL_FIELD(component, friction),
      TBL_FIELD(component, collision_tolerance),
    };

    entities->push_back(toml::table{{"character_controller_component", table}});
  }

  if (scene->registry.all_of<LuaScriptComponent>(entity)) {
    const auto& component = scene->registry.get<LuaScriptComponent>(entity);
    const auto& systems = component.lua_systems;

    toml::array path_array = {};
    for (const auto& system : systems)
      path_array.push_back(system->get_path());

    const auto table = toml::table{{"paths", path_array}};

    entities->push_back(toml::table{{"lua_script_component", table}});
  }

  if (scene->registry.all_of<CPPScriptComponent>(entity)) {
    const auto& component = scene->registry.get<CPPScriptComponent>(entity);
    toml::array hash_array = {};
    for (const auto& system : component.systems) {
      hash_array.push_back(std::to_string(system->hash_code));
    }
    const auto table = toml::table{{"system_hashes", hash_array}};
    entities->push_back(toml::table{{"cpp_script_component", table}});
  }

  if (scene->registry.all_of<SpriteComponent>(entity)) {
    const auto& component = scene->registry.get<SpriteComponent>(entity);
    const auto path = component.material->get_albedo_texture() ? component.material->get_albedo_texture()->get_path() : "";
    const auto table = toml::table{
      TBL_FIELD(component, layer),
      TBL_FIELD(component, sort_y),
      {"color", get_toml_array(component.material->parameters.color)},
      {"uv_size", get_toml_array(component.material->parameters.uv_size)},
      {"uv_offset", get_toml_array(component.material->parameters.uv_offset)},
      {"texture_path", path},
    };
    entities->push_back(toml::table{{"sprite_component", table}});
  }

  if (scene->registry.all_of<SpriteAnimationComponent>(entity)) {
    const auto& component = scene->registry.get<SpriteAnimationComponent>(entity);
    const auto table = toml::table{
      TBL_FIELD(component, num_frames),
      TBL_FIELD(component, loop),
      TBL_FIELD(component, inverted),
      TBL_FIELD(component, fps),
      TBL_FIELD(component, columns),
      {"frame_size", get_toml_array(component.frame_size)},
    };
    entities->push_back(toml::table{{"sprite_animation_component", table}});
  }

  if (scene->registry.all_of<TilemapComponent>(entity)) {
    const auto& component = scene->registry.get<TilemapComponent>(entity);
    const auto table = toml::table{
      {"path", component.path},
    };
    entities->push_back(toml::table{{"tilemap_component", table}});
  }
}

void EntitySerializer::serialize_entity_binary(Archive& archive, Scene* scene, Entity entity) {
  if (scene->registry.all_of<IDComponent>(entity)) {
    const auto& component = scene->registry.get<IDComponent>(entity);
    archive << component.uuid;
  }
  if (scene->registry.all_of<TagComponent>(entity)) {
    const auto& component = scene->registry.get<TagComponent>(entity);
    archive << component.tag;
  }
}

UUID EntitySerializer::deserialize_entity(toml::array* entity_arr, Scene* scene, bool preserve_uuid) {
  // these values are always present
  const uint64_t uuid = std::stoull(entity_arr->get(0)->as_table()->get("uuid")->as_string()->get());
  const auto tag_node = entity_arr->get(1)->as_table()->get("tag_component")->as_table();
  std::string name = tag_node->get("tag")->as_string()->get();

  entt::entity deserialized_entity = {};
  if (preserve_uuid)
    deserialized_entity = scene->create_entity_with_uuid(uuid, name);
  else
    deserialized_entity = scene->create_entity(name);

  auto& reg = scene->registry;

  auto& tag_component = reg.get_or_emplace<TagComponent>(deserialized_entity);
  tag_component.tag = name;
  tag_component.enabled = tag_node->get("enabled")->as_boolean()->get();

  for (auto& ent : *entity_arr) {
    if (const auto relation_node = ent.as_table()->get("relationship_component")) {
      auto& rc = reg.get_or_emplace<RelationshipComponent>(deserialized_entity);
      rc.parent = std::stoull(GET_STRING2(relation_node, "parent"));
      const auto children_node = relation_node->as_table()->get("children")->as_array();
      for (auto& child : *children_node)
        rc.children.emplace_back(std::stoull(child.as_string()->get()));
    } else if (const auto transform_node = ent.as_table()->get("transform_component")) {
      auto& tc = reg.get_or_emplace<TransformComponent>(deserialized_entity);
      tc.position = get_vec3_toml_array(GET_ARRAY(transform_node, "position"));
      tc.rotation = get_vec3_toml_array(GET_ARRAY(transform_node, "rotation"));
      tc.scale = get_vec3_toml_array(GET_ARRAY(transform_node, "scale"));
    } else if (const auto mesh_node = ent.as_table()->get("mesh_component")) {
      const auto path = GET_STRING2(mesh_node, "mesh_path");
      auto mesh = AssetManager::get_mesh_asset(path);
      auto& mc = reg.emplace<MeshComponent>(deserialized_entity, mesh);
      GET_BOOL(mesh_node, mc, cast_shadows);
      GET_BOOL(mesh_node, mc, stationary);
    } else if (const auto light_node = ent.as_table()->get("light_component")) {
      auto& lc = reg.emplace<LightComponent>(deserialized_entity);
      lc.type = (LightComponent::LightType)GET_UINT322(light_node, "type");
      GET_BOOL(light_node, lc, color_temperature_mode);
      GET_UINT32(light_node, lc, temperature);
      lc.color = get_vec3_toml_array(GET_ARRAY(light_node, "color"));
      GET_FLOAT(light_node, lc, intensity);
      GET_FLOAT(light_node, lc, range);
      GET_FLOAT(light_node, lc, radius);
      GET_FLOAT(light_node, lc, length);
      GET_FLOAT(light_node, lc, outer_cone_angle);
      GET_FLOAT(light_node, lc, inner_cone_angle);
      GET_BOOL(light_node, lc, cast_shadows);
      GET_UINT32(light_node, lc, shadow_map_res);
    } else if (const auto pp_node = ent.as_table()->get("post_process_probe")) {
      auto& pp = reg.emplace<PostProcessProbe>(deserialized_entity);
      GET_BOOL(pp_node, pp, vignette_enabled);
      GET_FLOAT(pp_node, pp, vignette_intensity);
      GET_BOOL(pp_node, pp, film_grain_enabled);
      GET_FLOAT(pp_node, pp, film_grain_intensity);
      GET_BOOL(pp_node, pp, chromatic_aberration_enabled);
      GET_FLOAT(pp_node, pp, chromatic_aberration_intensity);
      GET_BOOL(pp_node, pp, sharpen_enabled);
      GET_FLOAT(pp_node, pp, sharpen_intensity);
    } else if (const auto camera_node = ent.as_table()->get("camera_component")) {
      auto& cc = reg.emplace<CameraComponent>(deserialized_entity);
      cc.camera->set_projection((Camera::Projection)GET_UINT322(camera_node, "projection"));
      cc.camera->set_fov(GET_FLOAT2(camera_node, "fov"));
      cc.camera->set_near(GET_FLOAT2(camera_node, "near"));
      cc.camera->set_far(GET_FLOAT2(camera_node, "far"));
      cc.camera->set_zoom(GET_FLOAT2(camera_node, "zoom"));
    } else if (const auto rb_node = ent.as_table()->get("rigidbody_component")) {
      auto& rb = reg.emplace<RigidbodyComponent>(deserialized_entity);
      rb.allowed_dofs = (RigidbodyComponent::AllowedDOFs)GET_UINT322(rb_node, "allowed_dofs");
      rb.type = (RigidbodyComponent::BodyType)GET_UINT322(rb_node, "type");
      GET_FLOAT(rb_node, rb, mass);
      GET_FLOAT(rb_node, rb, linear_drag);
      GET_FLOAT(rb_node, rb, angular_drag);
      GET_FLOAT(rb_node, rb, gravity_scale);
      GET_BOOL(rb_node, rb, allow_sleep);
      GET_BOOL(rb_node, rb, awake);
      GET_BOOL(rb_node, rb, continuous);
      GET_BOOL(rb_node, rb, interpolation);
      GET_BOOL(rb_node, rb, is_sensor);
    } else if (const auto bc_node = ent.as_table()->get("box_collider_component")) {
      auto& bc = reg.emplace<BoxColliderComponent>(deserialized_entity);
      bc.size = get_vec3_toml_array(GET_ARRAY(bc_node, "size"));
      bc.offset = get_vec3_toml_array(GET_ARRAY(bc_node, "offset"));
      GET_FLOAT(bc_node, bc, density);
      GET_FLOAT(bc_node, bc, friction);
      GET_FLOAT(bc_node, bc, restitution);
    } else if (const auto sc_node = ent.as_table()->get("sphere_collider_component")) {
      auto& sc = reg.emplace<SphereColliderComponent>(deserialized_entity);
      GET_FLOAT(sc_node, sc, radius);
      sc.offset = get_vec3_toml_array(GET_ARRAY(sc_node, "offset"));
      GET_FLOAT(sc_node, sc, density);
      GET_FLOAT(sc_node, sc, friction);
      GET_FLOAT(sc_node, sc, restitution);
    } else if (const auto cc_node = ent.as_table()->get("capsule_collider_component")) {
      auto& cc = reg.emplace<CapsuleColliderComponent>(deserialized_entity);
      GET_FLOAT(cc_node, cc, height);
      GET_FLOAT(cc_node, cc, radius);
      cc.offset = get_vec3_toml_array(GET_ARRAY(cc_node, "offset"));
      GET_FLOAT(cc_node, cc, density);
      GET_FLOAT(cc_node, cc, friction);
      GET_FLOAT(cc_node, cc, restitution);
    } else if (const auto tcc_node = ent.as_table()->get("tapered_capsule_collider_component")) {
      auto& tcc = reg.emplace<TaperedCapsuleColliderComponent>(deserialized_entity);
      GET_FLOAT(tcc_node, tcc, height);
      GET_FLOAT(tcc_node, tcc, top_radius);
      GET_FLOAT(tcc_node, tcc, bottom_radius);
      tcc.offset = get_vec3_toml_array(GET_ARRAY(tcc_node, "offset"));
      GET_FLOAT(tcc_node, tcc, density);
      GET_FLOAT(tcc_node, tcc, friction);
      GET_FLOAT(tcc_node, tcc, restitution);
    } else if (const auto ccc_node = ent.as_table()->get("cylinder_collider_component")) {
      auto& ccc = reg.emplace<CylinderColliderComponent>(deserialized_entity);
      GET_FLOAT(ccc_node, ccc, height);
      GET_FLOAT(ccc_node, ccc, radius);
      ccc.offset = get_vec3_toml_array(GET_ARRAY(ccc_node, "offset"));
      GET_FLOAT(ccc_node, ccc, density);
      GET_FLOAT(ccc_node, ccc, friction);
      GET_FLOAT(ccc_node, ccc, restitution);
    } else if (const auto mc_node = ent.as_table()->get("mesh_collider_component")) {
      auto& mc = reg.emplace<MeshColliderComponent>(deserialized_entity);
      mc.offset = get_vec3_toml_array(GET_ARRAY(mc_node, "offset"));
      GET_FLOAT(mc_node, mc, friction);
      GET_FLOAT(mc_node, mc, restitution);
    } else if (const auto chc_node = ent.as_table()->get("character_controller_component")) {
      auto& chc = reg.emplace<CharacterControllerComponent>(deserialized_entity);
      GET_FLOAT(chc_node, chc, character_height_standing);
      GET_FLOAT(chc_node, chc, character_radius_standing);
      GET_FLOAT(chc_node, chc, character_height_crouching);
      GET_FLOAT(chc_node, chc, character_radius_crouching);
      GET_BOOL(chc_node, chc, control_movement_during_jump);
      GET_FLOAT(chc_node, chc, jump_force);
      GET_FLOAT(chc_node, chc, friction);
      GET_FLOAT(chc_node, chc, collision_tolerance);
    } else if (const auto lua_node = ent.as_table()->get("lua_script_component")) {
      auto& lsc = reg.emplace<LuaScriptComponent>(deserialized_entity);
      auto paths = GET_ARRAY(lua_node, "paths");
      for (auto& path : *paths) {
        auto ab = App::get_system<VFS>()->resolve_physical_dir(path.as_string()->get());
        lsc.lua_systems.emplace_back(create_shared<LuaSystem>(ab));
      }
    } else if (const auto cpp_node = ent.as_table()->get("cpp_script_component")) {
      auto& csc = reg.emplace<CPPScriptComponent>(deserialized_entity);
      auto system_hashes = GET_ARRAY(cpp_node, "system_hashes");
      for (auto& hash : *system_hashes) {
        auto* system_manager = App::get_system<SystemManager>();
        csc.systems.emplace_back(system_manager->get_system(std::stoull(hash.as_string()->get())));
      }
    } else if (const auto sprite_node = ent.as_table()->get("sprite_component")) {
      auto& sc = reg.emplace<SpriteComponent>(deserialized_entity);
      GET_UINT32(sprite_node, sc, layer);
      GET_BOOL(sprite_node, sc, sort_y);
      sc.material = create_shared<SpriteMaterial>();
      sc.material->parameters.color = get_vec4_toml_array(GET_ARRAY(sprite_node, "color"));
      sc.material->parameters.uv_offset = get_vec2_toml_array(GET_ARRAY(sprite_node, "uv_offset"));
      sc.material->parameters.uv_size = get_vec2_toml_array(GET_ARRAY(sprite_node, "uv_size"));

      const auto path = GET_STRING2(sprite_node, "texture_path");
      if (!path.empty())
        sc.material->set_albedo_texture(AssetManager::get_texture_asset({.path = path}));
    } else if (const auto sprite_anim_node = ent.as_table()->get("sprite_animation_component")) {
      auto& sac = reg.emplace<SpriteAnimationComponent>(deserialized_entity);
      GET_UINT32(sprite_anim_node, sac, num_frames);
      GET_BOOL(sprite_anim_node, sac, loop);
      GET_BOOL(sprite_anim_node, sac, inverted);
      GET_UINT32(sprite_anim_node, sac, fps);
      GET_UINT32(sprite_anim_node, sac, columns);
      sac.frame_size = get_vec2_toml_array(GET_ARRAY(sprite_anim_node, "frame_size"));
    } else if (const auto tilemap_node = ent.as_table()->get("tilemap_component")) {
      auto& tmc = reg.emplace<TilemapComponent>(deserialized_entity);
      const auto path = App::get_system<VFS>()->resolve_physical_dir(GET_STRING2(tilemap_node, "path"));
      tmc.load(path);
    }
  }
  return eutil::get_uuid(reg, deserialized_entity);
}

void EntitySerializer::serialize_entity_as_prefab(const char* filepath, Entity entity) {
#if 0 // TODO:
  if (scene->registry.all_of<PrefabComponent>(entity)) {
    OX_CORE_ERROR("Entity already has a prefab component!");
    return;
  }

  ryml::Tree tree;

  ryml::NodeRef node_root = tree.rootref();
  node_root |= ryml::MAP;

  node_root["Prefab"] << entity.add_component_internal<PrefabComponent>().id;

  ryml::NodeRef entities_node = node_root["Entities"];
  entities_node |= ryml::SEQ;

  std::vector<Entity> entities;
  entities.push_back(entity);
  Entity::get_all_children(entity, entities);

  for (const auto& child : entities) {
    if (child)
      serialize_entity(entity.get_scene(), entities_node, child);
  }

  std::stringstream ss;
  ss << tree;
  std::ofstream filestream(filepath);
  filestream << ss.str();
#endif
}

Entity EntitySerializer::deserialize_entity_as_prefab(const char* filepath, Scene* scene) {
#if 0 // TODO:
  auto content = FileSystem::read_file(filepath);
  if (content.empty()) {
    OX_CORE_ERROR("Couldn't read prefab file: {0}", filepath);
  }

  const ryml::Tree tree = ryml::parse_in_arena(ryml::to_csubstr(content));

  if (tree.empty()) {
    OX_CORE_ERROR("Couldn't parse the prefab file {0}", FileSystem::get_file_name(filepath));
  }

  const ryml::ConstNodeRef root = tree.rootref();

  if (!root.has_child("Prefab")) {
    OX_CORE_ERROR("Prefab file doesn't contain a prefab{0}", FileSystem::get_file_name(filepath));
    return {};
  }

  const UUID prefab_id = (uint64_t)root["Prefab"].val().data();

  if (!prefab_id) {
    OX_CORE_ERROR("Invalid prefab ID {0}", FileSystem::get_file_name(filepath));
    return {};
  }

  if (root.has_child("Entities")) {
    const ryml::ConstNodeRef entities_node = root["Entities"];

    Entity root_entity = {};
    std::unordered_map<UUID, UUID> old_new_id_map;
    for (const auto& entity : entities_node) {
      uint64_t old_uuid;
      entity["Entity"] >> old_uuid;
      UUID new_uuid = deserialize_entity(entity, scene, false);
      old_new_id_map.emplace(old_uuid, new_uuid);

      if (!root_entity)
        root_entity = scene->get_entity_by_uuid(new_uuid);
    }

    root_entity.add_component_internal<PrefabComponent>().id = prefab_id;

    // Fix parent/children UUIDs
    for (const auto& [_, newId] : old_new_id_map) {
      auto& relationship_component = scene->get_entity_by_uuid(newId).get_relationship();
      UUID parent = relationship_component.parent;
      if (parent)
        relationship_component.parent = old_new_id_map.at(parent);

      auto& children = relationship_component.children;
      for (auto& id : children)
        id = old_new_id_map.at(id);
    }

    return root_entity;
  }

#endif
  OX_LOG_ERROR("There are not entities in the prefab to deserialize! {0}", fs::get_file_name(filepath));
  return {};
}
} // namespace ox
