#include "Physics.hpp"

#include <cstdarg>

#include "Jolt/Physics/Body/BodyManager.h"
#include "RayCast.hpp"

#include "Core/App.hpp"
#include "Utils/OxMath.hpp"

#include "Jolt/Physics/Collision/CastResult.h"
#include "Jolt/Physics/Collision/RayCast.h"
#include "Jolt/RegisterTypes.h"

#include "Utils/Log.hpp"
#include "Utils/Profiler.hpp"

namespace ox {
Physics* Physics::_instance = nullptr;

static void TraceImpl(const char* inFMT, ...) {
  va_list list;
  va_start(list, inFMT);
  char buffer[1024];
  vsnprintf(buffer, sizeof(buffer), inFMT, list);
  va_end(list);

  OX_LOG_INFO("{}", buffer);
}

#ifdef JPH_ENABLE_ASSERTS
static bool AssertFailedImpl(const char* inExpression, const char* inMessage, const char* inFile, JPH::uint inLine) {
  OX_LOG_ERROR("{0}:{1}:{2} {3}", inFile, inLine, inExpression, inMessage != nullptr ? inMessage : "");
  return true;
};
#endif

void Physics::init() {
  // TODO: Override default allocators with Oxylus allocators.
  JPH::RegisterDefaultAllocator();

  // Install callbacks
  JPH::Trace = TraceImpl;
#ifdef JPH_ENABLE_ASSERTS
  JPH::AssertFailed = AssertFailedImpl;
#endif

  JPH::Factory::sInstance = new JPH::Factory();
  JPH::RegisterTypes();

  debug_renderer = new PhysicsDebugRenderer();

  temp_allocator = new JPH::TempAllocatorImpl(10 * 1024 * 1024);

  job_system = new JPH::JobSystemThreadPool();
  job_system->Init(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, (int)std::thread::hardware_concurrency() - 1);
  physics_system = new JPH::PhysicsSystem();
  physics_system->Init(MAX_BODIES,
                       0,
                       MAX_BODY_PAIRS,
                       MAX_CONTACT_CONSTRAINS,
                       layer_interface,
                       object_vs_broad_phase_layer_filter_interface,
                       object_layer_pair_filter_interface);
}

void Physics::set_instance() {
  if (_instance == nullptr)
    _instance = App::get_system<Physics>();
  JPH::Factory::sInstance = new JPH::Factory();
  JPH::RegisterTypes();
}

void Physics::step(float physicsTs) {
  OX_SCOPED_ZONE;

  OX_CHECK_NULL(physics_system, "Physics system not initialized");

  physics_system->Update(physicsTs, 1, temp_allocator, job_system);
}

void Physics::debug_draw() {
  JPH::BodyManager::DrawSettings settings{};
  settings.mDrawShape = true;
  settings.mDrawShapeWireframe = true;

  physics_system->DrawBodies(settings, debug_renderer);
}

void Physics::deinit() {
  JPH::UnregisterTypes();
  delete JPH::Factory::sInstance;
  JPH::Factory::sInstance = nullptr;
  delete temp_allocator;
  delete physics_system;
  delete job_system;
  delete debug_renderer;
}

JPH::PhysicsSystem* Physics::get_physics_system() {
  OX_SCOPED_ZONE;

  OX_CHECK_NULL(_instance->physics_system, "Physics system not initialized");

  return _instance->physics_system;
}

JPH::AllHitCollisionCollector<JPH::RayCastBodyCollector> Physics::cast_ray(const RayCast& ray_cast) {
  JPH::AllHitCollisionCollector<JPH::RayCastBodyCollector> collector;
  const JPH::RayCast ray{math::to_jolt(ray_cast.get_origin()), math::to_jolt(ray_cast.get_direction())};
  _instance->get_broad_phase_query().CastRay(ray, collector);

  return collector;
}
} // namespace ox
