#pragma once

#include <vuk/Types.hpp>
#include <vuk/runtime/vk/VkTypes.hpp>
#include "Utils/Profiler.hpp"

// Profilers
#define GPU_PROFILER_ENABLED 0
#define CPU_PROFILER_ENABLED 0
#define MEMORY_PROFILER_ENABLED 0

#ifdef OX_DISTRIBUTION
  #undef GPU_PROFILER_ENABLED
  #define GPU_PROFILER_ENABLED 0
#endif

#if GPU_PROFILER_ENABLED
  #define OX_TRACE_GPU_TRANSIENT(context, cmdbuf, name) TracyVkZoneTransient(context, , cmdbuf, name, true)
#else
  #define OX_TRACE_GPU_TRANSIENT(context, cmdbuf, name)
#endif

#ifdef OX_DISTRIBUTION
  #undef CPU_PROFILER_ENABLED
  #define CPU_PROFILER_ENABLED 0
#endif

#if CPU_PROFILER_ENABLED
  #define OX_SCOPED_ZONE ZoneScoped
  #define OX_SCOPED_ZONE_N(name) ZoneScopedN(name)
  #define OX_ZONE_NAME(txt, size) ZoneName(txt, size)
#else
  #define OX_SCOPED_ZONE
  #define OX_SCOPED_ZONE_N(name)
  #define OX_ZONE_NAME(txt, size)
#endif

#ifdef OX_DISTRIBUTION
  #undef MEMORY_PROFILER_ENABLED
  #define MEMORY_PROFILER_ENABLED 0
#endif

#if MEMORY_PROFILER_ENABLED
  #define OX_ALLOC(ptr, size) TracyAllocS(ptr, size, 10)
  #define OX_FREE(ptr) TracyFreeS(ptr, 10)
#else
  #define OX_ALLOC(ptr, size)
  #define OX_FREE(ptr)
#endif

#define TRACY_VK_USE_SYMBOL_TABLE

#include <tracy/Tracy.hpp>
#include <tracy/TracyVulkan.hpp>

namespace ox {
class VkContext;

class TracyProfiler {
public:
  TracyProfiler() = default;
  ~TracyProfiler() { destroy_context(); }

  void init_tracy_for_vulkan(VkContext* context);
  vuk::ProfilingCallbacks setup_vuk_callback();
  void destroy_context() const;

#ifdef TRACY_ENABLE
  tracy::VkCtx* get_graphics_ctx() const { return tracy_graphics_ctx; }
  tracy::VkCtx* get_transfer_ctx() const { return tracy_transfer_ctx; }
#endif

private:
#ifdef TRACY_ENABLE
  tracy::VkCtx* tracy_graphics_ctx;
  tracy::VkCtx* tracy_transfer_ctx;
#endif
  vuk::Unique<vuk::CommandPool> tracy_cpool;
  vuk::Unique<vuk::CommandBufferAllocation> tracy_cbufai;
};
} // namespace ox
