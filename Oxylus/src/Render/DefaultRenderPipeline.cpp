#include "DefaultRenderPipeline.hpp"

#include <ankerl/unordered_dense.h>
#include <cstdint>
#include <glm/gtc/type_ptr.inl>
#include <vuk/RenderGraph.hpp>
#include <vuk/runtime/CommandBuffer.hpp>
#include <vuk/runtime/vk/Descriptor.hpp>
#include <vuk/runtime/vk/Pipeline.hpp>
#include <vuk/vsl/Core.hpp>

#include "DebugRenderer.hpp"
#include "MeshVertex.hpp"
#include "RendererCommon.hpp"
#include "SceneRendererEvents.hpp"

#include "Core/App.hpp"
#include "Passes/Prefilter.hpp"

#include "Scene/Scene.hpp"

#include "Thread/TaskScheduler.hpp"

#include "Utils/CVars.hpp"
#include "Utils/Log.hpp"
#include "Utils/Profiler.hpp"
#include "Utils/Timer.hpp"

#include "Vulkan/VkContext.hpp"

#include "Core/FileSystem.hpp"
#include "Renderer.hpp"
#include "Utils/VukCommon.hpp"

#include "Utils/RectPacker.hpp"
#include "vuk/ImageAttachment.hpp"
#include "vuk/Value.hpp"
#include "vuk/runtime/vk/Allocator.hpp"

namespace ox {
VkDescriptorSetLayoutBinding binding(uint32_t binding, vuk::DescriptorType descriptor_type, uint32_t count = 1024) {
  return {
    .binding = binding,
    .descriptorType = (VkDescriptorType)descriptor_type,
    .descriptorCount = count,
    .stageFlags = (VkShaderStageFlags)vuk::ShaderStageFlagBits::eAll,
  };
}

void DefaultRenderPipeline::init(vuk::Allocator& allocator) {
  OX_SCOPED_ZONE;

  const Timer timer = {};

  load_pipelines(allocator);

  if (initalized) {
    OX_LOG_INFO("Reloaded render pipeline.");
    return;
  }

  const auto task_scheduler = App::get_system<TaskScheduler>();

  this->m_quad = RendererCommon::generate_quad();
  this->m_cube = RendererCommon::generate_cube();
  task_scheduler->add_task([this] { create_static_resources(); });
  task_scheduler->add_task([this, &allocator] { create_descriptor_sets(allocator); });

  task_scheduler->wait_for_all();

  initalized = true;

  OX_LOG_INFO("DefaultRenderPipeline initialized: {} ms", timer.get_elapsed_ms());
}

void DefaultRenderPipeline::load_pipelines(vuk::Allocator& allocator) {
  OX_SCOPED_ZONE;

  vuk::PipelineBaseCreateInfo bindless_pci = {};
  vuk::DescriptorSetLayoutCreateInfo bindless_dslci_00 = {};
  bindless_dslci_00.bindings = {
    binding(0, vuk::DescriptorType::eStorageBuffer, 1),
    binding(1, vuk::DescriptorType::eStorageBuffer),
    binding(2, vuk::DescriptorType::eStorageBuffer),
    binding(3, vuk::DescriptorType::eSampledImage),
    binding(4, vuk::DescriptorType::eSampledImage),
    binding(5, vuk::DescriptorType::eSampledImage),
    binding(6, vuk::DescriptorType::eSampledImage, 8),
    binding(7, vuk::DescriptorType::eSampledImage, 8),
    binding(8, vuk::DescriptorType::eStorageImage),
    binding(9, vuk::DescriptorType::eStorageImage),
    binding(10, vuk::DescriptorType::eSampledImage),
    binding(11, vuk::DescriptorType::eSampler),
    binding(12, vuk::DescriptorType::eSampler),
  };
  bindless_dslci_00.index = 0;
  for (int i = 0; i < 13; i++)
    bindless_dslci_00.flags.emplace_back(VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);
  bindless_pci.explicit_set_layouts.emplace_back(bindless_dslci_00);

  using SS = vuk::HlslShaderStage;

#define SHADER_FILE(path) fs::read_shader_file(path), fs::get_shader_path(path)

  auto* task_scheduler = App::get_system<TaskScheduler>();

  task_scheduler->add_task([=]() mutable {
    bindless_pci.add_hlsl(SHADER_FILE("FullscreenTriangle.hlsl"), SS::eVertex);
    bindless_pci.add_hlsl(SHADER_FILE("FinalPass.hlsl"), SS::ePixel);
    TRY(allocator.get_context().create_named_pipeline("final_pipeline", bindless_pci))
  });

  task_scheduler->add_task([=]() mutable {
    bindless_pci.add_hlsl(SHADER_FILE("DepthCopy.hlsl"), SS::eCompute);
    TRY(allocator.get_context().create_named_pipeline("depth_copy_pipeline", bindless_pci))
  });

  task_scheduler->add_task([=]() mutable {
    bindless_pci.add_hlsl(SHADER_FILE("Debug/DebugAABB.hlsl"), SS::eVertex, "VSmain");
    bindless_pci.add_hlsl(SHADER_FILE("Debug/DebugAABB.hlsl"), SS::ePixel, "PSmain");
    TRY(allocator.get_context().create_named_pipeline("debug_aabb_pipeline", bindless_pci))
  });

  // --- Culling ---
  vuk::DescriptorSetLayoutCreateInfo bindless_dslci_01 = {};
  bindless_dslci_01.bindings = {
    binding(0, vuk::DescriptorType::eStorageBuffer, 6), // read
    binding(1, vuk::DescriptorType::eStorageBuffer, 4), // rw
  };
  bindless_dslci_01.index = 2;
  for (int i = 0; i < 2; i++)
    bindless_dslci_01.flags.emplace_back(VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);

  task_scheduler->add_task([=]() mutable {
    bindless_pci.explicit_set_layouts.emplace_back(bindless_dslci_01);
    bindless_pci.add_hlsl(SHADER_FILE("VisBuffer.hlsl"), SS::eVertex, "VSmain");
    bindless_pci.add_hlsl(SHADER_FILE("VisBuffer.hlsl"), SS::ePixel, "PSmain");
    TRY(allocator.get_context().create_named_pipeline("vis_buffer_pipeline", bindless_pci))
  });

  task_scheduler->add_task([=]() mutable {
    bindless_pci.explicit_set_layouts.emplace_back(bindless_dslci_01);
    bindless_pci.add_hlsl(SHADER_FILE("FullscreenTriangle.hlsl"), SS::eVertex);
    bindless_pci.add_hlsl(SHADER_FILE("MaterialVisBuffer.hlsl"), SS::ePixel, "PSmain");
    TRY(allocator.get_context().create_named_pipeline("material_vis_buffer_pipeline", bindless_pci))
  });

  task_scheduler->add_task([=]() mutable {
    bindless_pci.explicit_set_layouts.emplace_back(bindless_dslci_01);
    bindless_pci.add_hlsl(SHADER_FILE("VisBufferResolve.hlsl"), SS::eVertex, "VSmain");
    bindless_pci.add_hlsl(SHADER_FILE("VisBufferResolve.hlsl"), SS::ePixel, "PSmain");
    TRY(allocator.get_context().create_named_pipeline("resolve_vis_buffer_pipeline", bindless_pci))
  });

  task_scheduler->add_task([=]() mutable {
    bindless_pci.explicit_set_layouts.emplace_back(bindless_dslci_01);
    bindless_pci.add_hlsl(SHADER_FILE("CullMeshlets.hlsl"), SS::eCompute);
    TRY(allocator.get_context().create_named_pipeline("cull_meshlets_pipeline", bindless_pci))
  });

  task_scheduler->add_task([=]() mutable {
    bindless_pci.explicit_set_layouts.emplace_back(bindless_dslci_01);
    bindless_pci.add_hlsl(SHADER_FILE("CullTriangles.hlsl"), SS::eCompute);
    TRY(allocator.get_context().create_named_pipeline("cull_triangles_pipeline", bindless_pci))
  });

  task_scheduler->add_task([=]() mutable {
    bindless_pci.explicit_set_layouts.emplace_back(bindless_dslci_01);
    bindless_pci.add_hlsl(SHADER_FILE("FullscreenTriangle.hlsl"), SS::eVertex);
    bindless_pci.add_hlsl(SHADER_FILE("ShadePBR.hlsl"), SS::ePixel, "PSmain");
    TRY(allocator.get_context().create_named_pipeline("shading_pipeline", bindless_pci))
  });

  // --- GTAO ---
  task_scheduler->add_task([&allocator]() mutable {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_hlsl(SHADER_FILE("GTAO/GTAO_First.hlsl"), SS::eCompute, "CSPrefilterDepths16x16");
    pci.define("XE_GTAO_FP32_DEPTHS", "");
    pci.define("XE_GTAO_USE_HALF_FLOAT_PRECISION", "0");
    pci.define("XE_GTAO_USE_DEFAULT_CONSTANTS", "0");
    TRY(allocator.get_context().create_named_pipeline("gtao_first_pipeline", pci))
  });

  task_scheduler->add_task([&allocator]() mutable {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_hlsl(SHADER_FILE("GTAO/GTAO_Main.hlsl"), SS::eCompute, "CSGTAOHigh");
    pci.define("XE_GTAO_FP32_DEPTHS", "");
    pci.define("XE_GTAO_USE_HALF_FLOAT_PRECISION", "0");
    pci.define("XE_GTAO_USE_DEFAULT_CONSTANTS", "0");
    TRY(allocator.get_context().create_named_pipeline("gtao_main_pipeline", pci))
  });

  task_scheduler->add_task([&allocator]() mutable {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_hlsl(SHADER_FILE("GTAO/GTAO_Final.hlsl"), SS::eCompute, "CSDenoisePass");
    pci.define("XE_GTAO_FP32_DEPTHS", "");
    pci.define("XE_GTAO_USE_HALF_FLOAT_PRECISION", "0");
    pci.define("XE_GTAO_USE_DEFAULT_CONSTANTS", "0");
    TRY(allocator.get_context().create_named_pipeline("gtao_denoise_pipeline", pci))
  });

  task_scheduler->add_task([&allocator]() mutable {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_hlsl(SHADER_FILE("GTAO/GTAO_Final.hlsl"), SS::eCompute, "CSDenoiseLastPass");
    pci.define("XE_GTAO_FP32_DEPTHS", "");
    pci.define("XE_GTAO_USE_HALF_FLOAT_PRECISION", "0");
    pci.define("XE_GTAO_USE_DEFAULT_CONSTANTS", "0");
    TRY(allocator.get_context().create_named_pipeline("gtao_final_pipeline", pci))
  });

  task_scheduler->add_task([&allocator]() mutable {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_hlsl(SHADER_FILE("FullscreenTriangle.hlsl"), SS::eVertex);
    pci.add_glsl(SHADER_FILE("PostProcess/FXAA.frag"));
    TRY(allocator.get_context().create_named_pipeline("fxaa_pipeline", pci))
  });

  // --- Bloom ---
  task_scheduler->add_task([&allocator]() mutable {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(SHADER_FILE("PostProcess/BloomPrefilter.comp"));
    TRY(allocator.get_context().create_named_pipeline("bloom_prefilter_pipeline", pci))
  });

  task_scheduler->add_task([&allocator]() mutable {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(SHADER_FILE("PostProcess/BloomDownsample.comp"));
    TRY(allocator.get_context().create_named_pipeline("bloom_downsample_pipeline", pci))
  });

  task_scheduler->add_task([&allocator]() mutable {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(SHADER_FILE("PostProcess/BloomUpsample.comp"));
    TRY(allocator.get_context().create_named_pipeline("bloom_upsample_pipeline", pci))
  });

  task_scheduler->add_task([=]() mutable {
    bindless_pci.add_hlsl(SHADER_FILE("Debug/Grid.hlsl"), SS::eVertex);
    bindless_pci.add_hlsl(SHADER_FILE("Debug/Grid.hlsl"), SS::ePixel, "PSmain");
    TRY(allocator.get_context().create_named_pipeline("grid_pipeline", bindless_pci))
  });

  task_scheduler->add_task([=]() mutable {
    bindless_pci.add_hlsl(SHADER_FILE("Debug/Unlit.hlsl"), SS::eVertex, "VSmain");
    bindless_pci.add_hlsl(SHADER_FILE("Debug/Unlit.hlsl"), SS::ePixel, "PSmain");
    TRY(allocator.get_context().create_named_pipeline("unlit_pipeline", bindless_pci))
  });

  // --- Atmosphere ---
  task_scheduler->add_task([=]() mutable {
    bindless_pci.add_hlsl(SHADER_FILE("Atmosphere/TransmittanceLUT.hlsl"), SS::eCompute);
    TRY(allocator.get_context().create_named_pipeline("sky_transmittance_pipeline", bindless_pci))
  });

  task_scheduler->add_task([=]() mutable {
    bindless_pci.add_hlsl(SHADER_FILE("Atmosphere/MultiScatterLUT.hlsl"), SS::eCompute);
    TRY(allocator.get_context().create_named_pipeline("sky_multiscatter_pipeline", bindless_pci))
  });

  task_scheduler->add_task([=]() mutable {
    bindless_pci.add_hlsl(SHADER_FILE("FullscreenTriangle.hlsl"), SS::eVertex);
    bindless_pci.add_hlsl(SHADER_FILE("Atmosphere/SkyView.hlsl"), SS::ePixel);
    TRY(allocator.get_context().create_named_pipeline("sky_view_pipeline", bindless_pci))
  });

  task_scheduler->add_task([=]() mutable {
    bindless_pci.add_hlsl(SHADER_FILE("Atmosphere/SkyViewFinal.hlsl"), SS::eVertex, "VSmain");
    bindless_pci.add_hlsl(SHADER_FILE("Atmosphere/SkyViewFinal.hlsl"), SS::ePixel, "PSmain");
    TRY(allocator.get_context().create_named_pipeline("sky_view_final_pipeline", bindless_pci))
  });

  task_scheduler->add_task([=]() mutable {
    bindless_pci.add_hlsl(SHADER_FILE("Atmosphere/SkyEnvMap.hlsl"), SS::eVertex, "VSmain");
    bindless_pci.add_hlsl(SHADER_FILE("Atmosphere/SkyEnvMap.hlsl"), SS::ePixel, "PSmain");
    TRY(allocator.get_context().create_named_pipeline("sky_envmap_pipeline", bindless_pci))
  });

  task_scheduler->add_task([=]() mutable {
    bindless_pci.add_hlsl(SHADER_FILE("2DForward.hlsl"), SS::eVertex, "VSmain");
    bindless_pci.add_hlsl(SHADER_FILE("2DForward.hlsl"), SS::ePixel, "PSmain");
    TRY(allocator.get_context().create_named_pipeline("2d_forward_pipeline", bindless_pci))
  });

  task_scheduler->wait_for_all();

  fsr.load_pipelines(allocator, bindless_pci);

  vuk::SamplerCreateInfo envmap_spd_sampler_ci = {};
  envmap_spd_sampler_ci.magFilter = vuk::Filter::eLinear;
  envmap_spd_sampler_ci.minFilter = vuk::Filter::eLinear;
  envmap_spd_sampler_ci.mipmapMode = vuk::SamplerMipmapMode::eNearest;
  envmap_spd_sampler_ci.addressModeU = vuk::SamplerAddressMode::eClampToEdge;
  envmap_spd_sampler_ci.addressModeV = vuk::SamplerAddressMode::eClampToEdge;
  envmap_spd_sampler_ci.addressModeW = vuk::SamplerAddressMode::eClampToEdge;
  envmap_spd_sampler_ci.minLod = -1000;
  envmap_spd_sampler_ci.maxLod = 1000;
  envmap_spd_sampler_ci.maxAnisotropy = 1.0f;

  envmap_spd.init(allocator, {.load = SPD::SPDLoad::LinearSampler, .view_type = vuk::ImageViewType::e2DArray, .sampler = envmap_spd_sampler_ci});

  hiz_sampler_ci = {
    .magFilter = vuk::Filter::eNearest,
    .minFilter = vuk::Filter::eNearest,
    .mipmapMode = vuk::SamplerMipmapMode::eNearest,
    .addressModeU = vuk::SamplerAddressMode::eClampToEdge,
    .addressModeV = vuk::SamplerAddressMode::eClampToEdge,
    .addressModeW = vuk::SamplerAddressMode::eClampToEdge,
    .minLod = 0.0f,
    .maxLod = 16.0f,
  };

  create_info_reduction = {
    .sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT,
    .pNext = nullptr,
    .reductionMode = VK_SAMPLER_REDUCTION_MODE_MIN,
  };

  hiz_sampler_ci.pNext = &create_info_reduction;

  hiz_spd.init(allocator, {.load = SPD::SPDLoad::LinearSampler, .view_type = vuk::ImageViewType::e2D, .sampler = hiz_sampler_ci});
}

void DefaultRenderPipeline::clear() {
  mesh_component_list.clear();
  sprite_component_list.clear();
  scene_lights.clear();
  light_datas.clear();
  dir_light_data = nullptr;
  scene_flattened.clear();
  render_queue_2d.clear();
}

void DefaultRenderPipeline::bind_camera_buffer(vuk::CommandBuffer& command_buffer) {
  const auto cb = command_buffer.scratch_buffer<CameraCB>(1, 0);
  *cb = camera_cb;
}

DefaultRenderPipeline::CameraData DefaultRenderPipeline::get_main_camera_data(const bool use_frozen_camera) {
  auto* cam = use_frozen_camera ? &frozen_camera : current_camera;

  CameraData camera_data{
    .position = Vec4(cam->get_position(), 0.0f),
    .projection = cam->get_projection_matrix(),
    .inv_projection = cam->get_inv_projection_matrix(),
    .view = cam->get_view_matrix(),
    .inv_view = cam->get_inv_view_matrix(),
    .projection_view = cam->get_projection_matrix() * cam->get_view_matrix(),
    .inv_projection_view = cam->get_inverse_projection_view(),
    .previous_projection = cam->get_projection_matrix(),
    .previous_inv_projection = cam->get_inv_projection_matrix(),
    .previous_view = cam->get_view_matrix(),
    .previous_inv_view = cam->get_inv_view_matrix(),
    .previous_projection_view = cam->get_projection_matrix() * cam->get_view_matrix(),
    .previous_inv_projection_view = cam->get_inverse_projection_view(),
    .near_clip = cam->get_near(),
    .far_clip = cam->get_far(),
    .fov = cam->get_fov(),
    .output_index = 0,
  };

  if (RendererCVar::cvar_fsr_enable.get())
    cam->set_jitter(fsr.get_jitter());

  camera_data.temporalaa_jitter = cam->get_jitter();
  camera_data.temporalaa_jitter_prev = cam->get_previous_jitter();

  for (uint32 i = 0; i < 6; i++) {
    const auto* plane = cam->get_frustum().planes[i];
    camera_data.frustum_planes[i] = {plane->normal, plane->distance};
  }

  return camera_data;
}

void DefaultRenderPipeline::create_dir_light_cameras(const LightComponent& light,
                                                     Camera& camera,
                                                     std::vector<CameraSH>& camera_data,
                                                     uint32_t cascade_count) {
  OX_SCOPED_ZONE;

  const auto lightRotation = glm::toMat4(glm::quat(light.rotation));
  const auto to = math::transform_normal(Vec4(0.0f, -1.0f, 0.0f, 0.0f), lightRotation);
  const auto up = math::transform_normal(Vec4(0.0f, 0.0f, 1.0f, 0.0f), lightRotation);
  auto light_view = glm::lookAt(Vec3{}, Vec3(to), Vec3(up));

  const auto unproj = camera.get_inverse_projection_view();

  Vec4 frustum_corners[8] = {
    math::transform_coord(Vec4(-1.f, -1.f, 1.f, 1.f), unproj), // near
    math::transform_coord(Vec4(-1.f, -1.f, 0.f, 1.f), unproj), // far
    math::transform_coord(Vec4(-1.f, 1.f, 1.f, 1.f), unproj),  // near
    math::transform_coord(Vec4(-1.f, 1.f, 0.f, 1.f), unproj),  // far
    math::transform_coord(Vec4(1.f, -1.f, 1.f, 1.f), unproj),  // near
    math::transform_coord(Vec4(1.f, -1.f, 0.f, 1.f), unproj),  // far
    math::transform_coord(Vec4(1.f, 1.f, 1.f, 1.f), unproj),   // near
    math::transform_coord(Vec4(1.f, 1.f, 0.f, 1.f), unproj),   // far
  };

  // Compute shadow cameras:
  for (uint32_t cascade = 0; cascade < cascade_count; ++cascade) {
    // Compute cascade bounds in light-view-space from the main frustum corners:
    const float farPlane = camera.get_far();
    const float split_near = cascade == 0 ? 0 : light.cascade_distances[cascade - 1] / farPlane;
    const float split_far = light.cascade_distances[cascade] / farPlane;

    Vec4 corners[8] = {
      math::transform(lerp(frustum_corners[0], frustum_corners[1], split_near), light_view),
      math::transform(lerp(frustum_corners[0], frustum_corners[1], split_far), light_view),
      math::transform(lerp(frustum_corners[2], frustum_corners[3], split_near), light_view),
      math::transform(lerp(frustum_corners[2], frustum_corners[3], split_far), light_view),
      math::transform(lerp(frustum_corners[4], frustum_corners[5], split_near), light_view),
      math::transform(lerp(frustum_corners[4], frustum_corners[5], split_far), light_view),
      math::transform(lerp(frustum_corners[6], frustum_corners[7], split_near), light_view),
      math::transform(lerp(frustum_corners[6], frustum_corners[7], split_far), light_view),
    };

    // Compute cascade bounding sphere center:
    Vec4 center = {};
    for (int j = 0; j < std::size(corners); ++j) {
      center += corners[j];
    }
    center /= float(std::size(corners));

    // Compute cascade bounding sphere radius:
    float radius = 0;
    for (int j = 0; j < std::size(corners); ++j) {
      radius = std::max(radius, glm::length(corners[j] - center));
    }

    // Fit AABB onto bounding sphere:
    auto vRadius = Vec4(radius);
    auto vMin = center - vRadius;
    auto vMax = center + vRadius;

    // Snap cascade to texel grid:
    const auto extent = vMax - vMin;
    const auto texelSize = extent / float(light.shadow_rect.w);
    vMin = glm::floor(vMin / texelSize) * texelSize;
    vMax = glm::floor(vMax / texelSize) * texelSize;
    center = (vMin + vMax) * 0.5f;

    // Extrude bounds to avoid early shadow clipping:
    float ext = abs(center.z - vMin.z);
    ext = std::max(ext, std::min(1500.0f, farPlane) * 0.5f);
    vMin.z = center.z - ext;
    vMax.z = center.z + ext;

    const auto light_projection = glm::ortho(vMin.x, vMax.x, vMin.y, vMax.y, vMax.z, vMin.z); // reversed Z
    const auto view_proj = light_projection * light_view;

    camera_data[cascade].projection_view = view_proj;
    camera_data[cascade].frustum = Frustum::from_matrix(view_proj);
  }
}

void DefaultRenderPipeline::create_cubemap_cameras(std::vector<DefaultRenderPipeline::CameraSH>& camera_data, const Vec3 pos, float near, float far) {
  OX_CHECK_EQ(camera_data.size(), 6);
  constexpr auto fov = 90.0f;
  const auto shadowProj = glm::perspective(glm::radians(fov), 1.0f, near, far);

  camera_data[0].projection_view = shadowProj * glm::lookAt(pos, pos + glm::vec3(1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0));
  camera_data[1].projection_view = shadowProj * glm::lookAt(pos, pos + glm::vec3(-1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0));
  camera_data[2].projection_view = shadowProj * glm::lookAt(pos, pos + glm::vec3(0.0, 1.0, 0.0), glm::vec3(0.0, 0.0, 1.0));
  camera_data[3].projection_view = shadowProj * glm::lookAt(pos, pos + glm::vec3(0.0, -1.0, 0.0), glm::vec3(0.0, 0.0, -1.0));
  camera_data[4].projection_view = shadowProj * glm::lookAt(pos, pos + glm::vec3(0.0, 0.0, 1.0), glm::vec3(0.0, -1.0, 0.0));
  camera_data[5].projection_view = shadowProj * glm::lookAt(pos, pos + glm::vec3(0.0, 0.0, -1.0), glm::vec3(0.0, -1.0, 0.0));

  for (int i = 0; i < 6; i++) {
    camera_data[i].frustum = Frustum::from_matrix(camera_data[i].projection_view);
  }
}

void DefaultRenderPipeline::update_frame_data(vuk::Allocator& allocator) {
  OX_SCOPED_ZONE;
  auto& ctx = allocator.get_context();

  scene_flattened.init();
  scene_flattened.update(mesh_component_list, sprite_component_list);

  render_queue_2d.init();
  render_queue_2d.update();
  render_queue_2d.sort();

  scene_data.num_lights = (uint32)scene_lights.size();
  scene_data.grid_max_distance = RendererCVar::cvar_draw_grid_distance.get();
  scene_data.screen_size = IVec2(Renderer::get_viewport_width(), Renderer::get_viewport_height());
  scene_data.screen_size_rcp = {1.0f / (float)std::max(1u, scene_data.screen_size.x), 1.0f / (float)std::max(1u, scene_data.screen_size.y)};
  scene_data.meshlet_count = scene_flattened.get_meshlet_instances_count();
  scene_data.draw_meshlet_aabbs = RendererCVar::cvar_draw_meshlet_aabbs.get();

  scene_data.indices.albedo_image_index = ALBEDO_IMAGE_INDEX;
  scene_data.indices.normal_image_index = NORMAL_IMAGE_INDEX;
  scene_data.indices.normal_vertex_image_index = NORMAL_VERTEX_IMAGE_INDEX;
  scene_data.indices.depth_image_index = DEPTH_IMAGE_INDEX;
  scene_data.indices.bloom_image_index = BLOOM_IMAGE_INDEX;
  scene_data.indices.sky_transmittance_lut_index = SKY_TRANSMITTANCE_LUT_INDEX;
  scene_data.indices.sky_multiscatter_lut_index = SKY_MULTISCATTER_LUT_INDEX;
  scene_data.indices.velocity_image_index = VELOCITY_IMAGE_INDEX;
  scene_data.indices.emission_image_index = EMISSION_IMAGE_INDEX;
  scene_data.indices.metallic_roughness_ao_image_index = METALROUGHAO_IMAGE_INDEX;
  scene_data.indices.sky_env_map_index = SKY_ENVMAP_INDEX;
  scene_data.indices.shadow_array_index = SHADOW_ATLAS_INDEX;
  scene_data.indices.gtao_buffer_image_index = GTAO_BUFFER_IMAGE_INDEX;
  scene_data.indices.hiz_image_index = HIZ_IMAGE_INDEX;
  scene_data.indices.vis_image_index = VIS_IMAGE_INDEX;
  scene_data.indices.lights_buffer_index = LIGHTS_BUFFER_INDEX;
  scene_data.indices.materials_buffer_index = MATERIALS_BUFFER_INDEX;
  scene_data.indices.mesh_instance_buffer_index = MESH_INSTANCES_BUFFER_INDEX;
  scene_data.indices.entites_buffer_index = ENTITIES_BUFFER_INDEX;
  scene_data.indices.transforms_buffer_index = TRANSFORMS_BUFFER_INDEX;
  scene_data.indices.sprite_materials_buffer_index = SPRITE_MATERIALS_BUFFER_INDEX;

  scene_data.post_processing_data.tonemapper = RendererCVar::cvar_tonemapper.get();
  scene_data.post_processing_data.exposure = RendererCVar::cvar_exposure.get();
  scene_data.post_processing_data.gamma = RendererCVar::cvar_gamma.get();
  scene_data.post_processing_data.enable_bloom = RendererCVar::cvar_bloom_enable.get();
  scene_data.post_processing_data.enable_ssr = RendererCVar::cvar_ssr_enable.get();
  scene_data.post_processing_data.enable_gtao = RendererCVar::cvar_gtao_enable.get();

  {
    OX_SCOPED_ZONE_N("Update 2d vertex data");
    vertex_buffer_2d = *vuk::allocate_cpu_buffer(allocator, sizeof(SpriteGPUData) * 3000);
    std::memcpy(vertex_buffer_2d.mapped_ptr, render_queue_2d.sprite_data.data(), sizeof(SpriteGPUData) * render_queue_2d.sprite_data.size());
  }

  {
    OX_SCOPED_ZONE_N("Update 1st set data");

    auto [scene_buff, scene_buff_fut] = create_cpu_buffer(allocator, std::span(&scene_data, 1));
    const auto& scene_buffer = *scene_buff;

    std::vector<PBRMaterial::Parameters> material_parameters = {};
    material_parameters.reserve(scene_flattened.get_material_count());
    for (auto& mat : scene_flattened.materials) {
      mat->set_id((uint32)material_parameters.size());

      const auto& albedo = mat->get_albedo_texture();
      const auto& normal = mat->get_normal_texture();
      const auto& physical = mat->get_physical_texture();
      const auto& ao = mat->get_ao_texture();
      const auto& emissive = mat->get_emissive_texture();

      if (albedo && albedo->is_valid_id())
        descriptor_set_00->update_sampled_image(10, albedo->get_id(), *albedo->get_view(), vuk::ImageLayout::eReadOnlyOptimalKHR);
      if (normal && normal->is_valid_id())
        descriptor_set_00->update_sampled_image(10, normal->get_id(), *normal->get_view(), vuk::ImageLayout::eReadOnlyOptimalKHR);
      if (physical && physical->is_valid_id())
        descriptor_set_00->update_sampled_image(10, physical->get_id(), *physical->get_view(), vuk::ImageLayout::eReadOnlyOptimalKHR);
      if (ao && ao->is_valid_id())
        descriptor_set_00->update_sampled_image(10, ao->get_id(), *ao->get_view(), vuk::ImageLayout::eReadOnlyOptimalKHR);
      if (emissive && emissive->is_valid_id())
        descriptor_set_00->update_sampled_image(10, emissive->get_id(), *emissive->get_view(), vuk::ImageLayout::eReadOnlyOptimalKHR);

      material_parameters.emplace_back(mat->parameters);
    }

    if (material_parameters.empty())
      material_parameters.emplace_back();

    auto [matBuff, matBufferFut] = create_cpu_buffer(allocator, std::span(material_parameters));
    auto& mat_buffer = *matBuff;

    std::vector<SpriteMaterial::Parameters> sprite_material_parameters = {};
    sprite_material_parameters.reserve(render_queue_2d.materials.size());
    for (uint32 index = 0; auto& mat : render_queue_2d.materials) {
      const auto& albedo = mat->get_albedo_texture();

      if (albedo && albedo->is_valid_id())
        descriptor_set_00->update_sampled_image(10, albedo->get_id(), *albedo->get_view(), vuk::ImageLayout::eReadOnlyOptimalKHR);

      SpriteMaterial::Parameters par = mat->parameters;
      par.uv_offset = sprite_component_list[index].current_uv_offset.value_or(mat->parameters.uv_offset);

      sprite_material_parameters.emplace_back(par);

      index += 1;
    }

    if (sprite_material_parameters.empty())
      sprite_material_parameters.emplace_back();

    auto [sprite_mat_buff, sprite_mat_buff_fut] = create_cpu_buffer(allocator, std::span(sprite_material_parameters));
    auto& sprite_mat_buffer = *sprite_mat_buff;

    light_datas.reserve(scene_lights.size());

    const Vec2 atlas_dim_rcp = Vec2(1.0f / float(shadow_map_atlas.get_extent().width), 1.0f / float(shadow_map_atlas.get_extent().height));

    for (auto& lc : scene_lights) {
      auto& light = light_datas.emplace_back();
      light.position = lc.position;
      light.set_range(lc.range);
      light.set_type((uint32)lc.type);
      light.rotation = lc.rotation;
      light.set_direction(lc.direction);
      light.set_color(float4(lc.color * (lc.type == LightComponent::Directional ? 1.0f : lc.intensity), 1.0f));
      light.set_radius(lc.radius);
      light.set_length(lc.length);

      bool cast_shadows = lc.cast_shadows;

      if (cast_shadows) {
        light.shadow_atlas_mul_add.x = lc.shadow_rect.w * atlas_dim_rcp.x;
        light.shadow_atlas_mul_add.y = lc.shadow_rect.h * atlas_dim_rcp.y;
        light.shadow_atlas_mul_add.z = lc.shadow_rect.x * atlas_dim_rcp.x;
        light.shadow_atlas_mul_add.w = lc.shadow_rect.y * atlas_dim_rcp.y;
      }

      switch (lc.type) {
        case LightComponent::LightType::Directional: {
          light.set_shadow_cascade_count((uint32)lc.cascade_distances.size());
        } break;
        case LightComponent::LightType::Point: {
          if (cast_shadows) {
            constexpr float far_z = 0.1f;
            const float near_z = std::max(1.0f, lc.range);
            const float f_range = far_z / (far_z - near_z);
            const float cubemap_depth_remap_near = f_range;
            const float cubemap_depth_remap_far = -f_range * near_z;
            light.set_cube_remap_near(cubemap_depth_remap_near);
            light.set_cube_remap_far(cubemap_depth_remap_far);
          }
        } break;
        case LightComponent::LightType::Spot: {
          const float outer_cone_angle = lc.outer_cone_angle;
          const float inner_cone_angle = std::min(lc.inner_cone_angle, outer_cone_angle);
          const float outer_cone_angle_cos = std::cos(outer_cone_angle);
          const float inner_cone_angle_cos = std::cos(inner_cone_angle);

          // https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_lights_punctual#inner-and-outer-cone-angles
          const float light_angle_scale = 1.0f / std::max(0.001f, inner_cone_angle_cos - outer_cone_angle_cos);
          const float lightAngleOffset = -outer_cone_angle_cos * light_angle_scale;

          light.set_cone_angle_cos(outer_cone_angle_cos);
          light.set_angle_scale(light_angle_scale);
          light.set_angle_offset(lightAngleOffset);
        } break;
      }
    }

    std::vector<ShaderEntity> shader_entities = {};

    for (uint32_t light_index = 0; light_index < light_datas.size(); ++light_index) {
      auto& light = light_datas[light_index];
      const auto& lc = scene_lights[light_index];

      if (lc.cast_shadows) {
        switch (lc.type) {
          case LightComponent::Directional: {
            auto cascade_count = (uint32)lc.cascade_distances.size();
            auto sh_cameras = std::vector<CameraSH>(cascade_count);
            create_dir_light_cameras(lc, *current_camera, sh_cameras, cascade_count);

            light.matrix_index = (uint32)shader_entities.size();
            for (uint32 cascade = 0; cascade < cascade_count; ++cascade) {
              shader_entities.emplace_back(sh_cameras[cascade].projection_view);
            }
            break;
          }
          case LightComponent::Point: {
            break;
          }
          case LightComponent::Spot:
// TODO:
#if 0
          auto sh_camera = create_spot_light_camera(lc, *current_camera);
          light.matrix_index = (uint32_t)shader_entities.size();
          shader_entities.emplace_back(sh_camera.projection_view);
#endif
            break;
        }
      }
    }

    if (shader_entities.empty())
      shader_entities.emplace_back();

    auto [seBuff, seFut] = create_cpu_buffer(allocator, std::span(shader_entities));
    const auto& shader_entities_buffer = *seBuff;

    if (light_datas.empty())
      light_datas.emplace_back();

    auto [lights_buff, lights_buff_fut] = create_cpu_buffer(allocator, std::span(light_datas));
    const auto& lights_buffer = *lights_buff;

    descriptor_set_00->update_storage_buffer(0, 0, scene_buffer);
    descriptor_set_00->update_storage_buffer(1, LIGHTS_BUFFER_INDEX, lights_buffer);
    descriptor_set_00->update_storage_buffer(1, MATERIALS_BUFFER_INDEX, mat_buffer);
    descriptor_set_00->update_storage_buffer(1, ENTITIES_BUFFER_INDEX, shader_entities_buffer);
    descriptor_set_00->update_storage_buffer(1, SPRITE_MATERIALS_BUFFER_INDEX, sprite_mat_buffer);

    auto [transBuff, transfBuffFut] = create_cpu_buffer(allocator, std::span(scene_flattened.transforms));
    transforms_buffer = *transBuff;
    descriptor_set_00->update_storage_buffer(1, TRANSFORMS_BUFFER_INDEX, transforms_buffer);

    debug_aabb_buffer = *vuk::allocate_cpu_buffer(allocator, sizeof(vuk::DrawIndirectCommand) + sizeof(DebugAabb) * MAX_AABB_COUNT);
    uint32 vert_count = 14;
    std::memcpy(debug_aabb_buffer.mapped_ptr, &vert_count, sizeof(uint32));
    descriptor_set_00->update_storage_buffer(2, DEBUG_AABB_INDEX, debug_aabb_buffer);

    // scene textures
    descriptor_set_00->update_sampled_image(3, ALBEDO_IMAGE_INDEX, *albedo_texture.get_view(), vuk::ImageLayout::eReadOnlyOptimalKHR);
    descriptor_set_00->update_sampled_image(3, NORMAL_IMAGE_INDEX, *normal_texture.get_view(), vuk::ImageLayout::eReadOnlyOptimalKHR);
    descriptor_set_00->update_sampled_image(3, NORMAL_VERTEX_IMAGE_INDEX, *normal_vertex_texture.get_view(), vuk::ImageLayout::eReadOnlyOptimalKHR);
    descriptor_set_00->update_sampled_image(3, DEPTH_IMAGE_INDEX, *depth_texture->get_view(), vuk::ImageLayout::eReadOnlyOptimalKHR);
    descriptor_set_00->update_sampled_image(3, SHADOW_ATLAS_INDEX, *shadow_map_atlas.get_view(), vuk::ImageLayout::eReadOnlyOptimalKHR);
    descriptor_set_00->update_sampled_image(3, SKY_TRANSMITTANCE_LUT_INDEX, *sky_transmittance_lut.get_view(), vuk::ImageLayout::eReadOnlyOptimalKHR);
    descriptor_set_00->update_sampled_image(3, SKY_MULTISCATTER_LUT_INDEX, *sky_multiscatter_lut.get_view(), vuk::ImageLayout::eReadOnlyOptimalKHR);
    descriptor_set_00->update_sampled_image(3, VELOCITY_IMAGE_INDEX, *velocity_texture.get_view(), vuk::ImageLayout::eReadOnlyOptimalKHR);
    descriptor_set_00->update_sampled_image(3,
                                            METALROUGHAO_IMAGE_INDEX,
                                            *metallic_roughness_texture.get_view(),
                                            vuk::ImageLayout::eReadOnlyOptimalKHR);
    descriptor_set_00->update_sampled_image(3, EMISSION_IMAGE_INDEX, *emission_texture.get_view(), vuk::ImageLayout::eReadOnlyOptimalKHR);

    // scene texture float
    descriptor_set_00->update_sampled_image(4, HIZ_IMAGE_INDEX, *hiz_texture.get_view(), vuk::ImageLayout::eReadOnlyOptimalKHR);

    // scene uint texture array
    descriptor_set_00->update_sampled_image(5, GTAO_BUFFER_IMAGE_INDEX, *gtao_final_texture.get_view(), vuk::ImageLayout::eReadOnlyOptimalKHR);
    descriptor_set_00->update_sampled_image(5, VIS_IMAGE_INDEX, *visibility_texture.get_view(), vuk::ImageLayout::eReadOnlyOptimalKHR);

    // scene cubemap texture array
    descriptor_set_00->update_sampled_image(6, SKY_ENVMAP_INDEX, *sky_envmap_texture.get_view(), vuk::ImageLayout::eReadOnlyOptimalKHR);

    // scene Read/Write float4 textures
    descriptor_set_00->update_storage_image(8, SKY_TRANSMITTANCE_LUT_INDEX, *sky_transmittance_lut.get_view());
    descriptor_set_00->update_storage_image(8, SKY_MULTISCATTER_LUT_INDEX, *sky_multiscatter_lut.get_view());

    // scene Read/Write float textures
    descriptor_set_00->update_storage_image(9, HIZ_IMAGE_INDEX, *hiz_texture.get_view());

    descriptor_set_00->commit(ctx);
  }
  {
    OX_SCOPED_ZONE_N("Update 2nd set data");

    constexpr auto MESHLET_DATA_BUFFERS_INDEX = 0;
    constexpr auto INDEX_BUFFER_INDEX = 1;
    constexpr auto VERTEX_BUFFER_INDEX = 2;
    constexpr auto PRIMITIVES_BUFFER_INDEX = 3;
    constexpr auto MESHLET_INSTANCE_BUFFERS_INDEX = 5;

    constexpr auto VISIBLE_MESHLETS_BUFFER_INDEX = 0;
    constexpr auto CULL_TRIANGLES_DISPATCH_PARAMS_BUFFERS_INDEX = 1;
    constexpr auto INDIRECT_COMMAND_BUFFER_INDEX = 2;
    constexpr auto INSTANCED_INDEX_BUFFER_INDEX = 3;

    constexpr auto READ_ONLY = 0;
    constexpr auto READ_WRITE = 1;

    auto [meshletBuff, meshletBuffFut] = create_cpu_buffer(allocator, std::span(scene_flattened.meshlets));
    const auto& meshlet_data_buffer = *meshletBuff;
    descriptor_set_02->update_storage_buffer(READ_ONLY, MESHLET_DATA_BUFFERS_INDEX, meshlet_data_buffer);

    auto [meshlet_instances_buff, meshlet_instances_buff_fut] = create_cpu_buffer(allocator, std::span(scene_flattened.meshlet_instances));
    const auto& meshlet_instances_buffer = *meshlet_instances_buff;
    descriptor_set_02->update_storage_buffer(READ_ONLY, MESHLET_INSTANCE_BUFFERS_INDEX, meshlet_instances_buffer);

    visible_meshlets_buffer = *allocate_gpu_buffer(allocator, scene_flattened.get_meshlet_instances_count() * sizeof(uint32_t));
    descriptor_set_02->update_storage_buffer(READ_WRITE, VISIBLE_MESHLETS_BUFFER_INDEX, visible_meshlets_buffer);

    struct DispatchParams {
      uint32 groupCountX;
      uint32 groupCountY;
      uint32 groupCountZ;
    };

    DispatchParams params{0, 1, 1};
    auto [dispatchBuff, dispatchBuffFut] = create_cpu_buffer(allocator, std::span(&params, 1));
    cull_triangles_dispatch_params_buffer = *dispatchBuff;
    descriptor_set_02->update_storage_buffer(READ_WRITE, CULL_TRIANGLES_DISPATCH_PARAMS_BUFFERS_INDEX, cull_triangles_dispatch_params_buffer);

    constexpr auto draw_command = vuk::DrawIndexedIndirectCommand{
      .indexCount = 0,
      .instanceCount = 1,
      .firstIndex = 0,
      .vertexOffset = 0,
      .firstInstance = 0,
    };

    auto [indirectBuff, indirectBuffFut] = create_cpu_buffer(allocator, std::span(&draw_command, 1));
    indirect_commands_buffer = *indirectBuff;
    descriptor_set_02->update_storage_buffer(READ_WRITE, INDIRECT_COMMAND_BUFFER_INDEX, indirect_commands_buffer);

    auto [indicesBuff, indicesBuffFut] = create_cpu_buffer(allocator, std::span(scene_flattened.indices)); // static
    index_buffer = *indicesBuff;
    descriptor_set_02->update_storage_buffer(READ_ONLY, INDEX_BUFFER_INDEX, *indicesBuff);

    auto [vertBuff, vertBuffFut] = create_cpu_buffer(allocator, std::span(scene_flattened.vertices)); // static
    vertex_buffer = *vertBuff;
    descriptor_set_02->update_storage_buffer(READ_ONLY, VERTEX_BUFFER_INDEX, vertex_buffer);

    auto [primsBuff, primsBuffFut] = create_cpu_buffer(allocator, std::span(scene_flattened.primitives)); // static
    primitives_buffer = *primsBuff;
    descriptor_set_02->update_storage_buffer(READ_ONLY, PRIMITIVES_BUFFER_INDEX, primitives_buffer);

    constexpr auto max_meshlet_primitives = 64;
    instanced_index_buffer = *allocate_gpu_buffer(allocator,
                                                  scene_flattened.get_meshlet_instances_count() * max_meshlet_primitives * 3 * sizeof(uint32));
    descriptor_set_02->update_storage_buffer(READ_WRITE, INSTANCED_INDEX_BUFFER_INDEX, instanced_index_buffer);

    descriptor_set_02->commit(ctx);
  }
}

void DefaultRenderPipeline::create_static_resources() {
  OX_SCOPED_ZONE;

  constexpr auto transmittance_lut_size = vuk::Extent3D{256, 64, 1};
  sky_transmittance_lut.create_texture(transmittance_lut_size, vuk::Format::eR32G32B32A32Sfloat, Preset::eSTT2DUnmipped);

  constexpr auto multi_scatter_lut_size = vuk::Extent3D{32, 32, 1};
  sky_multiscatter_lut.create_texture(multi_scatter_lut_size, vuk::Format::eR32G32B32A32Sfloat, Preset::eSTT2DUnmipped);

  constexpr auto shadow_size = vuk::Extent3D{1u, 1u, 1};
  const auto ia = vuk::ImageAttachment::from_preset(Preset::eRTT2DUnmipped, vuk::Format::eD32Sfloat, shadow_size, vuk::Samples::e1);
  shadow_map_atlas.create_texture(ia);
  shadow_map_atlas_transparent.create_texture(ia);

  constexpr auto envmap_size = vuk::Extent3D{512, 512, 1};
  auto ia2 = vuk::ImageAttachment::from_preset(Preset::eRTTCube, vuk::Format::eR16G16B16A16Sfloat, envmap_size, vuk::Samples::e1);
  ia2.usage |= vuk::ImageUsageFlagBits::eStorage;
  sky_envmap_texture.create_texture(ia2);
}

void DefaultRenderPipeline::create_dynamic_textures(const vuk::Extent3D& ext) {
  if (fsr.get_render_res() != ext)
    fsr.create_fs2_resources(ext, ext / 1.5f);

  if (color_texture.get_extent() != ext) { // since they all should be sized the same
    color_texture.create_texture(ext, vuk::Format::eR32G32B32A32Sfloat, Preset::eRTT2DUnmipped);
    albedo_texture.create_texture(ext, vuk::Format::eR8G8B8A8Srgb, Preset::eRTT2DUnmipped);

    depth_texture = create_unique<Texture>();
    depth_texture->create_texture(ext, vuk::Format::eD32Sfloat, Preset::eRTT2DUnmipped);
    depth_texture_prev = create_unique<Texture>();
    depth_texture_prev->create_texture(ext, vuk::Format::eD32Sfloat, Preset::eRTT2DUnmipped);

    const auto hiz_ext = vuk::Extent3D{math::previous_power2(ext.width), math::previous_power2(ext.height), 1u};
    hiz_texture.create_texture(hiz_ext, vuk::Format::eR32Sfloat, Preset::eSTT2D);

    material_depth_texture.create_texture(ext, vuk::Format::eD32Sfloat, Preset::eRTT2DUnmipped);
    normal_texture.create_texture(ext, vuk::Format::eR16G16B16A16Snorm, Preset::eRTT2DUnmipped);
    normal_vertex_texture.create_texture(ext, vuk::Format::eR16G16Snorm, Preset::eRTT2DUnmipped);
    velocity_texture.create_texture(ext, vuk::Format::eR16G16Sfloat, Preset::eRTT2DUnmipped);
    visibility_texture.create_texture(ext, vuk::Format::eR32Uint, Preset::eRTT2DUnmipped);
    emission_texture.create_texture(ext, vuk::Format::eB10G11R11UfloatPack32, Preset::eRTT2DUnmipped);
    metallic_roughness_texture.create_texture(ext, vuk::Format::eR8G8B8A8Unorm, Preset::eRTT2DUnmipped);
    gtao_final_texture.create_texture(ext, vuk::Format::eR8Uint, Preset::eSTT2DUnmipped);
    ssr_texture.create_texture(ext, vuk::Format::eR32G32B32A32Sfloat, Preset::eRTT2DUnmipped);

    resized = true;
  }

  // Shadow atlas packing:
  {
    OX_SCOPED_ZONE_N("Shadow atlas packing");
    thread_local RectPacker::State packer;
    float iterative_scaling = 1;

    while (iterative_scaling > 0.03f) {
      packer.clear();
      for (uint32_t lightIndex = 0; lightIndex < scene_lights.size(); lightIndex++) {
        LightComponent& light = scene_lights[lightIndex];
        light.shadow_rect = {};
        if (!light.cast_shadows)
          continue;

        const float dist = distance(current_camera->get_position(), light.position);
        const float range = light.range;
        const float amount = std::min(1.0f, range / std::max(0.001f, dist)) * iterative_scaling;

        constexpr int max_shadow_resolution_2D = 1024;
        constexpr int max_shadow_resolution_cube = 256;

        RectPacker::Rect rect = {};
        rect.id = int(lightIndex);
        switch (light.type) {
          case LightComponent::Directional:
            if (light.shadow_map_res > 0) {
              rect.w = light.shadow_map_res * int(light.cascade_distances.size());
              rect.h = light.shadow_map_res;
            } else {
              rect.w = int(max_shadow_resolution_2D * iterative_scaling) * int(light.cascade_distances.size());
              rect.h = int(max_shadow_resolution_2D * iterative_scaling);
            }
            break;
          case LightComponent::Spot:
            if (light.shadow_map_res > 0) {
              rect.w = int(light.shadow_map_res);
              rect.h = int(light.shadow_map_res);
            } else {
              rect.w = int(max_shadow_resolution_2D * amount);
              rect.h = int(max_shadow_resolution_2D * amount);
            }
            break;
          case LightComponent::Point:
            if (light.shadow_map_res > 0) {
              rect.w = int(light.shadow_map_res) * 6;
              rect.h = int(light.shadow_map_res);
            } else {
              rect.w = int(max_shadow_resolution_cube * amount) * 6;
              rect.h = int(max_shadow_resolution_cube * amount);
            }
            break;
        }
        if (rect.w > 8 && rect.h > 8) {
          packer.add_rect(rect);
        }
      }
      if (!packer.rects.empty()) {
        if (packer.pack(8192)) {
          for (const auto& rect : packer.rects) {
            if (rect.id == -1) {
              continue;
            }
            const uint32_t light_index = uint32_t(rect.id);
            LightComponent& light = scene_lights[light_index];
            if (rect.was_packed) {
              light.shadow_rect = rect;

              // Remove slice multipliers from rect:
              switch (light.type) {
                case LightComponent::Directional: light.shadow_rect.w /= int(light.cascade_distances.size()); break;
                case LightComponent::Point      : light.shadow_rect.w /= 6; break;
                case LightComponent::Spot       : break;
              }
            } else {
              light.direction = {};
            }
          }

          if ((int)shadow_map_atlas.get_extent().width < packer.width || (int)shadow_map_atlas.get_extent().height < packer.height) {
            const auto shadow_size = vuk::Extent3D{(uint32_t)packer.width, (uint32_t)packer.height, 1};

            auto ia = shadow_map_atlas.as_attachment();
            ia.extent = shadow_size;
            shadow_map_atlas.create_texture(ia);
            shadow_map_atlas_transparent.create_texture(ia);

            scene_data.shadow_atlas_res = UVec2(shadow_map_atlas.get_extent().width, shadow_map_atlas.get_extent().height);
          }

          break;
        }

        iterative_scaling *= 0.5f;
      } else {
        iterative_scaling = 0.0; // PE: fix - endless loop if some lights do not have shadows.
      }
    }
  }
}

void DefaultRenderPipeline::create_descriptor_sets(vuk::Allocator& allocator) {
  auto& ctx = allocator.get_context();
  descriptor_set_00 = ctx.create_persistent_descriptorset(allocator, *ctx.get_named_pipeline("shading_pipeline"), 0, 64);

  const vuk::Sampler linear_sampler_clamped = ctx.acquire_sampler(vuk::LinearSamplerClamped, ctx.get_frame_count());
  const vuk::Sampler linear_sampler_repeated = ctx.acquire_sampler(vuk::LinearSamplerRepeated, ctx.get_frame_count());
  const vuk::Sampler linear_sampler_repeated_anisotropy = ctx.acquire_sampler(vuk::LinearSamplerRepeatedAnisotropy, ctx.get_frame_count());
  const vuk::Sampler nearest_sampler_clamped = ctx.acquire_sampler(vuk::NearestSamplerClamped, ctx.get_frame_count());
  const vuk::Sampler nearest_sampler_repeated = ctx.acquire_sampler(vuk::NearestSamplerRepeated, ctx.get_frame_count());
  const vuk::Sampler cmp_depth_sampler = ctx.acquire_sampler(vuk::CmpDepthSampler, ctx.get_frame_count());
  const vuk::Sampler hiz_sampler = ctx.acquire_sampler(hiz_sampler_ci, ctx.get_frame_count());
  descriptor_set_00->update_sampler(11, 0, linear_sampler_clamped);
  descriptor_set_00->update_sampler(11, 1, linear_sampler_repeated);
  descriptor_set_00->update_sampler(11, 2, linear_sampler_repeated_anisotropy);
  descriptor_set_00->update_sampler(11, 3, nearest_sampler_clamped);
  descriptor_set_00->update_sampler(11, 4, nearest_sampler_repeated);
  descriptor_set_00->update_sampler(11, 5, hiz_sampler);
  descriptor_set_00->update_sampler(12, 0, cmp_depth_sampler);

  descriptor_set_02 = ctx.create_persistent_descriptorset(allocator, *ctx.get_named_pipeline("cull_meshlets_pipeline"), 2, 64);
}

void DefaultRenderPipeline::run_static_passes(vuk::Allocator& allocator) {
  auto* compiler = get_compiler();

  auto transmittance_fut = sky_transmittance_pass();
  auto multiscatter_fut = sky_multiscatter_pass(transmittance_fut);
  multiscatter_fut.wait(allocator, *compiler);
}

void DefaultRenderPipeline::on_dispatcher_events(EventDispatcher& dispatcher) {
  dispatcher.sink<SkyboxLoadEvent>().connect<&DefaultRenderPipeline::update_skybox>(*this);
}

void DefaultRenderPipeline::submit_mesh_component(const MeshComponent& render_object) {
  OX_SCOPED_ZONE;

  if (!current_camera)
    return;

  mesh_component_list.emplace_back(render_object);
}

void DefaultRenderPipeline::submit_light(const LightComponent& light) {
  OX_SCOPED_ZONE;
  auto& lc = scene_lights.emplace_back(light);
  if (light.type == LightComponent::LightType::Directional)
    dir_light_data = &lc;
}

void DefaultRenderPipeline::submit_sprite(const SpriteComponent& sprite) {
  OX_SCOPED_ZONE;
  sprite_component_list.emplace_back(sprite);

  const auto distance = glm::distance(float3(0.f, 0.f, current_camera->get_position().z), float3(0.f, 0.f, sprite.get_position().z));
  render_queue_2d.add(sprite, distance);
}

void DefaultRenderPipeline::submit_camera(Camera* camera) {
  OX_SCOPED_ZONE;

  if ((bool)RendererCVar::cvar_freeze_culling_frustum.get() && !saved_camera) {
    saved_camera = true;
    frozen_camera = *current_camera;
  } else if (!(bool)RendererCVar::cvar_freeze_culling_frustum.get() && saved_camera) {
    saved_camera = false;
  }

  if ((bool)RendererCVar::cvar_freeze_culling_frustum.get() && (bool)RendererCVar::cvar_draw_camera_frustum.get()) {
    const auto proj = frozen_camera.get_projection_matrix() * frozen_camera.get_view_matrix();
    DebugRenderer::draw_frustum(proj, float4(0, 1, 0, 1), 1.0f, 0.0f); // reversed-z
  }

  current_camera = camera;
}

void DefaultRenderPipeline::shutdown() {}

vuk::Value<vuk::ImageAttachment> DefaultRenderPipeline::on_render(vuk::Allocator& frame_allocator,
                                                                  vuk::Value<vuk::ImageAttachment> target,
                                                                  vuk::Extent3D ext) {
  OX_SCOPED_ZONE;
  if (!current_camera) {
    OX_LOG_ERROR("No camera is set for rendering!");
    // set a temporary one
    if (!default_camera)
      default_camera = create_shared<Camera>();
    current_camera = default_camera.get();
  }

  auto& vk_context = App::get_vkcontext();

  Vec3 sun_direction = {0, 1, 0};
  Vec3 sun_color = {};

  if (dir_light_data) {
    sun_direction = dir_light_data->direction;
    sun_color = dir_light_data->color * dir_light_data->intensity;
  }

  scene_data.sun_direction = sun_direction;
  scene_data.sun_color = Vec4(sun_color, 1.0f);

  create_dynamic_textures(ext);

  std::swap(depth_texture, depth_texture_prev);

  update_frame_data(frame_allocator);

  auto hiz_image = vuk::make_pass("transition", [](vuk::CommandBuffer&, VUK_IA(vuk::eComputeRW) output) {
    return output;
  })(vuk::acquire_ia("hiz_image", hiz_texture.as_attachment(), first_pass || resized ? vuk::eNone : vuk::eFragmentSampled | vuk::eComputeSampled));

  if (first_pass || resized) {
    run_static_passes(*vk_context.superframe_allocator);
    hiz_image = clear_image(hiz_image, vuk::Black<float>);
    resized = false;
  }

  auto vis_meshlets_buf = vuk::declare_buf("visible_meshlets_buffer", visible_meshlets_buffer);
  auto cull_triangles_buf = vuk::declare_buf("dispatch_params_buffer", cull_triangles_dispatch_params_buffer);
  auto instanced_idx_buf = vuk::declare_buf("instanced_index_buffer", instanced_index_buffer);
  auto indirect_commands_buff = vuk::declare_buf("meshlet_indirect_commands_buffer", indirect_commands_buffer);
  auto debug_aabb_buff = vuk::declare_buf("debug_aabb_buffer", debug_aabb_buffer);

  auto [vis_meshlets_buff_output,
        triangles_dis_buffer_output,
        debug_buffer_output] = vuk::make_pass("cull_meshlets",
                                              [this](vuk::CommandBuffer& command_buffer,
                                                     VUK_IA(vuk::eComputeSampled) _hiz,
                                                     VUK_BA(vuk::eComputeRW) _vis_meshlets_buff,
                                                     VUK_BA(vuk::eComputeRW) _triangles_dispatch_buffer,
                                                     VUK_BA(vuk::eComputeRW) _debug_buffer) {
    command_buffer.bind_compute_pipeline("cull_meshlets_pipeline").bind_persistent(0, *descriptor_set_00).bind_persistent(2, *descriptor_set_02);

    camera_cb.camera_data[0] = get_main_camera_data((bool)RendererCVar::cvar_freeze_culling_frustum.get());
    bind_camera_buffer(command_buffer);

    command_buffer.dispatch((scene_flattened.get_meshlet_instances_count() + 128 - 1) / 128);

    return std::make_tuple(_vis_meshlets_buff, _triangles_dispatch_buffer, _debug_buffer);
  })(hiz_image, vis_meshlets_buf, cull_triangles_buf, debug_aabb_buff);

  auto [instanced_index_buff, indirect_buff_output] = vuk::make_pass("cull_triangles",
                                                                     [this](vuk::CommandBuffer& command_buffer,
                                                                            VUK_BA(vuk::eComputeRead) meshlets,
                                                                            VUK_BA(vuk::eIndirectRead) _triangles_dispatch_buffer,
                                                                            VUK_BA(vuk::eComputeRW) _index_buffer,
                                                                            VUK_BA(vuk::eComputeRW) _indirect_buffer) {
    command_buffer.bind_compute_pipeline("cull_triangles_pipeline").bind_persistent(0, *descriptor_set_00).bind_persistent(2, *descriptor_set_02);

    camera_cb.camera_data[0] = get_main_camera_data((bool)RendererCVar::cvar_freeze_culling_frustum.get());
    bind_camera_buffer(command_buffer);

    command_buffer.dispatch_indirect(_triangles_dispatch_buffer);

    return std::make_tuple(_index_buffer, _indirect_buffer);
  })(vis_meshlets_buff_output, triangles_dis_buffer_output, instanced_idx_buf, indirect_commands_buff);

  auto depth = vuk::clear_image(vuk::declare_ia("depth_image", depth_texture->as_attachment()), vuk::DepthZero);
  auto vis_image = vuk::clear_image(vuk::acquire_ia("visibility_image", visibility_texture.as_attachment(), vuk::eNone), vuk::Black<float>);

  auto [vis_image_output, depth_output] = vuk::make_pass("main_vis_buffer_pass",
                                                         [this](vuk::CommandBuffer& command_buffer,
                                                                VUK_IA(vuk::eColorRW) _vis_buffer,
                                                                VUK_IA(vuk::eDepthStencilRW) _depth,
                                                                VUK_BA(vuk::eIndexRead) instanced_idx_buff,
                                                                VUK_BA(vuk::eIndirectRead) indirect_commands_buffer) {
    command_buffer.bind_graphics_pipeline("vis_buffer_pipeline")
      .set_dynamic_state(vuk::DynamicStateFlagBits::eScissor | vuk::DynamicStateFlagBits::eViewport)
      .set_viewport(0, vuk::Rect2D::framebuffer())
      .set_scissor(0, vuk::Rect2D::framebuffer())
      .broadcast_color_blend(vuk::BlendPreset::eOff)
      .set_depth_stencil({.depthTestEnable = true, .depthWriteEnable = true, .depthCompareOp = vuk::CompareOp::eGreater})
      .set_rasterization({.cullMode = vuk::CullModeFlagBits::eNone})
      .bind_persistent(0, *descriptor_set_00)
      .bind_persistent(2, *descriptor_set_02)
      .bind_index_buffer(instanced_idx_buff, vuk::IndexType::eUint32);

    camera_cb.camera_data[0] = get_main_camera_data();
    bind_camera_buffer(command_buffer);

    command_buffer.draw_indexed_indirect(1, indirect_commands_buffer);

    return std::make_tuple(_vis_buffer, _depth);
  })(vis_image, depth, instanced_index_buff, indirect_buff_output);

  auto hiz_image_copied = vuk::make_pass("depth_copy_pass",
                                         [this](vuk::CommandBuffer& command_buffer, VUK_IA(vuk::eComputeSampled) src, VUK_IA(vuk::eComputeRW) dst) {
    command_buffer.bind_compute_pipeline("depth_copy_pipeline")
      .bind_persistent(0, *descriptor_set_00)
      .dispatch((dst->extent.width + 15) / 16, (dst->extent.height + 15) / 16, 1);

    return dst;
  })(depth_output, hiz_image);

  auto depth_hiz_output = hiz_spd.dispatch("hiz_pass", frame_allocator, hiz_image_copied);

  auto material_depth = vuk::clear_image(vuk::declare_ia("material_depth_image", material_depth_texture.as_attachment()), vuk::DepthZero);

  // depth_hiz_output is not actually used in this pass, but passed here so it runs.
  auto material_depth_output = vuk::make_pass("material_vis_buffer_pass",
                                              [this](vuk::CommandBuffer& command_buffer,
                                                     VUK_IA(vuk::eDepthStencilRW) material_depth,
                                                     VUK_IA(vuk::eFragmentSampled) _vis_buffer,
                                                     VUK_IA(vuk::eFragmentSampled) _hiz) {
    command_buffer.bind_graphics_pipeline("material_vis_buffer_pipeline")
      .set_dynamic_state(vuk::DynamicStateFlagBits::eScissor | vuk::DynamicStateFlagBits::eViewport)
      .set_viewport(0, vuk::Rect2D::framebuffer())
      .set_scissor(0, vuk::Rect2D::framebuffer())
      .broadcast_color_blend(vuk::BlendPreset::eOff)
      .set_depth_stencil({.depthTestEnable = true, .depthWriteEnable = true, .depthCompareOp = vuk::CompareOp::eAlways})
      .set_rasterization({.cullMode = vuk::CullModeFlagBits::eNone})
      .bind_persistent(0, *descriptor_set_00)
      .bind_persistent(2, *descriptor_set_02)
      .draw(3, 1, 0, 0);

    return material_depth;
  })(material_depth, vis_image_output, depth_hiz_output);

  auto albedo = vuk::clear_image(vuk::declare_ia("albedo_texture", albedo_texture.as_attachment()), vuk::Black<float>);
  auto normal = vuk::clear_image(vuk::declare_ia("normal_texture", normal_texture.as_attachment()), vuk::Black<float>);
  auto normal_vertex = vuk::clear_image(vuk::declare_ia("normal_vertex_texture", normal_vertex_texture.as_attachment()), vuk::Black<float>);
  auto metallic_roughness = vuk::clear_image(vuk::declare_ia("metallic_roughness_texture", metallic_roughness_texture.as_attachment()),
                                             vuk::Black<float>);
  auto velocity = vuk::clear_image(vuk::declare_ia("velocity_texture", velocity_texture.as_attachment()), vuk::Black<float>);
  auto emission = vuk::clear_image(vuk::declare_ia("emission_texture", emission_texture.as_attachment()), vuk::Black<float>);
  auto [albedo_output,
        normal_output,
        normal_vertex_output,
        metallic_roughness_output,
        velocity_output,
        emission_output] = vuk::make_pass("resolve_vis_buffer_pass",
                                          [this](vuk::CommandBuffer& command_buffer,
                                                 VUK_IA(vuk::eDepthStencilRead) _depth,
                                                 VUK_IA(vuk::eColorRW) _albedo,
                                                 VUK_IA(vuk::eColorRW) _normal,
                                                 VUK_IA(vuk::eColorRW) _normal_vertex,
                                                 VUK_IA(vuk::eColorRW) _metallic_roughness,
                                                 VUK_IA(vuk::eColorRW) _velocity,
                                                 VUK_IA(vuk::eColorRW) _emission,
                                                 VUK_IA(vuk::eFragmentSampled) _vis) {
    command_buffer.bind_graphics_pipeline("resolve_vis_buffer_pipeline")
      .set_dynamic_state(vuk::DynamicStateFlagBits::eScissor | vuk::DynamicStateFlagBits::eViewport)
      .set_viewport(0, vuk::Rect2D::framebuffer())
      .set_scissor(0, vuk::Rect2D::framebuffer())
      .broadcast_color_blend(vuk::BlendPreset::eOff)
      .set_depth_stencil({.depthTestEnable = true, .depthWriteEnable = false, .depthCompareOp = vuk::CompareOp::eEqual})
      .set_rasterization({.cullMode = vuk::CullModeFlagBits::eNone})
      .bind_persistent(0, *descriptor_set_00)
      .bind_persistent(2, *descriptor_set_02);

    camera_cb.camera_data[0] = get_main_camera_data();
    bind_camera_buffer(command_buffer);

    for (const auto& material : scene_flattened.materials) {
      command_buffer.draw(3, 1, 0, material->get_id());
    }

    return std::make_tuple(_albedo, _normal, _normal_vertex, _metallic_roughness, _velocity, _emission);
  })(material_depth_output, albedo, normal, normal_vertex, metallic_roughness, velocity, emission, vis_image_output);

  auto envmap_image = vuk::clear_image(vuk::declare_ia("sky_envmap_image", sky_envmap_texture.as_attachment()), vuk::Black<float>);
  auto sky_envmap_output = dir_light_data ? sky_envmap_pass(envmap_image) : envmap_image;

  auto color_image = vuk::clear_image(vuk::declare_ia("color_image", color_texture.as_attachment()), vuk::Black<float>);
  // TODO: pass GTAO
  auto color_output = vuk::make_pass("shading_pass",
                                     [this](vuk::CommandBuffer& command_buffer,
                                            VUK_IA(vuk::eColorRW) _out,
                                            VUK_IA(vuk::eFragmentSampled) _albedo,
                                            VUK_IA(vuk::eFragmentSampled) _normal,
                                            VUK_IA(vuk::eFragmentSampled) _normal_vertex,
                                            VUK_IA(vuk::eFragmentSampled) _metallic_roughness,
                                            VUK_IA(vuk::eFragmentSampled) _velocity,
                                            VUK_IA(vuk::eFragmentSampled) _emission,
                                            VUK_IA(vuk::eFragmentSampled) _tranmisttance_lut,
                                            VUK_IA(vuk::eFragmentSampled) _multiscatter_lut,
                                            VUK_IA(vuk::eFragmentSampled) _envmap) {
    camera_cb.camera_data[0] = get_main_camera_data();
    bind_camera_buffer(command_buffer);

    command_buffer.bind_persistent(0, *descriptor_set_00)
      .set_dynamic_state(vuk::DynamicStateFlagBits::eScissor | vuk::DynamicStateFlagBits::eViewport)
      .set_viewport(0, vuk::Rect2D::framebuffer())
      .set_scissor(0, vuk::Rect2D::framebuffer())
      .broadcast_color_blend(vuk::BlendPreset::eOff)
      .set_depth_stencil({.depthTestEnable = false, .depthWriteEnable = false, .depthCompareOp = vuk::CompareOp::eGreaterOrEqual})
      .set_rasterization({.cullMode = vuk::CullModeFlagBits::eNone})
      .bind_graphics_pipeline("sky_view_final_pipeline")
      .draw(3, 1, 0, 0);

    command_buffer.bind_graphics_pipeline("shading_pipeline")
      .set_dynamic_state(vuk::DynamicStateFlagBits::eScissor | vuk::DynamicStateFlagBits::eViewport)
      .set_viewport(0, vuk::Rect2D::framebuffer())
      .set_scissor(0, vuk::Rect2D::framebuffer())
      .broadcast_color_blend(vuk::BlendPreset::eOff)
      .set_rasterization({.cullMode = vuk::CullModeFlagBits::eNone})
      .bind_persistent(0, *descriptor_set_00);

    camera_cb.camera_data[0] = get_main_camera_data();
    bind_camera_buffer(command_buffer);

    command_buffer.draw(3, 1, 0, 0);
    return _out;
  })(color_image,
     albedo_output,
     normal_output,
     normal_vertex_output,
     metallic_roughness_output,
     velocity_output,
     emission_output,
     vuk::acquire_ia("sky_transmittance_lut", sky_transmittance_lut.as_attachment(), vuk::eFragmentSampled),
     vuk::acquire_ia("sky_multiscatter_lut", sky_multiscatter_lut.as_attachment(), vuk::eFragmentSampled),
     sky_envmap_output);

  auto color_output_w2d = vuk::make_pass("2d_forward_pass",
                                         [this](vuk::CommandBuffer& command_buffer,
                                                VUK_IA(vuk::eColorWrite) target,
                                                VUK_IA(vuk::eDepthStencilRW) depth) {
    auto vertex_pack_2d = vuk::Packed{
      vuk::Format::eR32G32B32A32Sfloat, // 16 row
      vuk::Format::eR32G32B32A32Sfloat, // 16 row
      vuk::Format::eR32G32B32A32Sfloat, // 16 row
      vuk::Format::eR32G32B32A32Sfloat, // 16 row
      vuk::Format::eR32Uint,            // 4 material_id
      vuk::Format::eR32Uint,            // 4 flags
    };

    for (auto& batch : render_queue_2d.batches) {
      if (batch.count < 1)
        continue;

      command_buffer.bind_graphics_pipeline(batch.pipeline_name)
        .set_depth_stencil(vuk::PipelineDepthStencilStateCreateInfo{
          .depthTestEnable = true,
          .depthWriteEnable = false,
          .depthCompareOp = vuk::CompareOp::eGreaterOrEqual,
        })
        .set_dynamic_state(vuk::DynamicStateFlagBits::eScissor | vuk::DynamicStateFlagBits::eViewport)
        .set_viewport(0, vuk::Rect2D::framebuffer())
        .set_scissor(0, vuk::Rect2D::framebuffer())
        .broadcast_color_blend(vuk::BlendPreset::eAlphaBlend)
        .set_rasterization({.cullMode = vuk::CullModeFlagBits::eNone})
        .bind_vertex_buffer(0, vertex_buffer_2d, 0, vertex_pack_2d, vuk::VertexInputRate::eInstance)
        .bind_persistent(0, *descriptor_set_00);

      camera_cb.camera_data[0] = get_main_camera_data((bool)RendererCVar::cvar_freeze_culling_frustum.get());
      bind_camera_buffer(command_buffer);

      command_buffer.draw(6, batch.count, 0, batch.offset);
    }

    return target;
  })(color_output, depth_output);

  auto debug_output = vuk::make_pass("debug_pass",
                                     [this](vuk::CommandBuffer& command_buffer,
                                            VUK_IA(vuk::eDepthStencilRead) _depth,
                                            VUK_IA(vuk::eColorWrite) _output,
                                            VUK_BA(vuk::eIndirectRead) indirect_draw_buffer) {
    command_buffer.bind_graphics_pipeline("debug_aabb_pipeline")
      .set_dynamic_state(vuk::DynamicStateFlagBits::eScissor | vuk::DynamicStateFlagBits::eViewport | vuk::DynamicStateFlagBits::eDepthBias)
      .set_viewport(0, vuk::Rect2D::framebuffer())
      .set_scissor(0, vuk::Rect2D::framebuffer())
      .broadcast_color_blend(vuk::BlendPreset::eAlphaBlend)
      .set_rasterization({.polygonMode = vuk::PolygonMode::eLine,
                          .cullMode = vuk::CullModeFlagBits::eNone,
                          .depthBiasEnable = true,
                          .depthBiasConstantFactor = 50.0f})
      .set_primitive_topology(vuk::PrimitiveTopology::eLineList)
      .set_depth_stencil({.depthTestEnable = true, .depthWriteEnable = false, .depthCompareOp = vuk::CompareOp::eGreater})
      .bind_persistent(0, *descriptor_set_00);

    camera_cb.camera_data[0] = get_main_camera_data();
    bind_camera_buffer(command_buffer);

    command_buffer.draw_indirect(1, indirect_draw_buffer);

    return _output;
  })(depth_output, color_output_w2d, debug_buffer_output);

  auto bloom_output = vuk::clear_image(vuk::declare_ia("bloom_output", vuk::dummy_attachment), vuk::Black<float>);
  if (RendererCVar::cvar_bloom_enable.get()) {
    constexpr uint32_t bloom_mip_count = 8;

    auto bloom_ia = vuk::ImageAttachment{
      .format = vuk::Format::eR32G32B32A32Sfloat,
      .sample_count = vuk::SampleCountFlagBits::e1,
      .level_count = bloom_mip_count,
      .layer_count = 1,
    };
    auto bloom_down_image = vuk::clear_image(vuk::declare_ia("bloom_down_image", bloom_ia), vuk::Black<float>);
    bloom_down_image.same_extent_as(target);

    auto bloom_up_ia = vuk::ImageAttachment{
      .format = vuk::Format::eR32G32B32A32Sfloat,
      .sample_count = vuk::SampleCountFlagBits::e1,
      .level_count = bloom_mip_count - 1,
      .layer_count = 1,
    };
    auto bloom_up_image = vuk::clear_image(vuk::declare_ia("bloom_up_image", bloom_up_ia), vuk::Black<float>);
    bloom_up_image.same_extent_as(target);

    bloom_output = bloom_pass(bloom_down_image, bloom_up_image, color_output_w2d);
  }

  auto final_output = vuk::make_pass("final_pass",
                                     [this](vuk::CommandBuffer& command_buffer,
                                            VUK_IA(vuk::eColorRW) target,
                                            VUK_IA(vuk::eFragmentSampled) fwd_img,
                                            VUK_IA(vuk::eFragmentSampled) bloom_img) {
    command_buffer.bind_graphics_pipeline("final_pipeline")
      .bind_persistent(0, *descriptor_set_00)
      .set_dynamic_state(vuk::DynamicStateFlagBits::eScissor | vuk::DynamicStateFlagBits::eViewport)
      .set_viewport(0, vuk::Rect2D::framebuffer())
      .set_scissor(0, vuk::Rect2D::framebuffer())
      .broadcast_color_blend(vuk::BlendPreset::eOff)
      .set_rasterization({.cullMode = vuk::CullModeFlagBits::eNone})
      .bind_image(2, 0, fwd_img)
      .bind_image(2, 1, bloom_img)
      .draw(3, 1, 0, 0);
    return target;
  })(target, color_output_w2d, bloom_output);

  return debug_pass(frame_allocator, depth_output, final_output);
#if 0
  auto shadow_map = vuk::clear_image(vuk::declare_ia("shadow_map", shadow_map_atlas.as_attachment()), vuk::DepthZero);
  shadow_map = shadow_pass(shadow_map);

  auto gtao_output = vuk::clear_image(vuk::declare_ia("gtao_output", gtao_final_texture.as_attachment()), vuk::Black<uint32_t>);
  if (RendererCVar::cvar_gtao_enable.get())
    gtao_output = gtao_pass(frame_allocator, gtao_output, depth_output, normal_output);

  #if FSR
  auto ia = forward_texture.as_attachment();
  ia.image = {};
  ia.image_view = {};
  auto fsr_image = vuk::clear_image(vuk::declare_ia("fsr_output", ia), vuk::Black<float>);
  auto pre_alpha_image_dummy = vuk::clear_image(vuk::declare_ia("pre_alpha_image", ia), vuk::Black<float>);
  auto fsr_output = fsr.dispatch(pre_alpha_image_dummy,
                                 forward_output,
                                 fsr_image,
                                 depth_output,
                                 velocity_output,
                                 *current_camera,
                                 App::get_timestep().get_elapsed_millis(),
                                 1.0f,
                                 vk_context.current_frame);
  #endif

  auto fxaa_ia = vuk::ImageAttachment::from_preset(Preset::eGeneric2D, vuk::Format::eR32G32B32A32Sfloat, {}, vuk::Samples::e1);
  auto fxaa_image = vuk::clear_image(vuk::declare_ia("fxaa_image", fxaa_ia), vuk::Black<float>);
  fxaa_image.same_extent_as(target);
  if (RendererCVar::cvar_fxaa_enable.get())
    fxaa_image = apply_fxaa(fxaa_image, forward_output);
  else
    fxaa_image = forward_output;

  auto debug_output = fxaa_image;
  if (RendererCVar::cvar_enable_debug_renderer.get()) {
    debug_output = debug_pass(frame_allocator, fxaa_image, depth_output);
  }

  auto grid_output = debug_output;
  if (RendererCVar::cvar_draw_grid.get()) {
    grid_output = apply_grid(grid_output, depth_output);
  }

  return final_pass(target, grid_output, bloom_output);
#endif
}

vuk::Value<vuk::ImageAttachment> DefaultRenderPipeline::debug_pass(vuk::Allocator& frame_allocator,
                                                                   vuk::Value<vuk::ImageAttachment>& depth_output,
                                                                   vuk::Value<vuk::ImageAttachment>& input_clr) {
  const auto& lines = DebugRenderer::get_instance()->get_lines(false);
  auto [line_vertices, line_index_count] = DebugRenderer::get_vertices_from_lines(lines);

  const auto& triangles = DebugRenderer::get_instance()->get_triangles(false);
  auto [triangle_vertices, triangle_index_count] = DebugRenderer::get_vertices_from_triangles(triangles);

  const uint32 index_count = line_index_count + triangle_index_count;
  if (index_count >= DebugRenderer::MAX_LINE_INDICES)
    OX_LOG_ERROR("Increase DebugRenderer::MAX_LINE_INDICES");

  std::vector<Vertex> vertices = line_vertices;
  vertices.insert(vertices.end(), triangle_vertices.begin(), triangle_vertices.end());

  if (vertices.empty())
    vertices.emplace_back(Vertex{});

  debug_vertex_buffer_previous = std::move(debug_vertex_buffer);

  auto& vk_context = App::get_vkcontext();

  auto [v_buff, v_buff_fut] = create_cpu_buffer(*vk_context.superframe_allocator, std::span(vertices));
  debug_vertex_buffer = std::move(v_buff);

  auto& dbg_index_buffer = *DebugRenderer::get_instance()->get_global_index_buffer();

  if (!debug_vertex_buffer_previous)
    return input_clr;

#if 0 // TODO:
  // depth tested
  const auto& lines_dt = DebugRenderer::get_instance()->get_lines(true);
  auto [vertices_dt, index_count_dt] = DebugRenderer::get_vertices_from_lines(lines_dt);

  if (vertices_dt.empty())
    vertices_dt.emplace_back(Vertex{});

  auto [vd_buff, vd_buff_fut] = create_cpu_buffer(frame_allocator, std::span(vertices_dt));
  auto& v_buffer_dt = *vd_buff;
#endif

  return vuk::make_pass("debug_pass2", [this, line_index_count, dbg_index_buffer](vuk::CommandBuffer& command_buffer, VUK_IA(vuk::eColorWrite) _output) {
    // not depth tested
    command_buffer.bind_graphics_pipeline("unlit_pipeline")
      .set_depth_stencil(vuk::PipelineDepthStencilStateCreateInfo{
        .depthTestEnable = false,
        .depthWriteEnable = false,
        .depthCompareOp = vuk::CompareOp::eGreaterOrEqual,
      })
      .set_dynamic_state(vuk::DynamicStateFlagBits::eScissor | vuk::DynamicStateFlagBits::eViewport)
      .broadcast_color_blend({})
      .set_rasterization({.polygonMode = vuk::PolygonMode::eLine, .cullMode = vuk::CullModeFlagBits::eNone})
      .set_primitive_topology(vuk::PrimitiveTopology::eLineList)
      .set_viewport(0, vuk::Rect2D::framebuffer())
      .set_scissor(0, vuk::Rect2D::framebuffer())
      .bind_vertex_buffer(0, *debug_vertex_buffer, 0, vertex_pack)
      .bind_index_buffer(dbg_index_buffer, vuk::IndexType::eUint32)
      .bind_persistent(0, *descriptor_set_00);

    camera_cb.camera_data[0] = get_main_camera_data();
    bind_camera_buffer(command_buffer);

    command_buffer.draw_indexed(line_index_count, 1, 0, 0, 0);

#if 0 // TODO
  // depth tested
    command_buffer.bind_graphics_pipeline("unlit_pipeline")
      .set_depth_stencil(vuk::PipelineDepthStencilStateCreateInfo{
        .depthTestEnable = true,
        .depthWriteEnable = false,
        .depthCompareOp = vuk::CompareOp::eGreaterOrEqual,
      })
      .set_dynamic_state(vuk::DynamicStateFlagBits::eScissor | vuk::DynamicStateFlagBits::eViewport)
      .broadcast_color_blend({})
      .set_rasterization({.polygonMode = vuk::PolygonMode::eLine, .cullMode = vuk::CullModeFlagBits::eNone})
      .set_primitive_topology(vuk::PrimitiveTopology::eLineList)
      .set_viewport(0, vuk::Rect2D::framebuffer())
      .set_scissor(0, vuk::Rect2D::framebuffer())
      .bind_index_buffer(index_buffer, vuk::IndexType::eUint32)
      .bind_vertex_buffer(0, v_buffer_dt, 0, vertex_pack)
      .bind_persistent(0, *descriptor_set_00);

    camera_cb.camera_data[0] = get_main_camera_data();
    bind_camera_buffer(command_buffer);

    command_buffer.draw_indexed(index_count_dt, 1, 0, 0, 0);
#endif

    DebugRenderer::reset();

    return _output;
  })(input_clr);
}

void DefaultRenderPipeline::on_update(Scene* scene) {
  // TODO: Account for the bounding volume of the probe
  const auto pp_view = scene->registry.view<PostProcessProbe>();
  for (auto&& [e, component] : pp_view.each()) {
    scene_data.post_processing_data.film_grain = {component.film_grain_enabled, component.film_grain_intensity};
    scene_data.post_processing_data.chromatic_aberration = {component.chromatic_aberration_enabled, component.chromatic_aberration_intensity};
    scene_data.post_processing_data.vignette_offset.w = component.vignette_enabled;
    scene_data.post_processing_data.vignette_color.a = component.vignette_intensity;
    scene_data.post_processing_data.sharpen.x = component.sharpen_enabled;
    scene_data.post_processing_data.sharpen.y = component.sharpen_intensity;
  }
}

void DefaultRenderPipeline::on_submit() {
  first_pass = false;

  clear();
}

void DefaultRenderPipeline::update_skybox(const SkyboxLoadEvent& e) {
  OX_SCOPED_ZONE;
  cube_map = e.cube_map;

  if (cube_map)
    generate_prefilter(*App::get_vkcontext().superframe_allocator);
}

vuk::Value<vuk::ImageAttachment> DefaultRenderPipeline::shadow_pass(vuk::Value<vuk::ImageAttachment>& shadow_map) {
  OX_SCOPED_ZONE;

  auto pass = vuk::make_pass("shadow_pass", [this](vuk::CommandBuffer& command_buffer, VUK_IA(vuk::eDepthStencilRW) map) {
    command_buffer.bind_persistent(0, *descriptor_set_00)
      .bind_graphics_pipeline("shadow_pipeline")
      .set_dynamic_state(vuk::DynamicStateFlagBits::eScissor | vuk::DynamicStateFlagBits::eViewport)
      .broadcast_color_blend({})
      .set_rasterization({.cullMode = vuk::CullModeFlagBits::eBack})
      .set_depth_stencil(vuk::PipelineDepthStencilStateCreateInfo{
        .depthTestEnable = true,
        .depthWriteEnable = true,
        .depthCompareOp = vuk::CompareOp::eGreaterOrEqual,
      });

#if 0
    const auto max_viewport_count = App::get_vkcontext().get_max_viewport_count();
    for (auto& light : scene_lights) {
      if (!light.cast_shadows)
        continue;

      switch (light.type) {
        case LightComponent::Directional: {
          const uint32_t cascade_count = std::min((uint32_t)light.cascade_distances.size(), max_viewport_count);
          auto viewports = std::vector<vuk::Viewport>(cascade_count);
          auto cameras = std::vector<CameraData>(cascade_count);
          auto sh_cameras = std::vector<CameraSH>(cascade_count);
          create_dir_light_cameras(light, *current_camera, sh_cameras, cascade_count);
          RenderQueue shadow_queue = {};
          uint32_t batch_index = 0;
          for (auto& batch : render_queue.batches) {
            // Determine which cascades the object is contained in:
            uint16_t camera_mask = 0;
            for (uint32_t cascade = 0; cascade < cascade_count; ++cascade) {
              const auto frustum = sh_cameras[cascade].frustum;
              const auto aabb = mesh_component_list[batch.component_index].aabb;
              if (cascade < cascade_count && aabb.is_on_frustum(frustum)) {
                camera_mask |= 1 << cascade;
              }
            }

            if (camera_mask == 0) {
              continue;
            }

            auto& b = shadow_queue.add(batch);
            b.instance_index = batch_index;
            b.camera_mask = camera_mask;

            batch_index++;
          }

          if (!shadow_queue.empty()) {
            for (uint32_t cascade = 0; cascade < cascade_count; ++cascade) {
              cameras[cascade].projection_view = sh_cameras[cascade].projection_view;
              cameras[cascade].output_index = cascade;
              camera_cb.camera_data[cascade] = cameras[cascade];

              auto& vp = viewports[cascade];
              vp.x = float(light.shadow_rect.x + cascade * light.shadow_rect.w);
              vp.y = float(light.shadow_rect.y);
              vp.width = float(light.shadow_rect.w);
              vp.height = float(light.shadow_rect.h);
              vp.minDepth = 0.0f;
              vp.maxDepth = 1.0f;

              command_buffer.set_scissor(cascade, vuk::Rect2D::framebuffer());
              command_buffer.set_viewport(cascade, vp);
            }

            bind_camera_buffer(command_buffer);

            shadow_queue.sort_opaque();

            // render_meshes(shadow_queue, command_buffer, FILTER_TRANSPARENT, RENDER_FLAGS_SHADOWS_PASS, cascade_count);
          }

          break;
        }
        case LightComponent::Point: {
          Sphere bounding_sphere(light.position, light.range);

          auto sh_cameras = std::vector<CameraSH>(6);
          create_cubemap_cameras(sh_cameras, light.position, std::max(1.0f, light.range), 0.1f); // reversed z

          auto viewports = std::vector<vuk::Viewport>(6);

          uint32_t camera_count = 0;
          for (uint32_t shcam = 0; shcam < (uint32_t)sh_cameras.size(); ++shcam) {
            // cube map frustum vs main camera frustum
            if (current_camera->get_frustum().intersects(sh_cameras[shcam].frustum)) {
              camera_cb.camera_data[camera_count] = {};
              camera_cb.camera_data[camera_count].projection_view = sh_cameras[shcam].projection_view;
              camera_cb.camera_data[camera_count].output_index = shcam;
              camera_count++;
            }
          }

          RenderQueue shadow_queue = {};
          uint32_t batch_index = 0;
          for (auto& batch : render_queue.batches) {
            const auto aabb = mesh_component_list[batch.component_index].aabb;
            if (!bounding_sphere.intersects(aabb))
              continue;

            uint16_t camera_mask = 0;
            for (uint32_t camera_index = 0; camera_index < camera_count; ++camera_index) {
              const auto frustum = sh_cameras[camera_index].frustum;
              if (aabb.is_on_frustum(frustum)) {
                camera_mask |= 1 << camera_index;
              }
            }

            if (camera_mask == 0) {
              continue;
            }

            auto& b = shadow_queue.add(batch);
            b.instance_index = batch_index;
            b.camera_mask = camera_mask;

            batch_index++;
          }

          if (!shadow_queue.empty()) {
            for (uint32_t shcam = 0; shcam < (uint32_t)sh_cameras.size(); ++shcam) {
              viewports[shcam].x = float(light.shadow_rect.x + shcam * light.shadow_rect.w);
              viewports[shcam].y = float(light.shadow_rect.y);
              viewports[shcam].width = float(light.shadow_rect.w);
              viewports[shcam].height = float(light.shadow_rect.h);
              viewports[shcam].minDepth = 0.0f;
              viewports[shcam].maxDepth = 1.0f;

              command_buffer.set_scissor(shcam, vuk::Rect2D::framebuffer());
              command_buffer.set_viewport(shcam, viewports[shcam]);
            }

            bind_camera_buffer(command_buffer);

            shadow_queue.sort_opaque();

            // render_meshes(shadow_queue, command_buffer, FILTER_TRANSPARENT, RENDER_FLAGS_SHADOWS_PASS, camera_count);
          }
          break;
        }
        case LightComponent::Spot: {
          break;
        }
      }
    }
#endif

    return map;
  });

  return pass(shadow_map);
}

vuk::Value<vuk::ImageAttachment> DefaultRenderPipeline::bloom_pass(vuk::Value<vuk::ImageAttachment>& downsample_image,
                                                                   vuk::Value<vuk::ImageAttachment>& upsample_image,
                                                                   vuk::Value<vuk::ImageAttachment>& input) {
  OX_SCOPED_ZONE;
  auto bloom_mip_count = downsample_image->level_count;

  struct BloomPushConst {
    // x: threshold, y: clamp, z: radius, w: unused
    Vec4 params = {};
  } bloom_push_const;

  bloom_push_const.params.x = RendererCVar::cvar_bloom_threshold.get();
  bloom_push_const.params.y = RendererCVar::cvar_bloom_clamp.get();

  auto prefilter = vuk::make_pass("bloom_prefilter",
                                  [bloom_push_const](vuk::CommandBuffer& command_buffer,
                                                     VUK_IA(vuk::eComputeRW) target,
                                                     VUK_IA(vuk::eComputeSampled) input) {
    command_buffer.bind_compute_pipeline("bloom_prefilter_pipeline")
      .push_constants(vuk::ShaderStageFlagBits::eCompute, 0, bloom_push_const)
      .bind_image(0, 0, target)
      .bind_sampler(0, 0, vuk::NearestMagLinearMinSamplerClamped)
      .bind_image(0, 1, input)
      .bind_sampler(0, 1, vuk::NearestMagLinearMinSamplerClamped)
      .dispatch((Renderer::get_viewport_width() + 8 - 1) / 8, (Renderer::get_viewport_height() + 8 - 1) / 8, 1);
    return target;
  });

  auto prefiltered_image = prefilter(downsample_image.mip(0), input);
  auto converge = vuk::make_pass("converge", [](vuk::CommandBuffer& command_buffer, VUK_IA(vuk::eComputeRW) output) { return output; });
  auto prefiltered_downsample_image = converge(downsample_image);
  auto src_mip = prefiltered_downsample_image.mip(0);

  for (uint32_t i = 1; i < bloom_mip_count; i++) {
    auto pass = vuk::make_pass("bloom_downsample",
                               [i](vuk::CommandBuffer& command_buffer, VUK_IA(vuk::eComputeRW) target, VUK_IA(vuk::eComputeSampled) input) {
      const auto size = IVec2(Renderer::get_viewport_width() / (1 << i), Renderer::get_viewport_height() / (1 << i));

      command_buffer.bind_compute_pipeline("bloom_downsample_pipeline")
        .bind_image(0, 0, target)
        .bind_sampler(0, 0, vuk::LinearMipmapNearestSamplerClamped)
        .bind_image(0, 1, input)
        .bind_sampler(0, 1, vuk::LinearMipmapNearestSamplerClamped)
        .dispatch((size.x + 8 - 1) / 8, (size.y + 8 - 1) / 8, 1);
      return target;
    });

    src_mip = pass(prefiltered_downsample_image.mip(i), src_mip);
  }

  // Upsampling
  // https://www.froyok.fr/blog/2021-12-ue4-custom-bloom/resources/code/bloom_down_up_demo.jpg

  auto downsampled_image = converge(prefiltered_downsample_image);
  auto upsample_src_mip = downsampled_image.mip(bloom_mip_count - 1);

  for (int32_t i = (int32_t)bloom_mip_count - 2; i >= 0; i--) {
    auto pass = vuk::make_pass("bloom_upsample",
                               [i](vuk::CommandBuffer& command_buffer,
                                   VUK_IA(vuk::eComputeRW) output,
                                   VUK_IA(vuk::eComputeSampled) src1,
                                   VUK_IA(vuk::eComputeSampled) src2) {
      const auto size = IVec2(Renderer::get_viewport_width() / (1 << i), Renderer::get_viewport_height() / (1 << i));

      command_buffer.bind_compute_pipeline("bloom_upsample_pipeline")
        .bind_image(0, 0, output)
        .bind_sampler(0, 0, vuk::NearestMagLinearMinSamplerClamped)
        .bind_image(0, 1, src1)
        .bind_sampler(0, 1, vuk::NearestMagLinearMinSamplerClamped)
        .bind_image(0, 2, src2)
        .bind_sampler(0, 2, vuk::NearestMagLinearMinSamplerClamped)
        .dispatch((size.x + 8 - 1) / 8, (size.y + 8 - 1) / 8, 1);

      return output;
    });

    upsample_src_mip = pass(upsample_image.mip(i), upsample_src_mip, downsampled_image.mip(i));
  }

  return upsample_image;
}

vuk::Value<vuk::ImageAttachment> DefaultRenderPipeline::gtao_pass(vuk::Allocator& frame_allocator,
                                                                  vuk::Value<vuk::ImageAttachment>& gtao_final_output,
                                                                  vuk::Value<vuk::ImageAttachment>& depth_input,
                                                                  vuk::Value<vuk::ImageAttachment>& normal_input) {
  OX_SCOPED_ZONE;
  gtao_settings.quality_level = RendererCVar::cvar_gtao_quality_level.get();
  gtao_settings.denoise_passes = RendererCVar::cvar_gtao_denoise_passes.get();
  gtao_settings.radius = RendererCVar::cvar_gtao_radius.get();
  gtao_settings.radius_multiplier = 1.0f;
  gtao_settings.falloff_range = RendererCVar::cvar_gtao_falloff_range.get();
  gtao_settings.sample_distribution_power = RendererCVar::cvar_gtao_sample_distribution_power.get();
  gtao_settings.thin_occluder_compensation = RendererCVar::cvar_gtao_thin_occluder_compensation.get();
  gtao_settings.final_value_power = RendererCVar::cvar_gtao_final_value_power.get();
  gtao_settings.depth_mip_sampling_offset = RendererCVar::cvar_gtao_depth_mip_sampling_offset.get();

  gtao_update_constants(gtao_constants, (int)Renderer::get_viewport_width(), (int)Renderer::get_viewport_height(), gtao_settings, current_camera, 0);

  auto [gtao_const_buff, gtao_const_buff_fut] = create_cpu_buffer(frame_allocator, std::span(&gtao_constants, 1));
  auto& gtao_const_buffer = *gtao_const_buff;

  const auto depth_ia = vuk::ImageAttachment{
    .format = vuk::Format::eR32Sfloat,
    .sample_count = vuk::SampleCountFlagBits::e1,
    .level_count = 5,
    .layer_count = 1,
  };
  auto gtao_depth = vuk::clear_image(vuk::declare_ia("gtao_depth_image", depth_ia), vuk::Black<float>);
  gtao_depth.same_extent_as(depth_input);
  auto mip0 = gtao_depth.mip(0);
  auto mip1 = gtao_depth.mip(1);
  auto mip2 = gtao_depth.mip(2);
  auto mip3 = gtao_depth.mip(3);
  auto mip4 = gtao_depth.mip(4);

  auto gtao_depth_pass = vuk::make_pass("gtao_depth_pass",
                                        [gtao_const_buffer](vuk::CommandBuffer& command_buffer,
                                                            VUK_IA(vuk::eComputeSampled) depth_input,
                                                            VUK_IA(vuk::eComputeRW) depth_mip0,
                                                            VUK_IA(vuk::eComputeRW) depth_mip1,
                                                            VUK_IA(vuk::eComputeRW) depth_mip2,
                                                            VUK_IA(vuk::eComputeRW) depth_mip3,
                                                            VUK_IA(vuk::eComputeRW) depth_mip4) {
    command_buffer.bind_compute_pipeline("gtao_first_pipeline")
      .bind_buffer(0, 0, gtao_const_buffer)
      .bind_image(0, 1, depth_input)
      .bind_image(0, 2, depth_mip0)
      .bind_image(0, 3, depth_mip1)
      .bind_image(0, 4, depth_mip2)
      .bind_image(0, 5, depth_mip3)
      .bind_image(0, 6, depth_mip4)
      .bind_sampler(0, 7, vuk::NearestSamplerClamped)
      .dispatch((Renderer::get_viewport_width() + 16 - 1) / 16, (Renderer::get_viewport_height() + 16 - 1) / 16);
  });

  gtao_depth_pass(depth_input, mip0, mip1, mip2, mip3, mip4);

  auto gtao_main_pass = vuk::make_pass("gtao_main_pass",
                                       [gtao_const_buffer](vuk::CommandBuffer& command_buffer,
                                                           VUK_IA(vuk::eComputeRW) main_image,
                                                           VUK_IA(vuk::eComputeRW) edge_image,
                                                           VUK_IA(vuk::eComputeSampled) gtao_depth_input,
                                                           VUK_IA(vuk::eComputeSampled) normal_input) {
    command_buffer.bind_compute_pipeline("gtao_main_pipeline")
      .bind_buffer(0, 0, gtao_const_buffer)
      .bind_image(0, 1, gtao_depth_input)
      .bind_image(0, 2, normal_input)
      .bind_image(0, 3, main_image)
      .bind_image(0, 4, edge_image)
      .bind_sampler(0, 5, vuk::NearestSamplerClamped)
      .dispatch((Renderer::get_viewport_width() + 8 - 1) / 8, (Renderer::get_viewport_height() + 8 - 1) / 8);

    return std::make_tuple(main_image, edge_image);
  });

  auto main_image_ia = vuk::ImageAttachment{
    .format = vuk::Format::eR8Uint,
    .sample_count = vuk::SampleCountFlagBits::e1,
    .view_type = vuk::ImageViewType::e2D,
    .level_count = 1,
    .layer_count = 1,
  };

  auto gtao_main_image = vuk::clear_image(vuk::declare_ia("gtao_main_image", main_image_ia), vuk::Black<uint32_t>);
  main_image_ia.format = vuk::Format::eR8Unorm;
  auto gtao_edge_image = vuk::clear_image(vuk::declare_ia("gtao_main_image", main_image_ia), vuk::Black<float>);

  gtao_main_image.same_extent_as(depth_input);
  gtao_edge_image.same_extent_as(depth_input);

  auto [gtao_main_output, gtao_edge_output] = gtao_main_pass(gtao_main_image, gtao_edge_image, gtao_depth, normal_input);

  auto denoise_input_output = gtao_main_output;

  const int pass_count = std::max(1, gtao_settings.denoise_passes); // should be at least one for now.
  for (int i = 0; i < pass_count; i++) {
    auto denoise_pass = vuk::make_pass("gtao_denoise_pass",
                                       [gtao_const_buffer](vuk::CommandBuffer& command_buffer,
                                                           VUK_IA(vuk::eComputeRW) output,
                                                           VUK_IA(vuk::eComputeSampled) input,
                                                           VUK_IA(vuk::eComputeSampled) edge_image) {
      command_buffer.bind_compute_pipeline("gtao_denoise_pipeline")
        .bind_buffer(0, 0, gtao_const_buffer)
        .bind_image(0, 1, input)
        .bind_image(0, 2, edge_image)
        .bind_image(0, 3, output)
        .bind_sampler(0, 4, vuk::NearestSamplerClamped)
        .dispatch((Renderer::get_viewport_width() + XE_GTAO_NUMTHREADS_X * 2 - 1) / (XE_GTAO_NUMTHREADS_X * 2),
                  (Renderer::get_viewport_height() + XE_GTAO_NUMTHREADS_Y - 1) / XE_GTAO_NUMTHREADS_Y,
                  1);

      return output;
    });

    auto d_ia = vuk::ImageAttachment{
      .format = vuk::Format::eR8Uint,
      .sample_count = vuk::SampleCountFlagBits::e1,
      .view_type = vuk::ImageViewType::e2D,
      .level_count = 1,
      .layer_count = 1,
    };
    auto denoise_image = vuk::clear_image(vuk::declare_ia("gtao_denoised_image", d_ia), vuk::Black<uint32_t>);
    denoise_image.same_extent_as(gtao_main_output);

    auto denoise_output = denoise_pass(denoise_image, denoise_input_output, gtao_edge_output);
    denoise_input_output = denoise_output;
  }

  auto gtao_final_pass = vuk::make_pass("gtao_final_pass",
                                        [gtao_const_buffer](vuk::CommandBuffer& command_buffer,
                                                            VUK_IA(vuk::eComputeRW) final_image,
                                                            VUK_IA(vuk::eComputeSampled) denoise_input,
                                                            VUK_IA(vuk::eComputeSampled) edge_input) {
    command_buffer.bind_compute_pipeline("gtao_final_pipeline")
      .bind_buffer(0, 0, gtao_const_buffer)
      .bind_image(0, 1, denoise_input)
      .bind_image(0, 2, edge_input)
      .bind_image(0, 3, final_image)
      .bind_sampler(0, 4, vuk::NearestSamplerClamped)
      .dispatch((Renderer::get_viewport_width() + XE_GTAO_NUMTHREADS_X * 2 - 1) / (XE_GTAO_NUMTHREADS_X * 2),
                (Renderer::get_viewport_height() + XE_GTAO_NUMTHREADS_Y - 1) / XE_GTAO_NUMTHREADS_Y,
                1);
    return final_image;
  });

  return gtao_final_pass(gtao_final_output, denoise_input_output, gtao_edge_output);
}

vuk::Value<vuk::ImageAttachment> DefaultRenderPipeline::apply_fxaa(vuk::Value<vuk::ImageAttachment>& target,
                                                                   vuk::Value<vuk::ImageAttachment>& input) {
  OX_SCOPED_ZONE;

  auto pass = vuk::make_pass("fxaa", [](vuk::CommandBuffer& command_buffer, VUK_IA(vuk::eColorRW) dst, VUK_IA(vuk::eFragmentSampled) src) {
    struct FXAAData {
      Vec2 inverse_screen_size;
    } fxaa_data;

    fxaa_data.inverse_screen_size = 1.0f / Vec2((float)Renderer::get_viewport_width(), (float)Renderer::get_viewport_height());

    auto* fxaa_buffer = command_buffer.scratch_buffer<FXAAData>(0, 1);
    *fxaa_buffer = fxaa_data;

    command_buffer.bind_graphics_pipeline("fxaa_pipeline")
      .set_dynamic_state(vuk::DynamicStateFlagBits::eScissor | vuk::DynamicStateFlagBits::eViewport)
      .set_viewport(0, vuk::Rect2D::framebuffer())
      .set_scissor(0, vuk::Rect2D::framebuffer())
      .broadcast_color_blend(vuk::BlendPreset::eOff)
      .set_rasterization({.cullMode = vuk::CullModeFlagBits::eNone})
      .bind_image(0, 0, src)
      .bind_sampler(0, 0, vuk::LinearSamplerClamped)
      .draw(3, 1, 0, 0);

    return dst;
  });

  return pass(target, input);
}

vuk::Value<vuk::ImageAttachment> DefaultRenderPipeline::apply_grid(vuk::Value<vuk::ImageAttachment>& target,
                                                                   vuk::Value<vuk::ImageAttachment>& depth) {
  OX_SCOPED_ZONE;

  auto pass = vuk::make_pass("grid", [this](vuk::CommandBuffer& command_buffer, VUK_IA(vuk::eColorWrite) dst, VUK_IA(vuk::eDepthStencilRW) depth) {
    command_buffer.bind_graphics_pipeline("grid_pipeline")
      .set_dynamic_state(vuk::DynamicStateFlagBits::eScissor | vuk::DynamicStateFlagBits::eViewport)
      .set_viewport(0, vuk::Rect2D::framebuffer())
      .set_scissor(0, vuk::Rect2D::framebuffer())
      .broadcast_color_blend(vuk::BlendPreset::eAlphaBlend)
      .set_rasterization({.cullMode = vuk::CullModeFlagBits::eNone})
      .set_depth_stencil({.depthTestEnable = true, .depthWriteEnable = false, .depthCompareOp = vuk::CompareOp::eGreaterOrEqual})
      .bind_persistent(0, *descriptor_set_00);

    bind_camera_buffer(command_buffer);

    m_quad->bind_index_buffer(command_buffer)->bind_vertex_buffer(command_buffer);

    command_buffer.draw_indexed(m_quad->index_count, 1, 0, 0, 0);

    return dst;
  });

  return pass(target, depth);
}

void DefaultRenderPipeline::generate_prefilter(vuk::Allocator& allocator) {
  OX_SCOPED_ZONE;
  auto* compiler = get_compiler();

  auto brdf_img = Prefilter::generate_brdflut();
  brdf_texture = *brdf_img.get(allocator, *compiler);

  auto irradiance_img = Prefilter::generate_irradiance_cube(m_cube, cube_map);
  irradiance_texture = *irradiance_img.get(allocator, *compiler);

  auto prefilter_img = Prefilter::generate_prefiltered_cube(m_cube, cube_map);
  prefiltered_texture = *prefilter_img.get(allocator, *compiler);
}

vuk::Value<vuk::ImageAttachment> DefaultRenderPipeline::sky_transmittance_pass() {
  OX_SCOPED_ZONE;

  return vuk::make_pass("sky_transmittance_lut_pass", [this](vuk::CommandBuffer& command_buffer, VUK_IA(vuk::eComputeRW) dst) {
    const IVec2 lut_size = {256, 64};
    command_buffer.bind_persistent(0, *descriptor_set_00)
      .bind_compute_pipeline("sky_transmittance_pipeline")
      .dispatch((lut_size.x + 8 - 1) / 8, (lut_size.y + 8 - 1) / 8);

    return dst;
  })(vuk::clear_image(vuk::declare_ia("sky_transmittance_lut", sky_transmittance_lut.as_attachment()), vuk::Black<float>));
}

vuk::Value<vuk::ImageAttachment> DefaultRenderPipeline::sky_multiscatter_pass(vuk::Value<vuk::ImageAttachment>& transmittance_lut) {
  OX_SCOPED_ZONE;

  return vuk::make_pass("sky_multiscatter_lut_pass",
                        [this](vuk::CommandBuffer& command_buffer, VUK_IA(vuk::eComputeRW) dst, VUK_IA(vuk::eComputeSampled) transmittance_lut) {
    const IVec2 lut_size = {32, 32};
    command_buffer.bind_compute_pipeline("sky_multiscatter_pipeline").bind_persistent(0, *descriptor_set_00).dispatch(lut_size.x, lut_size.y);

    return dst;
  })(vuk::clear_image(vuk::declare_ia("sky_multiscatter_lut", sky_multiscatter_lut.as_attachment()), vuk::Black<float>), transmittance_lut);
}

vuk::Value<vuk::ImageAttachment> DefaultRenderPipeline::sky_envmap_pass(vuk::Value<vuk::ImageAttachment>& envmap_image) {
  [[maybe_unused]] auto map = vuk::make_pass("sky_envmap_pass", [this](vuk::CommandBuffer& command_buffer, VUK_IA(vuk::eColorRW) envmap) {
    auto sh_cameras = std::vector<CameraSH>(6);
    create_cubemap_cameras(sh_cameras);

    for (int i = 0; i < 6; i++)
      camera_cb.camera_data[i].projection_view = sh_cameras[i].projection_view;

    bind_camera_buffer(command_buffer);

    command_buffer.bind_persistent(0, *descriptor_set_00)
      .set_viewport(0, vuk::Rect2D::framebuffer())
      .set_scissor(0, vuk::Rect2D::framebuffer())
      .broadcast_color_blend(vuk::BlendPreset::eOff)
      .set_rasterization({.cullMode = vuk::CullModeFlagBits::eNone})
      .set_depth_stencil({})
      .bind_graphics_pipeline("sky_envmap_pipeline");

    m_cube->bind_index_buffer(command_buffer)->bind_vertex_buffer(command_buffer);
    command_buffer.draw_indexed(m_cube->index_count, 6, 0, 0, 0);

    return envmap;
  })(envmap_image.mip(0));

  return envmap_spd.dispatch("envmap_spd", *get_frame_allocator(), envmap_image);
}

#if 0 // UNUSED
void DefaultRenderPipeline::sky_view_lut_pass(const Shared<vuk::RenderGraph>& rg) {
  OX_SCOPED_ZONE;

  const auto attachment = vuk::ImageAttachment{.extent = vuk::Dimension3D::absolute(192, 104, 1),
                                               .format = vuk::Format::eR16G16B16A16Sfloat,
                                               .sample_count = vuk::SampleCountFlagBits::e1,
                                               .base_level = 0,
                                               .level_count = 1,
                                               .base_layer = 0,
                                               .layer_count = 1};

  rg->attach_and_clear_image("sky_view_lut", attachment, vuk::Black<float>);

  rg->add_pass({.name = "sky_view_pass",
                .resources =
                  {
                    "sky_view_lut"_image >> vuk::eColorRW,
                    "sky_transmittance_lut+"_image >> vuk::eFragmentSampled,
                    "sky_multiscatter_lut+"_image >> vuk::eFragmentSampled,
                  },
                .execute = [this](vuk::CommandBuffer& command_buffer) {
    bind_camera_buffer(command_buffer);

    command_buffer.bind_persistent(0, *descriptor_set_00)
      .set_dynamic_state(vuk::DynamicStateFlagBits::eScissor | vuk::DynamicStateFlagBits::eViewport)
      .set_viewport(0, vuk::Rect2D::framebuffer())
      .set_scissor(0, vuk::Rect2D::framebuffer())
      .broadcast_color_blend(vuk::BlendPreset::eOff)
      .set_rasterization({.cullMode = vuk::CullModeFlagBits::eNone})
      .bind_graphics_pipeline("sky_view_pipeline")
      .draw(3, 1, 0, 0);
  }});
}
#endif
} // namespace ox
