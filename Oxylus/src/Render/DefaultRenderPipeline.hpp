#pragma once

#include <glm/ext/scalar_constants.hpp> // Required for packing
#include <glm/fwd.hpp>
#include <glm/gtc/packing.inl>
#include <vuk/Value.hpp>

#include "Passes/FSR.hpp"
#include "RenderPipeline.hpp"
#include "RendererConfig.hpp"

#include "Passes/GTAO.hpp"
#include "Passes/SPD.hpp"
#include "Scene/Components.hpp"
#include "Utils/OxMath.hpp"

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
  void submit_mesh_component(const MeshComponent& render_object) override;
  void submit_light(const LightComponent& light) override;
  void submit_camera(Camera* camera) override;
  void submit_sprite(const SpriteComponent& sprite) override;

private:
  Camera* current_camera = nullptr;
  Camera frozen_camera = {};

  bool initalized = false;
  bool first_pass = true;
  bool resized = false;
  bool saved_camera = false;

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
  static constexpr auto NORMAL_VERTEX_IMAGE_INDEX = 12;

  // buffers and buffer/image combined indices
  static constexpr auto LIGHTS_BUFFER_INDEX = 0;
  static constexpr auto MATERIALS_BUFFER_INDEX = 1;
  static constexpr auto MESH_INSTANCES_BUFFER_INDEX = 2;
  static constexpr auto ENTITIES_BUFFER_INDEX = 3;
  static constexpr auto GTAO_BUFFER_IMAGE_INDEX = 4;
  static constexpr auto TRANSFORMS_BUFFER_INDEX = 5;
  static constexpr auto SPRITE_MATERIALS_BUFFER_INDEX = 6;

  // rw buffers indices
  static constexpr auto DEBUG_AABB_INDEX = 0;

  struct LightData {
    float3 position;

    float3 rotation;
    uint32 type8_flags8_range16;

    uint2 direction16_cone_angle_cos16; // coneAngleCos is used for cascade count in directional light
    uint2 color;                        // half4 packed

    float4 shadow_atlas_mul_add;

    uint32 radius16_length16;
    uint32 matrix_index;
    uint32 remap;

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
    void set_collider_tip(float3 value) { shadow_atlas_mul_add = float4(value.x, value.y, value.z, 0); }
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
    uint32 num_lights;
    float grid_max_distance;
    UVec2 screen_size;
    int draw_meshlet_aabbs;

    Vec2 screen_size_rcp;
    UVec2 shadow_atlas_res;

    Vec3 sun_direction;
    uint32 meshlet_count;

    Vec4 sun_color; // pre-multipled with intensity

    struct Indices {
      int albedo_image_index;
      int normal_image_index;
      int normal_vertex_image_index;
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
      int transforms_buffer_index;
      int sprite_materials_buffer_index;
    } indices;

    struct PostProcessingData {
      int tonemapper = RendererConfig::TONEMAP_ACES;
      float exposure = 1.0f;
      float gamma = 2.5f;

      int enable_bloom = 1;
      int enable_ssr = 1;
      int enable_gtao = 1;

      Vec4 vignette_color = Vec4(0.0f, 0.0f, 0.0f, 0.25f); // rgb: color, a: intensity
      Vec4 vignette_offset = Vec4(0.0f, 0.0f, 0.0f, 0.0f); // xy: offset, z: useMask, w: enable effect
      Vec2 film_grain = {};                                // x: enable, y: amount
      Vec2 chromatic_aberration = {};                      // x: enable, y: amount
      Vec2 sharpen = {};                                   // x: enable, y: amount
    } post_processing_data;
  } scene_data;

#define MAX_AABB_COUNT 100000

  struct DebugAabb {
    float3 center;
    float3 extent;
    float4 color;
  };

  vuk::Unique<vuk::PersistentDescriptorSet> descriptor_set_00;
  vuk::Unique<vuk::PersistentDescriptorSet> descriptor_set_02;

  vuk::Buffer visible_meshlets_buffer;
  vuk::Buffer cull_triangles_dispatch_params_buffer;
  vuk::Buffer vertex_buffer;
  vuk::Buffer index_buffer;
  vuk::Buffer primitives_buffer;
  vuk::Buffer transforms_buffer;
  vuk::Buffer instanced_index_buffer;
  vuk::Buffer indirect_commands_buffer;
  vuk::Buffer debug_aabb_buffer;

  vuk::Buffer vertex_buffer_2d;
  vuk::Unique<vuk::Buffer> debug_vertex_buffer;
  vuk::Unique<vuk::Buffer> debug_vertex_buffer_previous;

  vuk::SamplerCreateInfo hiz_sampler_ci;
  VkSamplerReductionModeCreateInfoEXT create_info_reduction;
  Texture color_texture;
  Texture albedo_texture;
  Unique<Texture> depth_texture;
  Unique<Texture> depth_texture_prev;
  Texture material_depth_texture;
  Texture hiz_texture;
  Texture normal_texture;
  Texture normal_vertex_texture;
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

  struct DrawBatch2D {
    vuk::Name pipeline_name = {};
    uint32 offset = 0;
    uint32 count = 0;
  };

  enum RenderFlags2D : uint32 {
    RENDER_FLAGS_2D_NONE = 0,

    RENDER_FLAGS_2D_SORT_Y = 1 << 0,
    RENDER_FLAGS_2D_FLIP_X = 1 << 1,
  };

  struct SpriteGPUData {
    float4x4 transform = {};
    uint32 material_id16_ypos16 = 0;
    uint32 flags16_distance16 = 0;

    constexpr bool operator>(const SpriteGPUData& other) const {
      union SortKey {
        struct {
          // The order of members is important here, it means the sort priority (low to high)!
          uint64_t distance_y : 32;
          uint64_t distance_z : 32;
        } bits;

        uint64_t value;
      };
      static_assert(sizeof(SortKey) == sizeof(uint64_t));
      const SortKey a = {
        .bits =
          {
            .distance_y = math::unpack_u32_low(flags16_distance16) & RENDER_FLAGS_2D_SORT_Y ? math::unpack_u32_high(material_id16_ypos16) : 0u,
            .distance_z = math::unpack_u32_high(flags16_distance16),
          },
      };
      const SortKey b = {
        .bits =
          {
            .distance_y = math::unpack_u32_low(other.flags16_distance16) & RENDER_FLAGS_2D_SORT_Y ? math::unpack_u32_high(other.material_id16_ypos16)
                                                                                                  : 0u,
            .distance_z = math::unpack_u32_high(other.flags16_distance16),
          },
      };
      return a.value > b.value;
    }
  };

  struct RenderQueue2D {
    std::vector<DrawBatch2D> batches = {};
    std::vector<SpriteGPUData> sprite_data = {};
    std::vector<Shared<SpriteMaterial>> materials = {};

    vuk::Name current_pipeline_name = {};

    uint32 num_sprites = 0;
    uint32 previous_offset = 0;

    uint32 last_batches_size = 0;
    uint32 last_sprite_data_size = 0;
    uint32 last_materials_size = 0;

    void init() {
      batches.reserve(last_batches_size);
      sprite_data.reserve(last_sprite_data_size);
      materials.reserve(last_materials_size);
    }

    // TODO: this will take a list of materials
    // TODO: sort pipelines
    void update() {
      const vuk::Name pipeline_name = "2d_forward_pipeline"; // FIXME: hardcoded until we have a modular material shader system
      if (current_pipeline_name != pipeline_name) {
        batches.emplace_back(DrawBatch2D{.pipeline_name = pipeline_name, .offset = previous_offset, .count = num_sprites - previous_offset});
        current_pipeline_name = pipeline_name;
      }

      previous_offset = num_sprites;
    }

    void add(const SpriteComponent& sprite, float distance) {
      sprite.material->set_id((uint32)materials.size());
      materials.emplace_back(sprite.material);

      uint16 flags = 0;
      if (sprite.sort_y)
        flags |= RENDER_FLAGS_2D_SORT_Y;

      if (sprite.flip_x)
        flags |= RENDER_FLAGS_2D_FLIP_X;

      const uint32 flags_and_distance = math::pack_u16(uint16(flags), glm::packHalf1x16(distance));
      const uint32 materialid_and_ypos = math::pack_u16(uint16(sprite.material->get_id()), glm::packHalf1x16(sprite.get_position().y));

      sprite_data.emplace_back(SpriteGPUData{
        .transform = sprite.transform,
        .material_id16_ypos16 = materialid_and_ypos,
        .flags16_distance16 = flags_and_distance,
      });

      num_sprites += 1;
    }

    void sort() { std::sort(sprite_data.begin(), sprite_data.end(), std::greater<SpriteGPUData>()); }

    void clear() {
      num_sprites = 0;
      previous_offset = 0;
      last_batches_size = (uint32)batches.size();
      last_sprite_data_size = (uint32)sprite_data.size();
      last_materials_size = (uint32)materials.size();
      current_pipeline_name = {};

      batches.clear();
      sprite_data.clear();
      materials.clear();
    }
  };

  RenderQueue2D render_queue_2d;

  struct SceneFlattened {
    std::vector<Mesh::Meshlet> meshlets;
    std::vector<Mesh::MeshletInstance> meshlet_instances;
    std::vector<Mat4> transforms;
    std::vector<Shared<PBRMaterial>> materials;

    std::vector<uint32> indices{};
    std::vector<Vertex> vertices{};
    std::vector<uint32> primitives{};

    uint32 last_meshlet_size = 0;
    uint32 last_meshlet_instances_size = 0;
    uint32 last_indices_size = 0;
    uint32 last_vertices_size = 0;
    uint32 last_primitives_size = 0;
    uint32 last_transforms_size = 0;

    uint32 get_meshlet_instances_count() const { return (uint32)meshlet_instances.size(); }
    uint32 get_material_count() const { return (uint32)materials.size(); }

    void init() {
      meshlets.reserve(last_meshlet_size);
      meshlet_instances.reserve(last_meshlet_instances_size);

      indices.reserve(last_indices_size);
      vertices.reserve(last_vertices_size);
      primitives.reserve(last_primitives_size);
      transforms.reserve(last_transforms_size);
    }

    void clear() {
      last_meshlet_size = (uint32)meshlets.size();
      last_meshlet_instances_size = (uint32)meshlet_instances.size();

      last_indices_size = (uint32)indices.size();
      last_vertices_size = (uint32)vertices.size();
      last_primitives_size = (uint32)primitives.size();

      indices.clear();
      vertices.clear();
      primitives.clear();
      transforms.clear();

      meshlets.clear();
      meshlet_instances.clear();
      materials.clear();
    }

    void update(const std::vector<MeshComponent>& mc_list, const std::vector<SpriteComponent>& sp_list) {
      OX_SCOPED_ZONE;

      if (mc_list.empty()) {
        meshlet_instances.emplace_back();
        meshlets.emplace_back();
        indices.emplace_back();
        vertices.emplace_back();
        primitives.emplace_back();
        transforms.emplace_back();
        materials.emplace_back(create_shared<PBRMaterial>());

        return;
      }

      for (auto& mc : mc_list) {
        for (int node_index = 0; auto& node : mc.mesh_base->nodes) {
          if (!node.meshlet_indices.empty()) {
            const auto instance_id = (uint32)transforms.size();
            const auto transform = node_index == 0 ? mc.transform : mc.child_transforms[node_index - 1];
            transforms.emplace_back(transform);
            for (auto& [meshletIndex, _, materialId] : node.meshlet_indices) {
              // meshlet.instance_id = uint32(instance_id);
              meshlet_instances.emplace_back(meshletIndex, instance_id, materialId);
            }
            node_index++;
          }
        }

        meshlets.insert(std::end(meshlets), std::begin(mc.mesh_base->_meshlets), std::end(mc.mesh_base->_meshlets));
        indices.insert(std::end(indices), std::begin(mc.mesh_base->_indices), std::end(mc.mesh_base->_indices));
        vertices.insert(std::end(vertices), std::begin(mc.mesh_base->_vertices), std::end(mc.mesh_base->_vertices));
        primitives.insert(std::end(primitives), std::begin(mc.mesh_base->_primitives), std::end(mc.mesh_base->_primitives));
        materials.insert(std::end(materials), std::begin(mc.materials), std::end(mc.materials));
      }
    }
  };

  SceneFlattened scene_flattened;
  std::vector<MeshComponent> mesh_component_list;
  std::vector<SpriteComponent> sprite_component_list;
  Shared<Mesh> m_quad = nullptr;
  Shared<Mesh> m_cube = nullptr;
  Shared<Camera> default_camera;

  std::vector<LightComponent> scene_lights = {};
  LightComponent* dir_light_data = nullptr;

  void clear();
  void bind_camera_buffer(vuk::CommandBuffer& command_buffer);
  CameraData get_main_camera_data(bool use_frozen_camera = false);
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
  [[nodiscard]] vuk::Value<vuk::ImageAttachment> apply_fxaa(vuk::Value<vuk::ImageAttachment>& target, vuk::Value<vuk::ImageAttachment>& input);
  [[nodiscard]] vuk::Value<vuk::ImageAttachment> shadow_pass(vuk::Value<vuk::ImageAttachment>& shadow_map);
  [[nodiscard]] vuk::Value<vuk::ImageAttachment> gtao_pass(vuk::Allocator& frame_allocator,
                                                           vuk::Value<vuk::ImageAttachment>& gtao_final_output,
                                                           vuk::Value<vuk::ImageAttachment>& depth_input,
                                                           vuk::Value<vuk::ImageAttachment>& normal_input);
  [[nodiscard]] vuk::Value<vuk::ImageAttachment> bloom_pass(vuk::Value<vuk::ImageAttachment>& downsample_image,
                                                            vuk::Value<vuk::ImageAttachment>& upsample_image,
                                                            vuk::Value<vuk::ImageAttachment>& input);
  [[nodiscard]] vuk::Value<vuk::ImageAttachment> apply_grid(vuk::Value<vuk::ImageAttachment>& target, vuk::Value<vuk::ImageAttachment>& depth);
  [[nodiscard]] vuk::Value<vuk::ImageAttachment> debug_pass(vuk::Allocator& frame_allocator,
                                                            vuk::Value<vuk::ImageAttachment>& depth_output,
                                                            vuk::Value<vuk::ImageAttachment>& input_clr);
};
} // namespace ox
