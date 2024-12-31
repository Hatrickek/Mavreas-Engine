#include "Scene.hpp"

#include "Entity.hpp"
#include "Jolt/Physics/Body/AllowedDOFs.h"
#include "Render/Camera.hpp"
#include "Scene/Components.hpp"
#include "Utils/Profiler.hpp"
#include "Utils/Timestep.hpp"

#include <glm/glm.hpp>

#include <ankerl/unordered_dense.h>

#include <glm/gtc/type_ptr.hpp>

#include <sol/state.hpp>

#include "SceneRenderer.hpp"

#include "Core/App.hpp"
#include "Core/FileSystem.hpp"

#include "Jolt/Jolt.h"
#include "Jolt/Physics/Body/BodyCreationSettings.h"

#include "Jolt/Physics/Character/Character.h"
#include "Jolt/Physics/Collision/Shape/BoxShape.h"
#include "Jolt/Physics/Collision/Shape/CapsuleShape.h"
#include "Jolt/Physics/Collision/Shape/CylinderShape.h"
#include "Jolt/Physics/Collision/Shape/MeshShape.h"
#include "Jolt/Physics/Collision/Shape/MutableCompoundShape.h"
#include "Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h"
#include "Jolt/Physics/Collision/Shape/SphereShape.h"
#include "Jolt/Physics/Collision/Shape/TaperedCapsuleShape.h"

#include "Physics/Physics.hpp"
#include "Physics/PhysicsMaterial.hpp"

#include "Render/RenderPipeline.hpp"

#include "Scripting/LuaManager.hpp"

namespace ox {
Scene::Scene() { init(); }

Scene::Scene(std::string name) : scene_name(std::move(name)) { init(); }

Scene::Scene(const Shared<RenderPipeline>& render_pipeline) { init(render_pipeline); }

Scene::~Scene() {
  App::get_system<LuaManager>()->get_state()->collect_gc();
  if (running)
    on_runtime_stop();
}

Scene::Scene(const Scene& scene) {
  this->scene_name = scene.scene_name;
  const auto pack = this->registry.storage<entt::entity>().data();
  const auto pack_size = this->registry.storage<entt::entity>().size();
  this->registry.storage<entt::entity>().push(pack, pack + pack_size);
}

void Scene::rigidbody_component_ctor(entt::registry& reg, entt::entity entity) {
  auto& component = reg.get<RigidbodyComponent>(entity);
  create_rigidbody(entity, reg.get<TransformComponent>(entity), component);
}

void Scene::collider_component_ctor(entt::registry& reg, entt::entity entity) {
  if (reg.all_of<RigidbodyComponent>(entity))
    create_rigidbody(entity, reg.get<TransformComponent>(entity), reg.get<RigidbodyComponent>(entity));
}

void Scene::character_controller_component_ctor(entt::registry& reg, entt::entity entity) const {
  auto& component = reg.get<CharacterControllerComponent>(entity);
  create_character_controller(reg.get<TransformComponent>(entity), component);
}

void Scene::init(const Shared<RenderPipeline>& render_pipeline) {
  OX_SCOPED_ZONE;

  // ctors
  // TODO: remove these...
  //registry.on_construct<RigidbodyComponent>().connect<&Scene::rigidbody_component_ctor>(this);
  //registry.on_construct<BoxColliderComponent>().connect<&Scene::collider_component_ctor>(this);
  //registry.on_construct<SphereColliderComponent>().connect<&Scene::collider_component_ctor>(this);
  //registry.on_construct<CapsuleColliderComponent>().connect<&Scene::collider_component_ctor>(this);
  //registry.on_construct<TaperedCapsuleColliderComponent>().connect<&Scene::collider_component_ctor>(this);
  //registry.on_construct<CylinderColliderComponent>().connect<&Scene::collider_component_ctor>(this);
  //registry.on_construct<MeshColliderComponent>().connect<&Scene::collider_component_ctor>(this);
  //registry.on_construct<CharacterControllerComponent>().connect<&Scene::character_controller_component_ctor>(this);

  dispatcher.sink<FutureMeshLoadEvent>().connect<&Scene::handle_future_mesh_load_event>(*this);

  // Renderer
  scene_renderer = create_shared<SceneRenderer>(this);

  scene_renderer->set_render_pipeline(render_pipeline);
  scene_renderer->init(dispatcher);
}

Entity Scene::create_entity(const std::string& name) { return create_entity_with_uuid(UUID(), name); }

entt::entity Scene::create_entity_with_uuid(UUID uuid, const std::string& name) {
  OX_SCOPED_ZONE;
  entt::entity ent = registry.create();
  entity_map.emplace(uuid, ent);
  registry.emplace<IDComponent>(ent, uuid);
  registry.emplace<RelationshipComponent>(ent);
  registry.emplace<TransformComponent>(ent);
  registry.emplace<TagComponent>(ent).tag = name.empty() ? "Entity" : name;
  return ent;
}

Entity Scene::load_mesh(const Shared<Mesh>& mesh) {
  entt::entity root_entity = entt::null;

  MeshComponent* parent = nullptr;
  auto traverse = [this, mesh, &parent, &root_entity](const Mesh::Node* node, const entt::entity parent_entity, auto& traverse_func) -> void {
    const Entity node_entity = create_entity(node->name);
    if (parent_entity != entt::null)
      eutil::set_parent(this, node_entity, parent_entity);

    // TODO: we could actually use this to have material components for each child instead of having them all in the parent maybe?
    auto& tc = registry.get<TransformComponent>(node_entity);
    tc.set_from_matrix(node->get_local_transform());
    if (!parent) {
      parent = &registry.get_or_emplace<MeshComponent>(node_entity, mesh);
      root_entity = node_entity;
    } else {
      parent->child_entities.emplace_back(node_entity);
    }

    for (const auto& child : node->children) {
      traverse_func(child, node_entity, traverse_func);
    }
  };

  for (const auto& node : mesh->root_nodes) {
    traverse(node, entt::null, traverse);
  }

  return root_entity;
}

void Scene::update_physics(const Timestep& delta_time) {
  OX_SCOPED_ZONE;
  // Minimum stable value is 16.0
  constexpr float physics_step_rate = 50.0f;
  constexpr float physics_ts = 1.0f / physics_step_rate;

  bool stepped = false;
  physics_frame_accumulator += (float)delta_time.get_seconds();

  auto physics = App::get_system<Physics>();

  while (physics_frame_accumulator >= physics_ts) {
    physics->step(physics_ts);

    {
      {
        OX_SCOPED_ZONE_N("CPPScripting/on_fixed_update");
        const auto script_view = registry.view<CPPScriptComponent>();
        for (auto&& [e, script_component] : script_view.each()) {
          for (const auto& system : script_component.systems) {
            system->on_fixed_update(physics_ts);
          }
        }
      }

      // TODO: Lua on_fixed_update
    }

    physics_frame_accumulator -= physics_ts;
    stepped = true;
  }

  const float interpolation_factor = physics_frame_accumulator / physics_ts;

  const auto& body_interface = physics->get_physics_system()->GetBodyInterface();
  const auto view = registry.group<RigidbodyComponent>(entt::get<TransformComponent>);
  for (auto&& [e, rb, tc] : view.each()) {
    if (!rb.runtime_body)
      continue;

    const auto* body = static_cast<const JPH::Body*>(rb.runtime_body);

    if (!body_interface.IsActive(body->GetID()))
      continue;

    if (rb.interpolation) {
      if (stepped) {
        JPH::Vec3 position = body->GetPosition();
        JPH::Vec3 rotation = body->GetRotation().GetEulerAngles();

        rb.previous_translation = rb.translation;
        rb.previous_rotation = rb.rotation;
        rb.translation = {position.GetX(), position.GetY(), position.GetZ()};
        rb.rotation = Vec3(rotation.GetX(), rotation.GetY(), rotation.GetZ());
      }

      tc.position = glm::lerp(rb.previous_translation, rb.translation, interpolation_factor);
      tc.rotation = glm::eulerAngles(glm::slerp(rb.previous_rotation, rb.rotation, interpolation_factor));
    } else {
      const JPH::Vec3 position = body->GetPosition();
      JPH::Vec3 rotation = body->GetRotation().GetEulerAngles();

      rb.previous_translation = rb.translation;
      rb.previous_rotation = rb.rotation;
      rb.translation = {position.GetX(), position.GetY(), position.GetZ()};
      rb.rotation = Vec3(rotation.GetX(), rotation.GetY(), rotation.GetZ());
      tc.position = rb.translation;
      tc.rotation = glm::eulerAngles(rb.rotation);
    }
  }

  // Character
  {
    const auto ch_view = registry.view<TransformComponent, CharacterControllerComponent>();
    for (auto&& [e, tc, ch] : ch_view.each()) {
      ch.character->PostSimulation(ch.collision_tolerance);
      if (ch.interpolation) {
        if (stepped) {
          JPH::Vec3 position = ch.character->GetPosition();
          JPH::Vec3 rotation = ch.character->GetRotation().GetEulerAngles();

          ch.previous_translation = ch.translation;
          ch.previous_rotation = ch.rotation;
          ch.translation = {position.GetX(), position.GetY(), position.GetZ()};
          ch.rotation = Vec3(rotation.GetX(), rotation.GetY(), rotation.GetZ());
        }

        tc.position = glm::lerp(ch.previous_translation, ch.translation, interpolation_factor);
        tc.rotation = glm::eulerAngles(glm::slerp(ch.previous_rotation, ch.rotation, interpolation_factor));
      } else {
        const JPH::Vec3 position = ch.character->GetPosition();
        JPH::Vec3 rotation = ch.character->GetRotation().GetEulerAngles();

        ch.previous_translation = ch.translation;
        ch.previous_rotation = ch.rotation;
        ch.translation = {position.GetX(), position.GetY(), position.GetZ()};
        ch.rotation = Vec3(rotation.GetX(), rotation.GetY(), rotation.GetZ());
        tc.position = ch.translation;
        tc.rotation = glm::eulerAngles(ch.rotation);
      }
    }
  }
}

void Scene::destroy_entity(const Entity entity) {
  OX_SCOPED_ZONE;
  eutil::deparent(this, entity);
  const auto children = registry.get<RelationshipComponent>(entity).children;

  for (size_t i = 0; i < children.size(); i++) {
    const Entity child_entity = get_entity_by_uuid(children[i]);
    if (child_entity != entt::null)
      destroy_entity(child_entity);
  }

  entity_map.erase(eutil::get_uuid(registry, entity));
  registry.destroy(entity);
}

template <typename... Component>
static void copy_component(entt::registry& dst, entt::registry& src, const ankerl::unordered_dense::map<UUID, entt::entity>& entt_map) {
  ([&] {
    auto view = src.view<Component>();
    for (auto src_entity : view) {
      entt::entity dst_entity = entt_map.at(src.get<IDComponent>(src_entity).uuid);

      Component& src_component = src.get<Component>(src_entity);
      dst.emplace_or_replace<Component>(dst_entity, src_component);
    }
  }(), ...);
}

template <typename... Component>
static void copy_component(ComponentGroup<Component...>,
                           entt::registry& dst,
                           entt::registry& src,
                           const ankerl::unordered_dense::map<UUID, entt::entity>& entt_map) {
  copy_component<Component...>(dst, src, entt_map);
}

template <typename... Component>
static void copy_component_if_exists(Entity dst, Entity src, entt::registry& registry) {
  ([&] {
    if (registry.all_of<Component>(src))
      registry.emplace_or_replace<Component>(dst, registry.get<Component>(src));
  }(), ...);
}

template <typename... Component>
static void copy_component_if_exists(ComponentGroup<Component...>, Entity dst, Entity src, entt::registry& registry) {
  copy_component_if_exists<Component...>(dst, src, registry);
}

void Scene::duplicate_children(const Entity entity) {
  auto& rc = registry.get<RelationshipComponent>(entity);

  for (auto& child : rc.children) {
    const auto e = create_entity(eutil::get_name(registry, get_entity_by_uuid(child)));
    copy_component_if_exists(AllComponents{}, e, get_entity_by_uuid(child), registry);
    child = registry.get<IDComponent>(e).uuid;

#if 0
    auto& rcc = registry.get<RelationshipComponent>(e);

    if (rcc.parent > 0) {
      const auto p = create_entity(EUtil::get_name(registry, get_entity_by_uuid(child)));
      copy_component_if_exists(AllComponents{}, p, get_entity_by_uuid(rcc.parent), registry);
      rcc.parent = registry.get<IDComponent>(p).uuid;
    }
#endif
    // duplicate_children(e);
  }
}

Entity Scene::duplicate_entity(Entity entity) {
  OX_SCOPED_ZONE;
  const Entity new_entity = create_entity(eutil::get_name(registry, entity));
  copy_component_if_exists(AllComponents{}, new_entity, entity, registry);

  duplicate_children(new_entity);

  return new_entity;
}

void Scene::on_runtime_start() {
  OX_SCOPED_ZONE;

  running = true;

  physics_frame_accumulator = 0.0f;

  // Physics
  auto physics = App::get_system<Physics>();
  {
    OX_SCOPED_ZONE_N("Physics Start");
    body_activation_listener_3d = new Physics3DBodyActivationListener();
    contact_listener_3d = new Physics3DContactListener(this);
    const auto physics_system = physics->get_physics_system();
    physics_system->SetBodyActivationListener(body_activation_listener_3d);
    physics_system->SetContactListener(contact_listener_3d);

    // Rigidbodies
    {
      const auto group = registry.group<RigidbodyComponent>(entt::get<TransformComponent>);
      for (auto&& [e, rb, tc] : group.each()) {
        rb.previous_translation = rb.translation = tc.position;
        rb.previous_rotation = rb.rotation = tc.rotation;
        create_rigidbody(e, tc, rb);
      }
    }

    // Characters
    {
      const auto group = registry.group<CharacterControllerComponent>(entt::get<TransformComponent>);
      for (auto&& [e, ch, tc] : group.each()) {
        create_character_controller(tc, ch);
      }
    }

    physics_system->OptimizeBroadPhase();
  }

  // Scripting
  {
    OX_SCOPED_ZONE_N("LuaScripting/on_init");
    const auto script_view = registry.view<LuaScriptComponent>();
    for (auto&& [e, script_component] : script_view.each()) {
      for (const auto& script : script_component.lua_systems) {
        script->reload();
        script->on_init(this, e);
      }
    }
  }

  {
    OX_SCOPED_ZONE_N("CPPScripting/on_init");
    const auto script_view = registry.view<CPPScriptComponent>();
    for (auto&& [e, script_component] : script_view.each()) {
      for (const auto& system : script_component.systems) {
        system->on_init(this, e);
      }
    }
  }
}

void Scene::on_runtime_stop() {
  OX_SCOPED_ZONE;

  running = false;

  // Physics
  {
    auto physics = App::get_system<Physics>();
    JPH::BodyInterface& body_interface = physics->get_physics_system()->GetBodyInterface();
    const auto rb_view = registry.view<RigidbodyComponent>();
    for (auto&& [e, rb] : rb_view.each()) {
      if (rb.runtime_body) {
        const auto* body = static_cast<const JPH::Body*>(rb.runtime_body);
        body_interface.RemoveBody(body->GetID());
        body_interface.DestroyBody(body->GetID());
      }
    }
    const auto ch_view = registry.view<CharacterControllerComponent>();
    for (auto&& [e, ch] : ch_view.each()) {
      if (ch.character) {
        body_interface.RemoveBody(ch.character->GetBodyID());
        ch.character = nullptr;
      }
    }

    delete body_activation_listener_3d;
    delete contact_listener_3d;
    body_activation_listener_3d = nullptr;
    contact_listener_3d = nullptr;
  }

  // Scripting
  {
    const auto script_view = registry.view<CPPScriptComponent>();
    for (auto&& [e, script_component] : script_view.each()) {
      for (const auto& system : script_component.systems) {
        system->on_release(this, e);
      }
    }
  }

  {
    const auto script_view = registry.view<LuaScriptComponent>();
    for (auto&& [e, script_component] : script_view.each()) {
      for (const auto& script : script_component.lua_systems) {
        script->on_release(this, e);
      }
    }
  }
}

Entity Scene::find_entity(const std::string_view& name) {
  OX_SCOPED_ZONE;
  const auto view = registry.view<TagComponent>();
  for (auto&& [e, tag] : view.each()) {
    if (tag.tag == name) {
      return e;
    }
  }
  return entt::null;
}

bool Scene::has_entity(UUID uuid) const {
  OX_SCOPED_ZONE;
  return entity_map.contains(uuid);
}

Entity Scene::get_entity_by_uuid(UUID uuid) {
  OX_SCOPED_ZONE;
  if (entity_map.contains(uuid))
    return entity_map.at(uuid);
  return entt::null;
}

void Scene::trigger_future_mesh_load_event(FutureMeshLoadEvent future_mesh_load_event) { dispatcher.trigger(std::move(future_mesh_load_event)); }

Shared<Scene> Scene::copy(const Shared<Scene>& src_scene) {
  OX_SCOPED_ZONE;
  Shared<Scene> new_scene = create_shared<Scene>();

  auto& src_scene_registry = src_scene->registry;
  auto& dst_scene_registry = new_scene->registry;

  // Create entities in new scene
  const auto view = src_scene_registry.view<IDComponent, TagComponent>();
  for (const auto e : view) {
    auto [id, tag] = view.get<IDComponent, TagComponent>(e);
    const auto& name = tag.tag;
    const Entity new_entity = new_scene->create_entity_with_uuid(id.uuid, name);
    dst_scene_registry.get<TagComponent>(new_entity).enabled = tag.enabled;
  }

  for (const auto e : view) {
    const Entity src_parent = eutil::get_parent(src_scene.get(), e);
    if (src_parent != entt::null) {
      const Entity dst = new_scene->get_entity_by_uuid(view.get<IDComponent>(e).uuid);
      const auto parent_uuid = eutil::get_uuid(src_scene_registry, src_parent);
      eutil::set_parent(new_scene.get(), dst, new_scene->get_entity_by_uuid(parent_uuid));
    }
  }

  // Copy components (except IDComponent and TagComponent)
  copy_component(AllComponents{}, dst_scene_registry, src_scene_registry, new_scene->entity_map);

  return new_scene;
}

void Scene::on_contact_added(const JPH::Body& body1,
                             const JPH::Body& body2,
                             const JPH::ContactManifold& manifold,
                             const JPH::ContactSettings& settings) {
  OX_SCOPED_ZONE;

  {
    OX_SCOPED_ZONE_N("CPPScripting/on_contact_added");
    const auto script_view = registry.view<CPPScriptComponent>();
    for (auto&& [e, script_component] : script_view.each()) {
      for (const auto& system : script_component.systems) {
        system->on_contact_added(this, e, body1, body2, manifold, settings);
      }
    }
  }

  // TODO: Lua callbacks
}

void Scene::on_contact_persisted(const JPH::Body& body1,
                                 const JPH::Body& body2,
                                 const JPH::ContactManifold& manifold,
                                 const JPH::ContactSettings& settings) {
  OX_SCOPED_ZONE;

  {
    OX_SCOPED_ZONE_N("CPPScripting/on_contact_persisted");
    const auto script_view = registry.view<CPPScriptComponent>();
    for (auto&& [e, script_component] : script_view.each()) {
      for (const auto& system : script_component.systems) {
        system->on_contact_persisted(this, e, body1, body2, manifold, settings);
      }
    }
  }

  // TODO: Lua callbacks
}

void Scene::create_rigidbody(entt::entity entity, const TransformComponent& transform, RigidbodyComponent& component) {
  OX_SCOPED_ZONE;
  if (!running)
    return;

  // TODO: We should get rid of 'new' usages and use JPH::Ref<> instead.

  auto physics = App::get_system<Physics>();

  auto& body_interface = physics->get_body_interface();
  if (component.runtime_body) {
    body_interface.DestroyBody(static_cast<JPH::Body*>(component.runtime_body)->GetID());
    component.runtime_body = nullptr;
  }

  JPH::MutableCompoundShapeSettings compound_shape_settings;
  float max_scale_component = glm::max(glm::max(transform.scale.x, transform.scale.y), transform.scale.z);

  const auto& entity_name = eutil::get_name(registry, entity);

  if (registry.all_of<BoxColliderComponent>(entity)) {
    const auto& bc = registry.get<BoxColliderComponent>(entity);
    const auto* mat = new PhysicsMaterial3D(entity_name, JPH::ColorArg(255, 0, 0), bc.friction, bc.restitution);

    Vec3 scale = bc.size;
    JPH::BoxShapeSettings shape_settings({glm::abs(scale.x), glm::abs(scale.y), glm::abs(scale.z)}, 0.05f, mat);
    shape_settings.SetDensity(glm::max(0.001f, bc.density));

    compound_shape_settings.AddShape({bc.offset.x, bc.offset.y, bc.offset.z}, JPH::Quat::sIdentity(), shape_settings.Create().Get());
  }

  if (registry.all_of<SphereColliderComponent>(entity)) {
    const auto& sc = registry.get<SphereColliderComponent>(entity);
    const auto* mat = new PhysicsMaterial3D(entity_name, JPH::ColorArg(255, 0, 0), sc.friction, sc.restitution);

    float radius = 2.0f * sc.radius * max_scale_component;
    JPH::SphereShapeSettings shape_settings(glm::max(0.01f, radius), mat);
    shape_settings.SetDensity(glm::max(0.001f, sc.density));

    compound_shape_settings.AddShape({sc.offset.x, sc.offset.y, sc.offset.z}, JPH::Quat::sIdentity(), shape_settings.Create().Get());
  }

  if (registry.all_of<CapsuleColliderComponent>(entity)) {
    const auto& cc = registry.get<CapsuleColliderComponent>(entity);
    const auto* mat = new PhysicsMaterial3D(entity_name, JPH::ColorArg(255, 0, 0), cc.friction, cc.restitution);

    float radius = 2.0f * cc.radius * max_scale_component;
    JPH::CapsuleShapeSettings shape_settings(glm::max(0.01f, cc.height) * 0.5f, glm::max(0.01f, radius), mat);
    shape_settings.SetDensity(glm::max(0.001f, cc.density));

    compound_shape_settings.AddShape({cc.offset.x, cc.offset.y, cc.offset.z}, JPH::Quat::sIdentity(), shape_settings.Create().Get());
  }

  if (registry.all_of<TaperedCapsuleColliderComponent>(entity)) {
    const auto& tcc = registry.get<TaperedCapsuleColliderComponent>(entity);
    const auto* mat = new PhysicsMaterial3D(entity_name, JPH::ColorArg(255, 0, 0), tcc.friction, tcc.restitution);

    float top_radius = 2.0f * tcc.top_radius * max_scale_component;
    float bottom_radius = 2.0f * tcc.bottom_radius * max_scale_component;
    JPH::TaperedCapsuleShapeSettings shape_settings(glm::max(0.01f, tcc.height) * 0.5f,
                                                    glm::max(0.01f, top_radius),
                                                    glm::max(0.01f, bottom_radius),
                                                    mat);
    shape_settings.SetDensity(glm::max(0.001f, tcc.density));

    compound_shape_settings.AddShape({tcc.offset.x, tcc.offset.y, tcc.offset.z}, JPH::Quat::sIdentity(), shape_settings.Create().Get());
  }

  if (registry.all_of<CylinderColliderComponent>(entity)) {
    const auto& cc = registry.get<CylinderColliderComponent>(entity);
    const auto* mat = new PhysicsMaterial3D(entity_name, JPH::ColorArg(255, 0, 0), cc.friction, cc.restitution);

    float radius = 2.0f * cc.radius * max_scale_component;
    JPH::CylinderShapeSettings shape_settings(glm::max(0.01f, cc.height) * 0.5f, glm::max(0.01f, radius), 0.05f, mat);
    shape_settings.SetDensity(glm::max(0.001f, cc.density));

    compound_shape_settings.AddShape({cc.offset.x, cc.offset.y, cc.offset.z}, JPH::Quat::sIdentity(), shape_settings.Create().Get());
  }

  if (registry.all_of<MeshColliderComponent>(entity) && registry.all_of<MeshComponent>(entity)) {
    const auto& mc = registry.get<MeshColliderComponent>(entity);
    const auto* mat = new PhysicsMaterial3D(entity_name, JPH::ColorArg(255, 0, 0), mc.friction, mc.restitution);

    // TODO: We should only get the vertices and indices for this particular MeshComponent using MeshComponent::node_index

    const auto& mesh_component = registry.get<MeshComponent>(entity);
    auto vertices = mesh_component.mesh_base->_vertices;
    const auto& indices = mesh_component.mesh_base->_indices;

    // scale vertices
    const auto world_transform = eutil::get_world_transform(this, entity);
    for (auto& vert : vertices) {
      Vec4 scaled_pos = world_transform * Vec4(vert.position, 1.0);
      vert.position = Vec3(scaled_pos);
    }

    const uint32_t vertex_count = (uint32_t)vertices.size();
    const uint32_t index_count = (uint32_t)indices.size();
    const uint32_t triangle_count = vertex_count / 3;

    JPH::VertexList vertex_list;
    vertex_list.resize(vertex_count);
    for (uint32_t i = 0; i < vertex_count; ++i)
      vertex_list[i] = JPH::Float3(vertices[i].position.x, vertices[i].position.y, vertices[i].position.z);

    JPH::IndexedTriangleList indexedTriangleList;
    indexedTriangleList.resize(index_count * 2);

    for (uint32_t i = 0; i < triangle_count; ++i) {
      indexedTriangleList[i * 2 + 0].mIdx[0] = indices[i * 3 + 0];
      indexedTriangleList[i * 2 + 0].mIdx[1] = indices[i * 3 + 1];
      indexedTriangleList[i * 2 + 0].mIdx[2] = indices[i * 3 + 2];

      indexedTriangleList[i * 2 + 1].mIdx[2] = indices[i * 3 + 0];
      indexedTriangleList[i * 2 + 1].mIdx[1] = indices[i * 3 + 1];
      indexedTriangleList[i * 2 + 1].mIdx[0] = indices[i * 3 + 2];
    }

    JPH::PhysicsMaterialList material_list = {};
    material_list.emplace_back(mat);
    JPH::MeshShapeSettings shape_settings(vertex_list, indexedTriangleList, material_list);
    compound_shape_settings.AddShape({mc.offset.x, mc.offset.y, mc.offset.z}, JPH::Quat::sIdentity(), shape_settings.Create().Get());
  }

  // Body
  auto rotation = glm::quat(transform.rotation);

  auto layer = registry.get<TagComponent>(entity).layer;
  uint8_t layer_index = 1; // Default Layer
  auto collision_mask_it = physics->layer_collision_mask.find(layer);
  if (collision_mask_it != physics->layer_collision_mask.end())
    layer_index = collision_mask_it->second.index;

  JPH::BodyCreationSettings body_settings(compound_shape_settings.Create().Get(),
                                          {transform.position.x, transform.position.y, transform.position.z},
                                          {rotation.x, rotation.y, rotation.z, rotation.w},
                                          static_cast<JPH::EMotionType>(component.type),
                                          layer_index);

  JPH::MassProperties mass_properties;
  mass_properties.mMass = glm::max(0.01f, component.mass);
  body_settings.mMassPropertiesOverride = mass_properties;
  body_settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
  body_settings.mAllowSleeping = component.allow_sleep;
  body_settings.mLinearDamping = glm::max(0.0f, component.linear_drag);
  body_settings.mAngularDamping = glm::max(0.0f, component.angular_drag);
  body_settings.mMotionQuality = component.continuous ? JPH::EMotionQuality::LinearCast : JPH::EMotionQuality::Discrete;
  body_settings.mGravityFactor = component.gravity_scale;
  body_settings.mAllowedDOFs = (JPH::EAllowedDOFs)component.allowed_dofs;

  body_settings.mIsSensor = component.is_sensor;

  JPH::Body* body = body_interface.CreateBody(body_settings);

  JPH::EActivation activation = component.awake && component.type != RigidbodyComponent::BodyType::Static ? JPH::EActivation::Activate
                                                                                                          : JPH::EActivation::DontActivate;
  body_interface.AddBody(body->GetID(), activation);

  body->SetUserData((uint64)entity);

  component.runtime_body = body;
}

void Scene::create_character_controller(const TransformComponent& transform, CharacterControllerComponent& component) const {
  OX_SCOPED_ZONE;
  if (!running)
    return;

  auto physics = App::get_system<Physics>();

  const auto position = JPH::Vec3(transform.position.x, transform.position.y, transform.position.z);
  const auto capsule_shape = JPH::RotatedTranslatedShapeSettings(JPH::Vec3(0,
                                                                           0.5f * component.character_height_standing +
                                                                             component.character_radius_standing,
                                                                           0),
                                                                 JPH::Quat::sIdentity(),
                                                                 new JPH::CapsuleShape(0.5f * component.character_height_standing,
                                                                                       component.character_radius_standing))
                               .Create()
                               .Get();

  // Create character
  const Shared<JPH::CharacterSettings> settings = create_shared<JPH::CharacterSettings>();
  settings->mMaxSlopeAngle = JPH::DegreesToRadians(45.0f);
  settings->mLayer = PhysicsLayers::MOVING;
  settings->mShape = capsule_shape;
  settings->mFriction = 0.0f;                                                     // For now this is not set.
  settings->mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(),
                                           -component.character_radius_standing); // Accept contacts that touch the lower sphere of the capsule
  component.character = create_shared<JPH::Character>(settings.get(), position, JPH::Quat::sIdentity(), 0, physics->get_physics_system());
  component.character->AddToPhysicsSystem(JPH::EActivation::Activate);
}

void Scene::handle_future_mesh_load_event(const FutureMeshLoadEvent& event) {
  const auto task_scheduler = App::get_system<TaskScheduler>();
  event.task->on_complete([this](const Shared<Mesh>& mesh) { this->load_mesh(mesh); });
  task_scheduler->schedule_task(event.task);
}

void Scene::on_runtime_update(const Timestep& delta_time) {
  OX_SCOPED_ZONE;

  // Camera
  {
    OX_SCOPED_ZONE_N("Camera System");
    const auto camera_view = registry.view<TransformComponent, CameraComponent>();
    for (const auto entity : camera_view) {
      auto [transform, camera] = camera_view.get<TransformComponent, CameraComponent>(entity);
      camera.camera->update(transform.position, transform.rotation);
      scene_renderer->get_render_pipeline()->submit_camera(camera.camera.get());
    }
  }

  // TODO: maybe bindings should be done once at entity creation...
  //       we can do it with entt on_create etc. callbacks...
  // scripting binds
  const auto cpp_script_view = registry.view<CPPScriptComponent>();
  {
    OX_SCOPED_ZONE_N("CPPScripting/binding");
    for (auto&& [e, script_component] : cpp_script_view.each()) {
      for (const auto& system : script_component.systems) {
        system->bind_globals(this, e, &dispatcher);
      }
    }
  }

  const auto lua_script_view = registry.view<LuaScriptComponent>();
  {
    OX_SCOPED_ZONE_N("LuaScripting/binding");
    for (auto&& [e, script_component] : lua_script_view.each()) {
      for (const auto& script : script_component.lua_systems) {
        script->bind_globals(this, e, delta_time);
      }
    }
  }

  scene_renderer->update(delta_time);

  update_physics(delta_time);

  // Scripting
  {
    OX_SCOPED_ZONE_N("CPPScripting/on_update");
    for (auto&& [e, script_component] : cpp_script_view.each()) {
      for (const auto& system : script_component.systems) {
        system->on_update(delta_time);
      }
    }
  }

  {
    OX_SCOPED_ZONE_N("LuaScripting/on_update");
    for (auto&& [e, script_component] : lua_script_view.each()) {
      for (const auto& script : script_component.lua_systems) {
        script->on_update(delta_time);
      }
    }
  }

  // Audio
  {
    OX_SCOPED_ZONE_N("Audio Systems");
    const auto listener_view = registry.group<AudioListenerComponent>(entt::get<TransformComponent>);
    for (auto&& [e, ac, tc] : listener_view.each()) {
      ac.listener = create_shared<AudioListener>();
      if (ac.active) {
        const Mat4 inverted = inverse(eutil::get_world_transform(this, e));
        const Vec3 forward = normalize(Vec3(inverted[2]));
        ac.listener->set_config(ac.config);
        ac.listener->set_position(tc.position);
        ac.listener->set_direction(-forward);
        break;
      }
    }

    const auto source_view = registry.group<AudioSourceComponent>(entt::get<TransformComponent>);
    for (auto&& [e, ac, tc] : source_view.each()) {
      if (ac.source) {
        const Mat4 inverted = inverse(eutil::get_world_transform(this, e));
        const Vec3 forward = normalize(Vec3(inverted[2]));
        ac.source->set_config(ac.config);
        ac.source->set_position(tc.position);
        ac.source->set_direction(forward);
        if (ac.config.play_on_awake)
          ac.source->play();
      }
    }
  }
}

void Scene::on_imgui_render(const Timestep& delta_time) {
  OX_SCOPED_ZONE;

  {
    OX_SCOPED_ZONE_N("CPPScripting/on_imgui_render");
    const auto script_view = registry.view<CPPScriptComponent>();
    for (auto&& [e, script_component] : script_view.each()) {
      for (const auto& system : script_component.systems) {
        system->on_imgui_render(delta_time);
      }
    }
  }

  {
    OX_SCOPED_ZONE_N("LuaScripting/on_imgui_render");
    const auto script_view = registry.view<LuaScriptComponent>();
    for (auto&& [e, script_component] : script_view.each()) {
      for (const auto& script : script_component.lua_systems) {
        script->on_imgui_render(delta_time);
      }
    }
  }
}

void Scene::on_editor_update(const Timestep& delta_time, Camera& camera) {
  OX_SCOPED_ZONE;
  scene_renderer->get_render_pipeline()->submit_camera(&camera);
  scene_renderer->update(delta_time);
}
} // namespace ox
