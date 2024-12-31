#include "SPD.hpp"

#include <vuk/RenderGraph.hpp>
#include <vuk/runtime/CommandBuffer.hpp>
#include <vuk/runtime/vk/Descriptor.hpp>
#include <vuk/runtime/vk/Pipeline.hpp>

#include "Core/FileSystem.hpp"
#include "Render/Utils/VukCommon.hpp"
#include "Render/Vulkan/VkContext.hpp"
#include "Utils/Log.hpp"
#include "Utils/Profiler.hpp"

namespace ox {
static void SpdSetup(uint2& dispatchThreadGroupCountXY,    // CPU side: dispatch thread group count xy
                     uint2& workGroupOffset,               // GPU side: pass in as constant
                     uint2& numWorkGroupsAndMips,          // GPU side: pass in as constant
                     UVec4 rectInfo,                       // left, top, width, height
                     int32_t mips)                         // optional: if -1, calculate based on rect width and height
{
  workGroupOffset[0] = rectInfo[0] / 64;                   // rectInfo[0] = left
  workGroupOffset[1] = rectInfo[1] / 64;                   // rectInfo[1] = top

  uint32 endIndexX = (rectInfo[0] + rectInfo[2] - 1) / 64; // rectInfo[0] = left, rectInfo[2] = width
  uint32 endIndexY = (rectInfo[1] + rectInfo[3] - 1) / 64; // rectInfo[1] = top, rectInfo[3] = height

  dispatchThreadGroupCountXY[0] = endIndexX + 1 - workGroupOffset[0];
  dispatchThreadGroupCountXY[1] = endIndexY + 1 - workGroupOffset[1];

  numWorkGroupsAndMips[0] = (dispatchThreadGroupCountXY[0]) * (dispatchThreadGroupCountXY[1]);

  if (mips >= 0) {
    numWorkGroupsAndMips[1] = uint32(mips);
  } else {
    // calculate based on rect width and height
    uint32 resolution = std::max(rectInfo[2], rectInfo[3]);
    numWorkGroupsAndMips[1] = uint32((std::min(floor(log2(float(resolution))), float(12))));
  }
}

static void SpdSetup(uint2& dispatchThreadGroupCountXY, // CPU side: dispatch thread group count xy
                     uint2& workGroupOffset,            // GPU side: pass in as constant
                     uint2& numWorkGroupsAndMips,       // GPU side: pass in as constant
                     uint4 rectInfo)                    // left, top, width, height
{
  SpdSetup(dispatchThreadGroupCountXY, workGroupOffset, numWorkGroupsAndMips, rectInfo, -1);
}

static VkDescriptorSetLayoutBinding binding(uint32 binding, vuk::DescriptorType descriptor_type, uint32 count = 1024) {
  return {
    .binding = binding,
    .descriptorType = (VkDescriptorType)descriptor_type,
    .descriptorCount = count,
    .stageFlags = (VkShaderStageFlags)vuk::ShaderStageFlagBits::eAll,
  };
}

void SPD::init(vuk::Allocator& allocator, Config config) {
  _config = config;

  vuk::PipelineBaseCreateInfo pci = {};
  vuk::DescriptorSetLayoutCreateInfo layout_create_info = {};
  if (config.load == SPDLoad::Load) {
    layout_create_info.bindings = {
      binding(0, vuk::DescriptorType::eStorageImage, 13),
      binding(1, vuk::DescriptorType::eStorageImage, 1),
      binding(2, vuk::DescriptorType::eStorageBuffer, 1),
    };
    layout_create_info.index = 0;
    for (int i = 0; i < 3; i++)
      layout_create_info.flags.emplace_back(VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);
    pci.explicit_set_layouts.emplace_back(layout_create_info);
  } else {
    layout_create_info.bindings = {
      binding(0, vuk::DescriptorType::eStorageImage, 12),
      binding(1, vuk::DescriptorType::eStorageImage, 1),
      binding(2, vuk::DescriptorType::eStorageBuffer, 1),
      binding(3, vuk::DescriptorType::eSampledImage, 1),
      binding(4, vuk::DescriptorType::eSampler, 1),
    };
    layout_create_info.index = 0;
    for (int i = 0; i < 5; i++)
      layout_create_info.flags.emplace_back(VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);
    pci.explicit_set_layouts.emplace_back(layout_create_info);
  }

  const auto shader_path = config.load == SPDLoad::LinearSampler ? "FFX/SPD/SPDLinear.hlsl" : "FFX/SPD/SPD.hlsl";
  if (config.load == SPDLoad::LinearSampler) {
    if (config.sampler.minFilter == vuk::Filter::eNearest) {
      pci.define("POINT_SAMPLER", "");
      pipeline_name = "spd_pipeline_linear_point";
    } else {
      pipeline_name = "spd_pipeline_linear";
    }
  } else {
    pipeline_name = "spd_pipeline";
  }
  if (config.view_type == vuk::ImageViewType::e2DArray)
    pci.define("TEXTURE_ARRAY", "");
  if (!allocator.get_context().is_pipeline_available(pipeline_name.c_str())) {
    pci.add_hlsl(fs::read_shader_file(shader_path), fs::get_shader_path(shader_path), vuk::HlslShaderStage::eCompute);
    TRY(allocator.get_context().create_named_pipeline(pipeline_name.c_str(), pci))
  }

  descriptor_set = allocator.get_context().create_persistent_descriptorset(allocator,
                                                                           *allocator.get_context().get_named_pipeline(pipeline_name.c_str()),
                                                                           0,
                                                                           64);
}

vuk::Value<vuk::ImageAttachment> SPD::dispatch(vuk::Name pass_name, vuk::Allocator& allocator, vuk::Value<vuk::ImageAttachment> image) {
  OX_SCOPED_ZONE;

  OX_ASSERT(image->level_count <= 13);

  std::vector<uint32> global_atomics(image->layer_count);
  std::fill(global_atomics.begin(), global_atomics.end(), 0u);

  auto buff = vuk::create_cpu_buffer(allocator, std::span(global_atomics));
  global_counter_buffer = *buff.first;
  descriptor_set->update_storage_buffer(2, 0, global_counter_buffer);

  uint32_t num_uavs = image->level_count;
  if (_config.load == SPDLoad::LinearSampler) {
    num_uavs = image->level_count - 1;
  }

  std::vector<vuk::ImageView> views = {};
  views.resize(num_uavs);
  std::vector<vuk::ImageViewCreateInfo> cis = {};
  cis.resize(num_uavs);
  for (uint32 mip = 0; mip < num_uavs; mip++) {
    cis[mip] = vuk::ImageViewCreateInfo{
      .image = image->image.image,
      .viewType = _config.view_type,
      .format = image->format,
      .subresourceRange =
        vuk::ImageSubresourceRange{
          .aspectMask = vuk::ImageAspectFlagBits::eColor,
          .baseMipLevel = mip + (_config.load == SPDLoad::LinearSampler ? 1 : 0),
          .levelCount = 1,
          .baseArrayLayer = 0,
          .layerCount = image->layer_count,
        },
      .view_usage = vuk::ImageUsageFlagBits::eStorage,
    };
  }
  allocator.allocate_image_views(views, cis);

  if (_config.load == SPDLoad::Load) {
    for (uint32 i = 0; i < num_uavs; i++)
      descriptor_set->update_storage_image(0, i, views[i]);
    descriptor_set->update_storage_image(1, 0, views[6]);
  }

  if (_config.load == SPDLoad::LinearSampler) {
    for (uint32 i = 0; i < num_uavs; i++)
      descriptor_set->update_storage_image(0, i, views[i]);
    descriptor_set->update_storage_image(1, 0, views[5]);

    vuk::ImageView base_view = {};
    auto ci = vuk::ImageViewCreateInfo{
      .image = image->image.image,
      .viewType = _config.view_type,
      .format = image->format,
      .subresourceRange =
        vuk::ImageSubresourceRange{
          .aspectMask = vuk::ImageAspectFlagBits::eColor,
          .baseMipLevel = 0,
          .levelCount = image->level_count,
          .baseArrayLayer = 0,
          .layerCount = image->layer_count,
        },
      .view_usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage,
    };
    allocator.allocate_image_views(std::span(&base_view, 1), std::span(&ci, 1));

    descriptor_set->update_sampled_image(3, 0, base_view, vuk::ImageLayout::eReadOnlyOptimal);

    const auto sampler = allocator.get_context().acquire_sampler(_config.sampler, allocator.get_context().get_frame_count());
    descriptor_set->update_sampler(4, 0, sampler);
  }

  descriptor_set->commit(allocator.get_context());

  auto pass = vuk::make_pass(pass_name, [this, num_uavs](vuk::CommandBuffer& command_buffer, VUK_IA(vuk::eComputeRW) input) {
    uint2 dispatchThreadGroupCountXY;
    uint2 workGroupOffset;                                                         // needed if Left and Top are not 0,0
    uint2 numWorkGroupsAndMips;
    const uint4 rectInfo = uint4(0, 0, input->extent.width, input->extent.height); // left, top, width, height
    SpdSetup(dispatchThreadGroupCountXY, workGroupOffset, numWorkGroupsAndMips, rectInfo);

    command_buffer.image_barrier(input,
                                 vuk::Access::eComputeRW,
                                 vuk::Access::eComputeSampled,
                                 _config.load == SPDLoad::LinearSampler ? 1 : 0,
                                 num_uavs);

    if (_config.load == SPDLoad::LinearSampler) {
      command_buffer.image_barrier(input, vuk::Access::eComputeRW, vuk::Access::eComputeRW, 0, 1);
    }

    command_buffer.bind_compute_pipeline(pipeline_name.c_str());

    command_buffer.bind_persistent(0, *descriptor_set);

    // Bind push constants
    struct SpdConstants {
      uint32 mips;
      uint32 numWorkGroupsPerSlice;
      uint32 workGroupOffset[2];
    };

    struct SpdLinearSamplerConstants {
      uint32 mips;
      uint32 numWorkGroupsPerSlice;
      uint32 workGroupOffset[2];
      float invInputSize[2];
      float padding[2];
    };

    if (_config.load == SPDLoad::LinearSampler) {
      SpdLinearSamplerConstants data;
      data.numWorkGroupsPerSlice = numWorkGroupsAndMips[0];
      data.mips = numWorkGroupsAndMips[1];
      data.workGroupOffset[0] = workGroupOffset[0];
      data.workGroupOffset[1] = workGroupOffset[1];
      data.invInputSize[0] = 1.0f / (float)input->extent.width;
      data.invInputSize[1] = 1.0f / (float)input->extent.height;
      command_buffer.push_constants(vuk::ShaderStageFlagBits::eCompute, 0, &data, sizeof(SpdLinearSamplerConstants));
    } else {
      SpdConstants data;
      data.numWorkGroupsPerSlice = numWorkGroupsAndMips[0];
      data.mips = numWorkGroupsAndMips[1];
      data.workGroupOffset[0] = workGroupOffset[0];
      data.workGroupOffset[1] = workGroupOffset[1];
      command_buffer.push_constants(vuk::ShaderStageFlagBits::eCompute, 0, &data, sizeof(SpdConstants));
    }

    // should be / 64
    uint32 dispatchX = dispatchThreadGroupCountXY[0];
    uint32 dispatchY = dispatchThreadGroupCountXY[1];
    uint32 dispatchZ = input->layer_count; // slices

    command_buffer.dispatch(dispatchX, dispatchY, dispatchZ)
      .image_barrier(input, vuk::Access::eComputeSampled, vuk::Access::eComputeRW, _config.load == SPDLoad::LinearSampler ? 1 : 0, num_uavs);

    return input;
  });

  return pass(image);
}

} // namespace ox
