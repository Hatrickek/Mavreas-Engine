#pragma once

#include <span>
#include <vuk/Value.hpp>
#include <vuk/vsl/Core.hpp>

namespace vuk {
inline SamplerCreateInfo NearestSamplerClamped = {
  .magFilter = Filter::eNearest,
  .minFilter = Filter::eNearest,
  .mipmapMode = SamplerMipmapMode::eNearest,
  .addressModeU = SamplerAddressMode::eClampToEdge,
  .addressModeV = SamplerAddressMode::eClampToEdge,
  .addressModeW = SamplerAddressMode::eClampToEdge,
};

inline SamplerCreateInfo NearestSamplerRepeated = {
  .magFilter = Filter::eNearest,
  .minFilter = Filter::eNearest,
  .mipmapMode = SamplerMipmapMode::eNearest,
  .addressModeU = SamplerAddressMode::eRepeat,
  .addressModeV = SamplerAddressMode::eRepeat,
  .addressModeW = SamplerAddressMode::eRepeat,
};

inline SamplerCreateInfo NearestMagLinearMinSamplerClamped = {
  .magFilter = Filter::eLinear,
  .minFilter = Filter::eNearest,
  .mipmapMode = SamplerMipmapMode::eNearest,
  .addressModeU = SamplerAddressMode::eClampToEdge,
  .addressModeV = SamplerAddressMode::eClampToEdge,
  .addressModeW = SamplerAddressMode::eClampToEdge,
};

inline SamplerCreateInfo LinearMipmapNearestSamplerClamped = {
  .magFilter = Filter::eNearest,
  .minFilter = Filter::eNearest,
  .mipmapMode = SamplerMipmapMode::eLinear,
  .addressModeU = SamplerAddressMode::eClampToEdge,
  .addressModeV = SamplerAddressMode::eClampToEdge,
  .addressModeW = SamplerAddressMode::eClampToEdge,
};

inline SamplerCreateInfo LinearSamplerRepeated = {
  .magFilter = Filter::eLinear,
  .minFilter = Filter::eLinear,
  .mipmapMode = SamplerMipmapMode::eLinear,
  .addressModeU = SamplerAddressMode::eRepeat,
  .addressModeV = SamplerAddressMode::eRepeat,
  .addressModeW = SamplerAddressMode::eRepeat,
};

inline SamplerCreateInfo LinearSamplerRepeatedAnisotropy = {
  .magFilter = Filter::eLinear,
  .minFilter = Filter::eLinear,
  .mipmapMode = SamplerMipmapMode::eLinear,
  .addressModeU = SamplerAddressMode::eRepeat,
  .addressModeV = SamplerAddressMode::eRepeat,
  .addressModeW = SamplerAddressMode::eRepeat,
  .anisotropyEnable = true,
  .maxAnisotropy = 16.0f,
};

inline SamplerCreateInfo LinearSamplerClamped = {
  .magFilter = Filter::eLinear,
  .minFilter = Filter::eLinear,
  .mipmapMode = SamplerMipmapMode::eLinear,
  .addressModeU = SamplerAddressMode::eClampToEdge,
  .addressModeV = SamplerAddressMode::eClampToEdge,
  .addressModeW = SamplerAddressMode::eClampToEdge,
  .borderColor = BorderColor::eFloatOpaqueWhite,
};

inline SamplerCreateInfo CmpDepthSampler = {
  .magFilter = Filter::eLinear,
  .minFilter = Filter::eLinear,
  .mipmapMode = SamplerMipmapMode::eNearest,
  .addressModeU = SamplerAddressMode::eClampToEdge,
  .addressModeV = SamplerAddressMode::eClampToEdge,
  .addressModeW = SamplerAddressMode::eClampToEdge,
  .compareEnable = true,
  .compareOp = CompareOp::eGreaterOrEqual,
  .minLod = 0.0f,
  .maxLod = 0.0f,
};

inline vuk::ImageAttachment dummy_attachment = {
  .extent = {1, 1, 1},
  .format = vuk::Format::eR8G8B8A8Unorm,
  .sample_count = vuk::SampleCountFlagBits::e1,
  .level_count = 1,
  .layer_count = 1,
};

inline vuk::Extent3D operator/(const vuk::Extent3D& ext, float rhs) {
  return {unsigned((float)ext.width / rhs), unsigned((float)ext.height / rhs), 1u};
}

template <class T>
std::pair<Unique<Buffer>, Value<Buffer>> create_cpu_buffer(Allocator& allocator, std::span<T> data) {
  return create_buffer(allocator, MemoryUsage::eCPUtoGPU, DomainFlagBits::eTransferOnGraphics, data);
}

template <class T>
std::pair<Unique<Buffer>, Value<Buffer>> create_gpu_buffer(Allocator& allocator, std::span<T> data) {
  return create_buffer(allocator, MemoryUsage::eGPUonly, DomainFlagBits::eTransferOnGraphics, data);
}

inline vuk::Unique<Buffer> allocate_cpu_buffer(Allocator& allocator, uint64_t size, uint64_t alignment = 1) {
  return *vuk::allocate_buffer(allocator, {.mem_usage = MemoryUsage::eCPUtoGPU, .size = size, .alignment = alignment});
}

inline vuk::Unique<Buffer> allocate_gpu_buffer(Allocator& allocator, uint64_t size, uint64_t alignment = 1) {
  return *vuk::allocate_buffer(allocator, {.mem_usage = MemoryUsage::eGPUonly, .size = size, .alignment = alignment});
}

vuk::Value<vuk::ImageAttachment> generate_mips(vuk::Value<vuk::ImageAttachment> image, uint32_t mip_count);
} // namespace vuk
