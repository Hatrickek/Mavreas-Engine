#include "Entity.hpp"
#include "Scene.hpp"

#include "Utils/Log.hpp"

#include "Components.hpp"

namespace ox::eutil {
const UUID& get_uuid(const entt::registry& reg, entt::entity ent) { return reg.get<IDComponent>(ent).uuid; }
const std::string& get_name(const entt::registry& reg, entt::entity ent) { return reg.get<TagComponent>(ent).tag; }

entt::entity get_parent(Scene* scene, entt::entity entity) {
  const auto& rc = scene->registry.get<RelationshipComponent>(entity);
  return rc.parent != 0 ? scene->get_entity_by_uuid(rc.parent) : entt::null;
}

entt::entity get_child(Scene* scene, entt::entity entity, uint32_t index) {
  const auto& rc = scene->registry.get<RelationshipComponent>(entity);
  return scene->get_entity_by_uuid(rc.children[index]);
}

void get_all_children(Scene* scene, entt::entity parent, std::vector<entt::entity>& out_entities) {
  const std::vector<UUID> children = scene->registry.get<RelationshipComponent>(parent).children;
  for (const auto& child : children) {
    entt::entity entity = scene->get_entity_by_uuid(child);
    out_entities.emplace_back(entity);
    get_all_children(scene, entity, out_entities);
  }
}

void deparent(Scene* scene, entt::entity entity) {
  auto& transform = scene->registry.get<RelationshipComponent>(entity);
  const UUID uuid = get_uuid(scene->registry, entity);
  const entt::entity parent_entity = get_parent(scene, entity);

  if (parent_entity == entt::null)
    return;

  auto& parent = scene->registry.get<RelationshipComponent>(parent_entity);
  std::erase_if(parent.children, [uuid](const UUID child) { return child == uuid; });
  transform.parent = 0;
}

void set_parent(Scene* scene, entt::entity entity, entt::entity parent) {
  auto uuid = get_uuid(scene->registry, parent);
  OX_ASSERT(scene->entity_map.contains(uuid), "Parent is not in the same scene as entity");
  deparent(scene, entity);

  auto& [parent_uuid, _] = scene->registry.get<RelationshipComponent>(entity);
  parent_uuid = get_uuid(scene->registry, parent);
  scene->registry.get<RelationshipComponent>(parent).children.emplace_back(get_uuid(scene->registry, entity));
}

Mat4 get_world_transform(Scene* scene, Entity entity) {
  OX_SCOPED_ZONE;
  const auto& transform = scene->registry.get<TransformComponent>(entity);
  const auto& rc = scene->registry.get<RelationshipComponent>(entity);
  const Entity parent = scene->get_entity_by_uuid(rc.parent);
  const Mat4 parent_transform = parent != entt::null ? get_world_transform(scene, parent) : Mat4(1.0f);
  return parent_transform * glm::translate(Mat4(1.0f), transform.position) * glm::toMat4(glm::quat(transform.rotation)) *
         glm::scale(Mat4(1.0f), transform.scale);
}

Mat4 get_local_transform(Scene* scene, Entity entity) {
  OX_SCOPED_ZONE;
  const auto& transform = scene->registry.get<TransformComponent>(entity);
  return transform.get_local_transform();
}
} // namespace ox
