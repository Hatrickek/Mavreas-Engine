#pragma once
#include <optional>
#include <string>

#include "Assets/TilemapSerializer.hpp"
#include "Core/App.hpp"
#include "Core/Base.hpp"
#include "Core/Systems/SystemManager.hpp"
#include "Core/Types.hpp"
#include "Core/UUID.hpp"

#include "Audio/AudioListener.hpp"
#include "Audio/AudioSource.hpp"

#include "Assets/SpriteMaterial.hpp"
#include "Render/Camera.hpp"
#include "Render/Mesh.hpp"
#include "Render/ParticleSystem.hpp"
#include "Render/Utils/RectPacker.hpp"

#include "Scripting/LuaSystem.hpp"

namespace JPH {
class Character;
}

namespace ox {
class Texture;

struct IDComponent {
  UUID uuid;

  IDComponent() = default;

  IDComponent(const IDComponent&) = default;

  IDComponent(UUID id) : uuid(id) {}
};

struct TagComponent {
  std::string tag;
  uint16_t layer = BIT(1);
  bool enabled = true;

  // non-serialized data
  bool handled = true;

  TagComponent() = default;

  TagComponent(std::string tag) : tag(std::move(tag)) {}
};

struct RelationshipComponent {
  UUID parent = 0;
  std::vector<UUID> children{};
};

struct PrefabComponent {
  UUID id;
};

struct TransformComponent {
  static constexpr auto in_place_delete = true;

  Vec3 position = Vec3(0);
  Vec3 rotation = Vec3(0); // Stored in radians
  Vec3 scale = Vec3(1);

  TransformComponent() = default;
  TransformComponent(const TransformComponent&) = default;
  TransformComponent(const Vec3& translation) : position(translation) {}

  TransformComponent(const Mat4& transform_matrix) {
    OX_SCOPED_ZONE;
    math::decompose_transform(transform_matrix, position, rotation, scale);
  }

  void set_from_matrix(const Mat4& transform_matrix) {
    OX_SCOPED_ZONE;
    math::decompose_transform(transform_matrix, position, rotation, scale);
  }

  float4x4 get_local_transform() const {
    return glm::translate(Mat4(1.0f), position) * glm::toMat4(glm::quat(rotation)) * glm::scale(Mat4(1.0f), scale);
  }
};

// Rendering
struct MeshComponent {
  static constexpr auto in_place_delete = true; // pointer stability

  Shared<Mesh> mesh_base = nullptr;
  bool cast_shadows = true;
  bool stationary = false;

  // non-serialized data
  uint32_t mesh_id = Asset::INVALID_ID;
  std::vector<Shared<PBRMaterial>> materials = {};
  Mat4 transform = Mat4{1};
  std::vector<entt::entity> child_entities = {}; // filled at load
  std::vector<Mat4> child_transforms = {};       // filled at submit
  AABB aabb = {};
  bool dirty = false;

  MeshComponent() = default;

  explicit MeshComponent(const Shared<Mesh>& mesh) : mesh_base(mesh) {
    materials = mesh_base->_materials;
    mesh_id = mesh->get_id();
    dirty = true;
  }
};

struct SpriteComponent {
  Shared<SpriteMaterial> material = nullptr;
  uint32 layer = 0;

  bool sort_y = true;
  bool flip_x = false;

  // non-serialized data
  float4x4 transform = {};
  AABB rect = {};

  // set if an animation is controlling this sprite
  std::optional<float2> current_uv_offset = std::nullopt;

  SpriteComponent() {
    material = create_shared<SpriteMaterial>();
    material->create();
  }

  float3 get_position() const { return float3(transform[3]); }
  float2 get_size() const { return {glm::length(glm::vec3(transform[0])), glm::length(glm::vec3(transform[1]))}; }
};

struct SpriteAnimationComponent {
  uint32 num_frames = 0;
  bool loop = true;
  bool inverted = false;
  uint32 fps = 0;
  uint32 columns = 1;
  float2 frame_size = {};

  // non-serialized data
  float current_time = 0.f;
  bool is_inverted = false;

  void reset() { current_time = 0.f; }

  void set_frame_size(Texture* sprite) {
    if (num_frames > 0) {
      const auto horizontal = sprite->get_extent().width / num_frames;
      const auto vertical = sprite->get_extent().height;

      frame_size = {horizontal, vertical};

      reset();
    }
  }

  void set_num_frames(uint32 value) {
    num_frames = value;
    reset();
  }

  void set_fps(uint32 value) {
    fps = value;
    reset();
  }

  void set_columns(uint32 value) {
    columns = value;
    reset();
  }
};

struct TilemapComponent {
  std::string path = {};
  ankerl::unordered_dense::map<std::string, Shared<SpriteMaterial>> layers = {};
  int2 tilemap_size = {64, 64};

  TilemapComponent() {}

  void load(const std::string& _path) {
    path = _path;
    TilemapSerializer serializer(this);
    serializer.deserialize(path);
  }
};

struct CameraComponent {
  Shared<Camera> camera = nullptr;

  CameraComponent() { camera = create_shared<Camera>(); }
};

struct ParticleSystemComponent {
  Shared<ParticleSystem> system = nullptr;

  ParticleSystemComponent() : system(create_shared<ParticleSystem>()) {}
};

struct LightComponent {
  enum LightType { Directional = 0, Point, Spot };

  LightType type = Point;
  bool color_temperature_mode = false;
  uint32_t temperature = 6570;
  Vec3 color = Vec3(1.0f);
  float intensity = 1.0f;

  float range = 1.0f;
  float radius = 0.025f;
  float length = 0;
  float outer_cone_angle = glm::pi<float>() / 4.0f;
  float inner_cone_angle = 0;

  bool cast_shadows = true;
  uint32_t shadow_map_res = 0;
  std::vector<float> cascade_distances = {8, 80, 800};

  // non-serialized data
  Vec3 position = {};
  Vec3 rotation = {};
  Vec3 direction = {};
  RectPacker::Rect shadow_rect = {};
};

struct PostProcessProbe {
  bool vignette_enabled = false;
  float vignette_intensity = 0.25f;

  bool film_grain_enabled = false;
  float film_grain_intensity = 0.2f;

  bool chromatic_aberration_enabled = false;
  float chromatic_aberration_intensity = 0.5f;

  bool sharpen_enabled = false;
  float sharpen_intensity = 0.5f;
};

// Physics
struct RigidbodyComponent {
  enum class BodyType { Static = 0, Kinematic, Dynamic };
  enum class AllowedDOFs : uint32 {
    None = 0b000000,         ///< No degrees of freedom are allowed. Note that this is not valid and will crash. Use a static body instead.
    All = 0b111111,          ///< All degrees of freedom are allowed
    TranslationX = 0b000001, ///< Body can move in world space X axis
    TranslationY = 0b000010, ///< Body can move in world space Y axis
    TranslationZ = 0b000100, ///< Body can move in world space Z axis
    RotationX = 0b001000,    ///< Body can rotate around world space X axis
    RotationY = 0b010000,    ///< Body can rotate around world space Y axis
    RotationZ = 0b100000,    ///< Body can rotate around world space Z axis
    Plane2D = TranslationX | TranslationY | RotationZ, ///< Body can only move in X and Y axis and rotate around Z axis
  };

  AllowedDOFs allowed_dofs = AllowedDOFs::All;
  BodyType type = BodyType::Dynamic;
  float mass = 1.0f;
  float linear_drag = 0.0f;
  float angular_drag = 0.05f;
  float gravity_scale = 1.0f;
  bool allow_sleep = true;
  bool awake = true;
  bool continuous = false;
  bool interpolation = false;

  bool is_sensor = false;

  // Stored as JPH::Body
  void* runtime_body = nullptr;

  // For interpolation/extrapolation
  Vec3 previous_translation = Vec3(0.0f);
  glm::quat previous_rotation = Vec3(0.0f);
  Vec3 translation = Vec3(0.0f);
  glm::quat rotation = Vec3(0.0f);

  JPH::Body* get_body() { return static_cast<JPH::Body*>(runtime_body); }
};

struct BoxColliderComponent {
  Vec3 size = {0.5f, 0.5f, 0.5f};
  Vec3 offset = {0.0f, 0.0f, 0.0f};
  float density = 1.0f;

  float friction = 0.5f;
  float restitution = 0.0f;
};

struct SphereColliderComponent {
  float radius = 0.5f;
  Vec3 offset = {0.0f, 0.0f, 0.0f};
  float density = 1.0f;

  float friction = 0.5f;
  float restitution = 0.0f;
};

struct CapsuleColliderComponent {
  float height = 1.0f;
  float radius = 0.5f;
  Vec3 offset = {0.0f, 0.0f, 0.0f};
  float density = 1.0f;

  float friction = 0.5f;
  float restitution = 0.0f;
};

struct TaperedCapsuleColliderComponent {
  float height = 1.0f;
  float top_radius = 0.5f;
  float bottom_radius = 0.5f;
  Vec3 offset = {0.0f, 0.0f, 0.0f};
  float density = 1.0f;

  float friction = 0.5f;
  float restitution = 0.0f;
};

struct CylinderColliderComponent {
  float height = 1.0f;
  float radius = 0.5f;
  Vec3 offset = {0.0f, 0.0f, 0.0f};
  float density = 1.0f;

  float friction = 0.5f;
  float restitution = 0.0f;
};

struct MeshColliderComponent {
  Vec3 offset = {0.0f, 0.0f, 0.0f};
  float friction = 0.5f;
  float restitution = 0.0f;
};

struct CharacterControllerComponent {
  Shared<JPH::Character> character = nullptr;

  // Size
  float character_height_standing = 1.35f;
  float character_radius_standing = 0.3f;
  float character_height_crouching = 0.8f;
  float character_radius_crouching = 0.3f;

  // Movement
  struct MovementSettings {
    float max_speed;
    float acceleration;
    float deceleration;

    MovementSettings(const float maxSpeed, float accel, float decel) : max_speed(maxSpeed), acceleration(accel), deceleration(decel) {}
  };

  bool interpolation = true;

  bool control_movement_during_jump = true;
  float jump_force = 8.0f;
  bool auto_bunny_hop = true;
  float air_control = 0.3f;
  MovementSettings ground_settings = MovementSettings(7, 14, 10);
  MovementSettings air_settings = MovementSettings(7, 2, 2);
  MovementSettings strafe_settings = MovementSettings(0.0f, 50, 50);

  float friction = 6.0f;
  float gravity = 20;
  float collision_tolerance = 0.05f;

  // For interpolation/extrapolation
  Vec3 previous_translation = Vec3(0.0f);
  Quat previous_rotation = Vec3(0.0f);
  Vec3 translation = Vec3(0.0f);
  Quat rotation = Vec3(0.0f);

  CharacterControllerComponent() = default;
};

// Audio
struct AudioSourceComponent {
  AudioSourceConfig config;

  Shared<AudioSource> source = nullptr;
};

struct AudioListenerComponent {
  bool active = true;
  AudioListenerConfig config;

  Shared<AudioListener> listener;
};

// Scripting
struct LuaScriptComponent {
  std::vector<Shared<LuaSystem>> lua_systems = {};
};

struct CPPScriptComponent {
  std::vector<Shared<System>> systems = {};

  template <typename T>
  void add_system() {
    auto system = App::get_system<SystemManager>()->register_system<T>();
    systems.emplace(system);
  }
};

template <typename... Component>
struct ComponentGroup {};

using AllComponents = ComponentGroup<TransformComponent,
                                     RelationshipComponent,
                                     PrefabComponent,
                                     CameraComponent,

                                     // Render
                                     LightComponent,
                                     MeshComponent,
                                     ParticleSystemComponent,
                                     SpriteComponent,
                                     SpriteAnimationComponent,
                                     TilemapComponent,

                                     //  Physics
                                     RigidbodyComponent,
                                     BoxColliderComponent,
                                     SphereColliderComponent,
                                     CapsuleColliderComponent,
                                     TaperedCapsuleColliderComponent,
                                     CylinderColliderComponent,
                                     MeshColliderComponent,

                                     // Audio
                                     AudioSourceComponent,
                                     AudioListenerComponent,

                                     // Scripting
                                     LuaScriptComponent,
                                     CPPScriptComponent>;
} // namespace ox
