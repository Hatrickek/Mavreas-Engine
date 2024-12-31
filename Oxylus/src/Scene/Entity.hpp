#pragma once

#include <entt/entity/registry.hpp>

#include "Core/Types.hpp"
#include "Core/UUID.hpp"

namespace ox {
using Entity = entt::entity;
class Scene;
} // namespace ox

namespace ox::eutil {

///@brief Utility entity class
const UUID& get_uuid(const entt::registry& reg, entt::entity ent);
const std::string& get_name(const entt::registry& reg, entt::entity ent);

void deparent(Scene* scene, entt::entity entity);
entt::entity get_parent(Scene* scene, entt::entity entity);
entt::entity get_child(Scene* scene, entt::entity entity, uint32_t index);
void get_all_children(Scene* scene, entt::entity parent, std::vector<entt::entity>& out_entities);
void set_parent(Scene* scene, entt::entity entity, entt::entity parent);
Mat4 get_world_transform(Scene* scene, Entity entity);
Mat4 get_local_transform(Scene* scene, Entity entity);
} // namespace ox::eutil
