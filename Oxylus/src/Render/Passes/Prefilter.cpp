#include "Prefilter.hpp"

#include <glm/gtx/quaternion.hpp>
#include <vuk/RenderGraph.hpp>

#include "Core/App.hpp"
#include "Core/FileSystem.hpp"
#include "Render/Mesh.hpp"
#include "Render/Vulkan/VkContext.hpp"

#define M_PI 3.14159265358979323846

namespace ox {
vuk::Value<vuk::ImageAttachment> Prefilter::generate_brdflut() {
  auto& vk_context = App::get_vkcontext();

  if (!vk_context.runtime->is_pipeline_available("BRDFLUTPipeline")) {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(fs::read_shader_file("GenBrdfLut.vert"), "GenBrdfLut.vert");
    pci.add_glsl(fs::read_shader_file("GenBrdfLut.frag"), "GenBrdfLut.frag");
    vk_context.runtime->create_named_pipeline("BRDFLUTPipeline", pci);
  }

  constexpr uint32_t dim = 512;
  const vuk::ImageAttachment attachment = {
    .image_type = vuk::ImageType::e2D,
    .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment,
    .extent = {dim, dim, 1},
    .format = vuk::Format::eR32G32B32A32Sfloat,
    .sample_count = vuk::Samples::e1,
    .view_type = vuk::ImageViewType::e2D,
    .base_level = 0,
    .level_count = 1,
    .base_layer = 0,
    .layer_count = 1,
  };

  const auto img = vuk::declare_ia("brdf_lut", attachment);

  auto pass = vuk::make_pass("BRDF_LUTPass", [](vuk::CommandBuffer& command_buffer, VUK_IA(vuk::eColorWrite) output) {
    command_buffer.broadcast_color_blend({})
      .set_rasterization({})
      .set_depth_stencil({})
      .set_viewport(0, vuk::Rect2D::framebuffer())
      .set_scissor(0, vuk::Rect2D::framebuffer())
      .bind_graphics_pipeline("BRDFLUTPipeline")
      .draw(3, 1, 0, 0);
    return output;
  });

  return pass(img);
}

vuk::Value<vuk::ImageAttachment> Prefilter::generate_irradiance_cube(const Shared<Mesh>& skybox, const Shared<Texture>& cubemap) {
  auto& vk_context = App::get_vkcontext();
  constexpr int32_t dim = 64;

  const auto capture_projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
  const Mat4 capture_views[] = {lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f)),
                                lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(-1.0f, 0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f)),
                                lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f)),
                                lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f)),
                                lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), Vec3(0.0f, -1.0f, 0.0f)),
                                lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f), Vec3(0.0f, -1.0f, 0.0f))};

  if (!vk_context.runtime->is_pipeline_available("IrradiancePipeline")) {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(fs::read_shader_file("Cubemap.vert"), "Cubemap.vert");
    pci.add_glsl(fs::read_shader_file("IrradianceCube.frag"), "IrradianceCube.frag");
    vk_context.runtime->create_named_pipeline("IrradiancePipeline", pci);
  }

  vuk::ImageAttachment attachment = {
    .image_flags = vuk::ImageCreateFlagBits::eCubeCompatible,
    .image_type = vuk::ImageType::e2D,
    .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment,
    .extent = {dim, dim, 1},
    .format = vuk::Format::eR32G32B32A32Sfloat,
    .sample_count = vuk::Samples::e1,
    .view_type = vuk::ImageViewType::eCube,
    .base_level = 0,
    .level_count = 1,
    .base_layer = 0,
    .layer_count = 6,
  };

  const auto img = vuk::declare_ia("irradiance_image", attachment);

  auto pass = vuk::make_pass("ComputeIrradiance", [=](vuk::CommandBuffer& command_buffer, VUK_IA(vuk::eColorWrite) output) {
    constexpr struct PushBlock {
      float delta_phi;
      float delta_theta;
    } push_block = {.delta_phi = 2.0f * static_cast<float>(M_PI) / 180.0f, .delta_theta = 0.5f * static_cast<float>(M_PI) / 64.0f};

    command_buffer.broadcast_color_blend(vuk::BlendPreset::eOff)
      .set_rasterization({})
      .set_depth_stencil({})
      .set_viewport(0, vuk::Rect2D::framebuffer())
      .set_scissor(0, vuk::Rect2D::framebuffer())
      .bind_graphics_pipeline("IrradiancePipeline")
      .bind_sampler(1, 0, {.magFilter = vuk::Filter::eLinear, .minFilter = vuk::Filter::eLinear})
      .bind_image(1, 0, cubemap->as_attachment())
      .push_constants(vuk::ShaderStageFlagBits::eFragment, 0, push_block);

    auto* projection = command_buffer.scratch_buffer<glm::mat4>(0, 0);
    *projection = capture_projection;
    const auto view = command_buffer.scratch_buffer<glm::mat4[6]>(0, 1);
    memcpy(view, capture_views, sizeof(capture_views));

    skybox->bind_vertex_buffer(command_buffer);
    skybox->bind_index_buffer(command_buffer);
    command_buffer.draw_indexed(skybox->index_count, 6, 0, 0, 0);

    return output;
  });

  return pass(img);
}

vuk::Value<vuk::ImageAttachment> Prefilter::generate_prefiltered_cube(const Shared<Mesh>& skybox, const Shared<Texture>& cubemap) {
  auto& vk_context = App::get_vkcontext();
  constexpr int32_t dim = 512;

  constexpr auto pipeline_name = "PrefilterPipeline";

  if (!vk_context.runtime->is_pipeline_available(pipeline_name)) {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(fs::read_shader_file("Cubemap.vert"), "Cubemap.vert");
    pci.add_glsl(fs::read_shader_file("PrefilterEnvMap.frag"), "PrefilterEnvMap.frag");
    vk_context.runtime->create_named_pipeline(pipeline_name, pci);
  }

  const auto capture_projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
  const Mat4 capture_views[] = {lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f)),
                                lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(-1.0f, 0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f)),
                                lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f)),
                                lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f)),
                                lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), Vec3(0.0f, -1.0f, 0.0f)),
                                lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f), Vec3(0.0f, -1.0f, 0.0f))};

  auto ia = vuk::ImageAttachment::from_preset(vuk::ImageAttachment::Preset::eRTTCube,
                                              vuk::Format::eR32G32B32A32Sfloat,
                                              {dim, dim, 1},
                                              vuk::SampleCountFlagBits::e1);

  auto img = vuk::declare_ia("prefilter_cube", ia);

  auto pass = vuk::make_pass("PrefilterPass", [=](vuk::CommandBuffer& command_buffer, VUK_IA(vuk::eColorWrite) output) {
    constexpr struct PushBlock {
      float roughness = {};
      uint32_t num_samples = 32u;
    } push_block;

    command_buffer.broadcast_color_blend(vuk::BlendPreset::eOff)
      .set_rasterization({})
      .set_depth_stencil({})
      .set_viewport(0, vuk::Rect2D::framebuffer())
      .set_scissor(0, vuk::Rect2D::framebuffer())
      .bind_graphics_pipeline("PrefilterPipeline")
      .bind_sampler(1, 0, {.magFilter = vuk::Filter::eLinear, .minFilter = vuk::Filter::eLinear})
      .bind_image(1, 0, cubemap->as_attachment())
      .push_constants(vuk::ShaderStageFlagBits::eFragment, 0, push_block);

    auto* projection = command_buffer.scratch_buffer<glm::mat4>(0, 0);
    *projection = capture_projection;
    const auto view = command_buffer.scratch_buffer<glm::mat4[6]>(0, 1);
    memcpy(view, capture_views, sizeof(capture_views));

    skybox->bind_vertex_buffer(command_buffer);
    skybox->bind_index_buffer(command_buffer);
    command_buffer.draw_indexed(skybox->index_count, 6, 0, 0, 0);

    return output;
  });

  return pass(img);
}
} // namespace ox
