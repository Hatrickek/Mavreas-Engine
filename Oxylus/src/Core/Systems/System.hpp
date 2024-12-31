#pragma once

#include <Scene/Entity.hpp>
#include "Event/Event.hpp"
#include "Utils/Timestep.hpp"
#include "entt/entity/fwd.hpp"

namespace JPH {
class ContactSettings;
class ContactManifold;
class Body;
} // namespace JPH

namespace ox {
class System {
public:
  std::size_t hash_code = {}; // set in SystemManager::register_system

  System() = default;
  virtual ~System() = default;

  /// Called right after when the scene gets initalized.
  virtual void on_init(Scene* scene, entt::entity e) {}

  /// Called when the system is destroyed.
  virtual void on_release(Scene* scene, entt::entity e) {}

  /// Called after physic system is updated.
  virtual void on_update(const Timestep& delta_time) {}

  /// Called every fixed frame-rate frame with the frequency of the physics system
  virtual void on_fixed_update(float delta_time) {}

  /// Called in the main imgui loop which is right after `App::on_update`
  virtual void on_imgui_render(const Timestep& delta_time) {}

  /// Called right after main loop is finished before the core shutdown process.
  virtual void on_shutdown() {}

  /// Physics interfaces
  virtual void on_contact_added(Scene* scene,
                                entt::entity e,
                                const JPH::Body& body1,
                                const JPH::Body& body2,
                                const JPH::ContactManifold& manifold,
                                const JPH::ContactSettings& settings) {}
  virtual void on_contact_persisted(Scene* scene,
                                    entt::entity e,
                                    const JPH::Body& body1,
                                    const JPH::Body& body2,
                                    const JPH::ContactManifold& manifold,
                                    const JPH::ContactSettings& settings) {}

  void bind_globals(Scene* s, entt::entity e, EventDispatcher* d) {
    scene = s;
    entity = e;
    dispatcher = d;
  }

protected:
  Scene* scene = nullptr;
  entt::entity entity = entt::null;
  EventDispatcher* dispatcher = nullptr;
};
} // namespace ox
