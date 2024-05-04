﻿#pragma once

#include <glm/ext/scalar_constants.hpp>
#include <glm/fwd.hpp>
#include <glm/gtc/packing.inl>
#include <vuk/Value.hpp>

#include "Passes/FSR.hpp"
#include "RenderPipeline.hpp"
#include "RendererConfig.hpp"

#include "Passes/GTAO.hpp"
#include "Passes/SPD.hpp"

namespace ox {
struct SkyboxLoadEvent;

class DefaultRenderPipeline : public RenderPipeline {
public:
  explicit DefaultRenderPipeline(const std::string& name) : RenderPipeline(name) {}

  ~DefaultRenderPipeline() override = default;

  void init(vuk::Allocator& allocator) override;
  void load_pipelines(vuk::Allocator& allocator);
  void shutdown() override;

  [[nodiscard]] vuk::Value<vuk::ImageAttachment> on_render(vuk::Allocator& frame_allocator,
                                                           vuk::Value<vuk::ImageAttachment> target,
                                                           vuk::Extent3D ext) override;
  void on_update(Scene* scene) override;
  void on_submit() override;

  void on_dispatcher_events(EventDispatcher& dispatcher) override;
  void register_mesh_component(const MeshComponent& render_object) override;
  void register_light(const LightComponent& light) override;
  void register_camera(Camera* camera) override;

private:
  Camera* current_camera = nullptr;

  bool initalized = false;
  bool first_pass = false;

  struct MeshInstance {
    Mat4 transform;
  };

  struct MeshInstancePointer {
    uint32_t data;

    void create(uint32_t instance_index, uint32_t camera_index = 0, float dither = 0) {
      data = 0;
      data |= instance_index & 0xFFFFFF;
      data |= (camera_index & 0xF) << 24u;
      data |= (uint32_t(dither * 15.0f) & 0xF) << 28u;
    }
  };

  struct ShaderEntity {
    Mat4 transform;
  };

  // scene cubemap textures
  static constexpr auto SKY_ENVMAP_INDEX = 0;

  // scene textures
  static constexpr auto ALBEDO_IMAGE_INDEX = 0;
  static constexpr auto NORMAL_IMAGE_INDEX = 1;
  static constexpr auto DEPTH_IMAGE_INDEX = 2;
  static constexpr auto SHADOW_ATLAS_INDEX = 3;
  static constexpr auto SKY_TRANSMITTANCE_LUT_INDEX = 4;
  static constexpr auto SKY_MULTISCATTER_LUT_INDEX = 5;
  static constexpr auto VELOCITY_IMAGE_INDEX = 6;
  static constexpr auto BLOOM_IMAGE_INDEX = 7;
  static constexpr auto HIZ_IMAGE_INDEX = 8;
  static constexpr auto VIS_IMAGE_INDEX = 9;
  static constexpr auto METALROUGHAO_IMAGE_INDEX = 10;
  static constexpr auto EMISSION_IMAGE_INDEX = 11;

  // buffers and buffer/image combined indices
  static constexpr auto LIGHTS_BUFFER_INDEX = 0;
  static constexpr auto MATERIALS_BUFFER_INDEX = 1;
  static constexpr auto MESH_INSTANCES_BUFFER_INDEX = 2;
  static constexpr auto ENTITIES_BUFFER_INDEX = 3;
  static constexpr auto GTAO_BUFFER_IMAGE_INDEX = 4;

  struct LightData {
    float3 position;
    float _pad0;

    float3 rotation;
    uint32 type8_flags8_range16;

    uint2 direction16_cone_angle_cos16; // coneAngleCos is used for cascade count in directional light
    uint2 color;                        // half4 packed

    float4 shadow_atlas_mul_add;

    uint32 radius16_length16;
    uint32 matrix_index;
    uint32 remap;
    uint32 _pad1;

    void set_type(uint32 type) { type8_flags8_range16 |= type & 0xFF; }
    void set_flags(uint32 flags) { type8_flags8_range16 |= (flags & 0xFF) << 8u; }
    void set_range(float value) { type8_flags8_range16 |= glm::packHalf1x16(value) << 16u; }
    void set_radius(float value) { radius16_length16 |= glm::packHalf1x16(value); }
    void set_length(float value) { radius16_length16 |= glm::packHalf1x16(value) << 16u; }
    void set_color(float4 value) {
      color.x |= glm::packHalf1x16(value.x);
      color.x |= glm::packHalf1x16(value.y) << 16u;
      color.y |= glm::packHalf1x16(value.z);
      color.y |= glm::packHalf1x16(value.w) << 16u;
    }
    void set_direction(float3 value) {
      direction16_cone_angle_cos16.x |= glm::packHalf1x16(value.x);
      direction16_cone_angle_cos16.x |= glm::packHalf1x16(value.y) << 16u;
      direction16_cone_angle_cos16.y |= glm::packHalf1x16(value.z);
    }
    void set_cone_angle_cos(float value) { direction16_cone_angle_cos16.y |= glm::packHalf1x16(value) << 16u; }
    void set_shadow_cascade_count(uint32 value) { direction16_cone_angle_cos16.y |= (value & 0xFFFF) << 16u; }
    void set_angle_scale(float value) { remap |= glm::packHalf1x16(value); }
    void set_angle_offset(float value) { remap |= glm::packHalf1x16(value) << 16u; }
    void set_cube_remap_near(float value) { remap |= glm::packHalf1x16(value); }
    void set_cube_remap_far(float value) { remap |= glm::packHalf1x16(value) << 16u; }
    void set_indices(uint32 indices) { matrix_index = indices; }
    void set_gravity(float value) { set_cone_angle_cos(value); }
    void SetColliderTip(float3 value) { shadow_atlas_mul_add = float4(value.x, value.y, value.z, 0); }
  };

  std::vector<LightData> light_datas;

  struct CameraSH {
    Mat4 projection_view;
    Frustum frustum;
  };

  struct CameraData {
    Vec4 position = {};

    Mat4 projection = {};
    Mat4 inv_projection = {};
    Mat4 view = {};
    Mat4 inv_view = {};
    Mat4 projection_view = {};
    Mat4 inv_projection_view = {};

    Mat4 previous_projection = {};
    Mat4 previous_inv_projection = {};
    Mat4 previous_view = {};
    Mat4 previous_inv_view = {};
    Mat4 previous_projection_view = {};
    Mat4 previous_inv_projection_view = {};

    Vec2 temporalaa_jitter = {};
    Vec2 temporalaa_jitter_prev = {};

    Vec4 frustum_planes[6] = {};

    Vec3 up = {};
    float near_clip = 0;
    Vec3 forward = {};
    float far_clip = 0;
    Vec3 right = {};
    float fov = 0;
    Vec3 _pad = {};
    uint32_t output_index = 0;
  };

  struct CameraCB {
    CameraData camera_data[16];
  } camera_cb;

  // GPU Buffer
  struct SceneData {
    int num_lights;
    float grid_max_distance;
    UVec2 screen_size;

    Vec2 screen_size_rcp;
    UVec2 shadow_atlas_res;

    Vec3 sun_direction;
    uint32 meshlet_count;

    Vec4 sun_color; // pre-multipled with intensity

    struct Indices {
      int albedo_image_index;
      int normal_image_index;
      int depth_image_index;
      int bloom_image_index;

      int mesh_instance_buffer_index;
      int entites_buffer_index;
      int materials_buffer_index;
      int lights_buffer_index;

      int sky_env_map_index;
      int sky_transmittance_lut_index;
      int sky_multiscatter_lut_index;
      int velocity_image_index;

      int shadow_array_index;
      int gtao_buffer_image_index;
      int hiz_image_index;
      int vis_image_index;

      int emission_image_index;
      int metallic_roughness_ao_image_index;
      int2 _pad1;
    } indices;

    struct PostProcessingData {
      int tonemapper = RendererConfig::TONEMAP_ACES;
      float exposure = 1.0f;
      float gamma = 2.5f;
      int _pad;

      int enable_bloom = 1;
      int enable_ssr = 1;
      int enable_gtao = 1;
      int _pad2;

      Vec4 vignette_color = Vec4(0.0f, 0.0f, 0.0f, 0.25f); // rgb: color, a: intensity
      Vec4 vignette_offset = Vec4(0.0f, 0.0f, 0.0f, 0.0f); // xy: offset, z: useMask, w: enable effect

      Vec2 film_grain = {};                                // x: enable, y: amount
      Vec2 chromatic_aberration = {};                      // x: enable, y: amount

      Vec2 sharpen = {};                                   // x: enable, y: amount
      Vec2 _pad3;
    } post_processing_data;
  } scene_data;

  struct ShaderPC {
    uint64_t vertex_buffer_ptr;
    uint32_t mesh_index;
    uint32_t material_index;
  };

  vuk::Unique<vuk::PersistentDescriptorSet> descriptor_set_00;
  vuk::Unique<vuk::PersistentDescriptorSet> descriptor_set_02;

  vuk::Unique<vuk::Buffer> visible_meshlets_buffer;
  vuk::Unique<vuk::Buffer> cull_triangles_dispatch_params_buffer;
  vuk::Unique<vuk::Buffer> index_buffer;
  vuk::Unique<vuk::Buffer> instanced_index_buffer;
  vuk::Unique<vuk::Buffer> indirect_commands_buffer;

  Texture color_texture;
  Texture albedo_texture;
  Texture depth_texture;
  Texture material_depth_texture;
  Texture hiz_texture;
  Texture normal_texture;
  Texture velocity_texture;
  Texture visibility_texture;
  Texture emission_texture;
  Texture metallic_roughness_texture;

  Texture sky_transmittance_lut;
  Texture sky_multiscatter_lut;
  Texture sky_envmap_texture;
  Texture gtao_final_texture;
  Texture ssr_texture;
  Texture shadow_map_atlas;
  Texture shadow_map_atlas_transparent;

  GTAOConstants gtao_constants = {};
  GTAOSettings gtao_settings = {};

  FSR fsr = {};
  SPD envmap_spd = {};
  SPD hiz_spd = {};

  Shared<Texture> cube_map = nullptr;
  vuk::ImageAttachment brdf_texture;
  vuk::ImageAttachment irradiance_texture;
  vuk::ImageAttachment prefiltered_texture;

  enum Filter {
    // Include nothing:
    FILTER_NONE = 0,

    // Object filtering types:
    FILTER_OPAQUE = 1 << 0,
    FILTER_TRANSPARENT = 1 << 1,
    FILTER_CLIP = 1 << 2,
    FILTER_WATER = 1 << 3,
    FILTER_NAVIGATION_MESH = 1 << 4,
    FILTER_OBJECT_ALL = FILTER_OPAQUE | FILTER_TRANSPARENT | FILTER_CLIP | FILTER_WATER | FILTER_NAVIGATION_MESH,

    // Include everything:
    FILTER_ALL = ~0,
  };

  enum RenderFlags {
    RENDER_FLAGS_NONE = 0,

    RENDER_FLAGS_SHADOWS_PASS = 1 << 0,
  };

  struct RenderBatch {
    uint32_t mesh_index;
    uint32_t component_index;
    uint32_t instance_index;
    uint16_t distance;
    uint16_t camera_mask;
    uint32_t sort_bits; // an additional bitmask for sorting only, it should be used to reduce pipeline changes

    void create(const uint32_t mesh_idx,
                const uint32_t component_idx,
                const uint32_t instance_idx,
                const float distance,
                const uint32_t sort_bits,
                const uint16_t camera_mask = 0xFFFF) {
      this->mesh_index = mesh_idx;
      this->component_index = component_idx;
      this->instance_index = instance_idx;
      this->distance = uint16_t(glm::floatBitsToUint(distance));
      this->sort_bits = sort_bits;
      this->camera_mask = camera_mask;
    }

    float get_distance() const { return glm::uintBitsToFloat(distance); }
    constexpr uint32_t get_mesh_index() const { return mesh_index; }
    constexpr uint32_t get_instance_index() const { return instance_index; }

    // opaque sorting
    // Priority is set to mesh index to have more instancing
    // distance is second priority (front to back Z-buffering)
    constexpr bool operator<(const RenderBatch& other) const {
      union SortKey {
        struct {
          // The order of members is important here, it means the sort priority (low to high)!
          uint64_t distance : 16;
          uint64_t mesh_index : 16;
          uint64_t sort_bits : 32;
        } bits;

        uint64_t value;
      };
      static_assert(sizeof(SortKey) == sizeof(uint64_t));
      const SortKey a = {.bits = {.distance = distance, .mesh_index = mesh_index, .sort_bits = sort_bits}};
      const SortKey b = {.bits = {.distance = other.distance, .mesh_index = other.mesh_index, .sort_bits = other.sort_bits}};
      return a.value < b.value;
    }

    // transparent sorting
    // Priority is distance for correct alpha blending (back to front rendering)
    // mesh index is second priority for instancing
    constexpr bool operator>(const RenderBatch& other) const {
      union SortKey {
        struct {
          // The order of members is important here, it means the sort priority (low to high)!
          uint64_t mesh_index : 16;
          uint64_t sort_bits : 32;
          uint64_t distance : 16;
        } bits;

        uint64_t value;
      };
      static_assert(sizeof(SortKey) == sizeof(uint64_t));
      const SortKey a = {.bits = {.mesh_index = mesh_index, .sort_bits = sort_bits, .distance = distance}};
      const SortKey b = {.bits = {.mesh_index = other.mesh_index, .sort_bits = other.sort_bits, .distance = other.distance}};
      return a.value > b.value;
    }
  };

  struct RenderQueue {
    std::vector<RenderBatch> batches = {};

    void clear() { batches.clear(); }

    void add(const uint32_t mesh_index,
             const uint32_t component_index,
             const uint32_t instance_index,
             const float distance,
             const uint32_t sort_bits,
             const uint16_t camera_mask = 0xFFFF) {
      batches.emplace_back().create(mesh_index, component_index, instance_index, distance, sort_bits, camera_mask);
    }

    RenderBatch& add(const RenderBatch& render_batch) { return batches.emplace_back(render_batch); }

    void sort_transparent() {
      OX_SCOPED_ZONE;
      std::sort(batches.begin(), batches.end(), std::greater<RenderBatch>());
    }

    void sort_opaque() {
      OX_SCOPED_ZONE;
      std::sort(batches.begin(), batches.end(), std::less<RenderBatch>());
    }

    bool empty() const { return batches.empty(); }

    size_t size() const { return batches.size(); }
  };

  Mesh::SceneFlattened scene_flattened;
  std::vector<MeshComponent> mesh_component_list;
  RenderQueue render_queue;
  Shared<Mesh> m_quad = nullptr;
  Shared<Mesh> m_cube = nullptr;
  Shared<Camera> default_camera;

  std::vector<LightComponent> scene_lights = {};
  LightComponent* dir_light_data = nullptr;

  void clear();
  void bind_camera_buffer(vuk::CommandBuffer& command_buffer);
  CameraData get_main_camera_data() const;
  void create_dir_light_cameras(const LightComponent& light, Camera& camera, std::vector<CameraSH>& camera_data, uint32_t cascade_count);
  void create_cubemap_cameras(std::vector<CameraSH>& camera_data, Vec3 pos = {}, float near = 0.1f, float far = 90.0f);
  void update_frame_data(vuk::Allocator& allocator);
  void create_static_resources();
  void create_dynamic_textures(const vuk::Extent3D& ext);
  void create_descriptor_sets(vuk::Allocator& allocator);
  void run_static_passes(vuk::Allocator& allocator);

  void update_skybox(const SkyboxLoadEvent& e);
  void generate_prefilter(vuk::Allocator& allocator);

  [[nodiscard]] vuk::Value<vuk::ImageAttachment> sky_envmap_pass(vuk::Value<vuk::ImageAttachment>& envmap_image);
  [[nodiscard]] vuk::Value<vuk::ImageAttachment> sky_transmittance_pass();
  [[nodiscard]] vuk::Value<vuk::ImageAttachment> sky_multiscatter_pass(vuk::Value<vuk::ImageAttachment>& transmittance_lut);
  [[nodiscard]] std::tuple<vuk::Value<vuk::ImageAttachment>, vuk::Value<vuk::ImageAttachment>, vuk::Value<vuk::ImageAttachment>>
  depth_pre_pass(const vuk::Value<vuk::ImageAttachment>& depth_image,
                 const vuk::Value<vuk::ImageAttachment>& normal_image,
                 const vuk::Value<vuk::ImageAttachment>& velocity_image);
  void render_meshes(const RenderQueue& render_queue,
                     vuk::CommandBuffer& command_buffer,
                     uint32_t filter,
                     uint32_t flags = 0,
                     uint32_t camera_count = 1) const;
  [[nodiscard]] vuk::Value<vuk::ImageAttachment> forward_pass(const vuk::Value<vuk::ImageAttachment>& output,
                                                              const vuk::Value<vuk::ImageAttachment>& depth_input,
                                                              const vuk::Value<vuk::ImageAttachment>& shadow_map,
                                                              const vuk::Value<vuk::ImageAttachment>& transmittance_lut,
                                                              const vuk::Value<vuk::ImageAttachment>& multiscatter_lut,
                                                              const vuk::Value<vuk::ImageAttachment>& envmap,
                                                              const vuk::Value<vuk::ImageAttachment>& gtao);
  [[nodiscard]] vuk::Value<vuk::ImageAttachment> apply_fxaa(vuk::Value<vuk::ImageAttachment>& target, vuk::Value<vuk::ImageAttachment>& input);
  [[nodiscard]] vuk::Value<vuk::ImageAttachment> shadow_pass(vuk::Value<vuk::ImageAttachment>& shadow_map);
  [[nodiscard]] vuk::Value<vuk::ImageAttachment> gtao_pass(vuk::Allocator& frame_allocator,
                                                           vuk::Value<vuk::ImageAttachment>& gtao_final_output,
                                                           vuk::Value<vuk::ImageAttachment>& depth_input,
                                                           vuk::Value<vuk::ImageAttachment>& normal_input);
  [[nodiscard]] vuk::Value<vuk::ImageAttachment> bloom_pass(vuk::Value<vuk::ImageAttachment>& downsample_image,
                                                            vuk::Value<vuk::ImageAttachment>& upsample_image,
                                                            vuk::Value<vuk::ImageAttachment>& input);
  [[nodiscard]] vuk::Value<vuk::ImageAttachment> debug_pass(vuk::Allocator& frame_allocator,
                                                            vuk::Value<vuk::ImageAttachment>& input,
                                                            vuk::Value<vuk::ImageAttachment>& depth) const;
  [[nodiscard]] vuk::Value<vuk::ImageAttachment> apply_grid(vuk::Value<vuk::ImageAttachment>& target, vuk::Value<vuk::ImageAttachment>& depth);
  [[nodiscard]] std::tuple<vuk::Value<vuk::Buffer>, vuk::Value<vuk::Buffer>> cull_meshlets_pass(vuk::Value<vuk::ImageAttachment>& hiz,
                                                                                                vuk::Value<vuk::Buffer> vis_meshlets_buf,
                                                                                                vuk::Value<vuk::Buffer> cull_triangles_buf,
                                                                                                vuk::Value<vuk::Buffer> instanced_idx_buffer,
                                                                                                vuk::Value<vuk::Buffer> meshlet_indirect_buf);
};
} // namespace ox
