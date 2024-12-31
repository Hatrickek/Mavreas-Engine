#pragma once

#include <vuk/Value.hpp>
#include <vuk/Types.hpp>
#include <vuk/runtime/vk/Descriptor.hpp>
#include "Core/Types.hpp"

namespace ox {
class SPD {
public:
  enum class SPDLoad : uint32 {
    Load,
    LinearSampler,
  };

  struct Config {
    SPDLoad load = SPDLoad::Load;
    vuk::ImageViewType view_type = vuk::ImageViewType::e2D;
    vuk::SamplerCreateInfo sampler = {};
  };

  SPD() = default;
  ~SPD() = default;

  void init(vuk::Allocator& allocator, Config config);

  vuk::Value<vuk::ImageAttachment> dispatch(vuk::Name pass_name, vuk::Allocator& allocator, vuk::Value<vuk::ImageAttachment> image);

private:
  static constexpr auto SPD_MAX_MIP_LEVELS = 13;
  Config _config;
  vuk::Unique<vuk::PersistentDescriptorSet> descriptor_set;
  vuk::Buffer global_counter_buffer;
  std::string pipeline_name;
};
} // namespace ox
