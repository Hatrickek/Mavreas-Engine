#include "Profiler.hpp"

#include "Core/PlatformDetection.hpp"
#include "Log.hpp"
#include "Render/Vulkan/VkContext.hpp"

namespace ox {
void TracyProfiler::init_tracy_for_vulkan(VkContext* context) {
#if !(GPU_PROFILER_ENABLED)
  return;
#endif
#ifdef TRACY_ENABLE
  VkCommandPoolCreateInfo cpci{.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT};
  cpci.queueFamilyIndex = context->graphics_queue_family_index;
  context->superframe_allocator->allocate_command_pools(std::span{&*tracy_cpool, 1}, std::span{&cpci, 1});
  vuk::CommandBufferAllocationCreateInfo ci{.command_pool = *tracy_cpool};
  context->superframe_allocator->allocate_command_buffers(std::span{&*tracy_cbufai, 1}, std::span{&ci, 1});
  tracy_graphics_ctx = TracyVkContextCalibrated(context->vkb_instance.instance,
                                                context->physical_device,
                                                context->device,
                                                context->graphics_queue,
                                                tracy_cbufai->command_buffer,
                                                context->vkb_instance.fp_vkGetInstanceProcAddr,
                                                context->vkb_instance.fp_vkGetDeviceProcAddr);
  tracy_transfer_ctx = TracyVkContextCalibrated(context->vkb_instance.instance,
                                                context->physical_device,
                                                context->device,
                                                context->graphics_queue,
                                                tracy_cbufai->command_buffer,
                                                context->vkb_instance.fp_vkGetInstanceProcAddr,
                                                context->vkb_instance.fp_vkGetDeviceProcAddr);
  OX_LOG_INFO("Tracy GPU profiler initialized.");
#endif
}
vuk::ProfilingCallbacks TracyProfiler::setup_vuk_callback() {
#if !(GPU_PROFILER_ENABLED)
  return vuk::ProfilingCallbacks{};
#endif
  vuk::ProfilingCallbacks cbs = {};
#ifdef TRACY_ENABLE
  cbs.user_data = this;
  cbs.on_begin_command_buffer = [](void* user_data, vuk::ExecutorTag tag, VkCommandBuffer cbuf) {
    const auto* tracy_profiler = reinterpret_cast<TracyProfiler*>(user_data);
    if ((tag.domain & vuk::DomainFlagBits::eQueueMask) != vuk::DomainFlagBits::eTransferQueue) {
      TracyVkCollect(tracy_profiler->get_graphics_ctx(), cbuf);
      TracyVkCollect(tracy_profiler->get_transfer_ctx(), cbuf);
    }
    return (void*)nullptr;
  };
  // runs whenever entering a new vuk::Pass
  // we start a GPU zone and then keep it open
  cbs.on_begin_pass = [](void* user_data, vuk::Name pass_name, VkCommandBuffer cmdbuf, vuk::DomainFlagBits domain) {
    const auto* tracy_profiler = reinterpret_cast<TracyProfiler*>(user_data);
    void* pass_data = new char[sizeof(tracy::VkCtxScope)];
    if (domain & vuk::DomainFlagBits::eGraphicsQueue) {
      new (pass_data) TracyVkZoneTransient(tracy_profiler->get_graphics_ctx(), , cmdbuf, pass_name.c_str(), true);
    } else if (domain & vuk::DomainFlagBits::eTransferQueue) {
      new (pass_data) TracyVkZoneTransient(tracy_profiler->get_transfer_ctx(), , cmdbuf, pass_name.c_str(), true);
    }

    return pass_data;
  };
  // runs whenever a pass has ended, we end the GPU zone we started
  cbs.on_end_pass = [](void* user_data, void* pass_data) {
    const auto tracy_scope = reinterpret_cast<tracy::VkCtxScope*>(pass_data);
    tracy_scope->~VkCtxScope();
  };
#endif
  return cbs;
}

void TracyProfiler::destroy_context() const {
#if GPU_PROFILER_ENABLED
  TracyVkDestroy(tracy_graphics_ctx) TracyVkDestroy(tracy_transfer_ctx)
#endif
}
} // namespace ox

#ifdef TRACY_ENABLE
void* operator new(std::size_t count) {
  const auto ptr = std::malloc(count);
  OX_ALLOC(ptr, count);
  return ptr;
}

void operator delete(void* ptr) noexcept {
  OX_FREE(ptr);
  std::free(ptr);
}

void* operator new[](std::size_t count) {
  const auto ptr = std::malloc(count);
  OX_ALLOC(ptr, count);
  return ptr;
}

void operator delete[](void* ptr) noexcept {
  OX_FREE(ptr);
  std::free(ptr);
}

void* operator new(std::size_t count, const std::nothrow_t&) noexcept {
  const auto ptr = std::malloc(count);
  OX_ALLOC(ptr, count);
  return ptr;
}

void operator delete(void* ptr, const std::nothrow_t&) noexcept {
  OX_FREE(ptr);
  std::free(ptr);
}

void* operator new[](std::size_t count, const std::nothrow_t&) noexcept {
  const auto ptr = std::malloc(count);
  OX_ALLOC(ptr, count);
  return ptr;
}

void operator delete[](void* ptr, const std::nothrow_t&) noexcept {
  OX_FREE(ptr);
  std::free(ptr);
}
#endif
