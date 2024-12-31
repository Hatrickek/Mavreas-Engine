#include "VkContext.hpp"

#include <sstream>
#include <vuk/ImageAttachment.hpp>
#include <vuk/runtime/ThisThreadExecutor.hpp>

#include "Render/Window.hpp"
#include "Utils/Log.hpp"
#include "Utils/Profiler.hpp"

#include "GLFW/glfw3.h"
#include "Render/RendererConfig.hpp"

namespace ox {
static VkBool32 debug_callback(const VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                               VkDebugUtilsMessageTypeFlagsEXT messageType,
                               const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                               void* pUserData) {
  std::string prefix;
  if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
    prefix = "VULKAN VERBOSE: ";
  } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
    prefix = "VULKAN INFO: ";
  } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    prefix = "VULKAN WARNING: ";
  } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    prefix = "VULKAN ERROR: ";
  }

  std::stringstream debug_message;
  debug_message << prefix << "[" << pCallbackData->messageIdNumber << "][" << pCallbackData->pMessageIdName << "] : " << pCallbackData->pMessage;

  if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
    OX_LOG_INFO("{}", debug_message.str());
  } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
    OX_LOG_INFO("{}", debug_message.str());
  } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    OX_LOG_WARN(debug_message.str().c_str());
    //OX_DEBUGBREAK();
  } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    OX_LOG_FATAL("{}", debug_message.str());
  }
  return VK_FALSE;
}

static vuk::Swapchain make_swapchain(vuk::Allocator allocator,
                                     vkb::Device vkbdevice,
                                     VkSurfaceKHR surface,
                                     std::optional<vuk::Swapchain> old_swapchain,
                                     vuk::PresentModeKHR present_mode) {
  vkb::SwapchainBuilder swb(vkbdevice);
  swb.set_desired_format(vuk::SurfaceFormatKHR{vuk::Format::eR8G8B8A8Unorm, vuk::ColorSpaceKHR::eSrgbNonlinear});
  swb.set_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
  swb.set_desired_present_mode((VkPresentModeKHR)present_mode);

  // TODO:
  // swb.set_desired_format(vuk::SurfaceFormatKHR{vuk::Format::eR8G8B8A8Srgb, vuk::ColorSpaceKHR::eSrgbNonlinear});
  // swb.add_fallback_format(vuk::SurfaceFormatKHR{vuk::Format::eB8G8R8A8Srgb, vuk::ColorSpaceKHR::eSrgbNonlinear});

  bool is_recycle = false;
  vkb::Result vkswapchain = {vkb::Swapchain{}};
  if (!old_swapchain) {
    vkswapchain = swb.build();
    old_swapchain.emplace(allocator, vkswapchain->image_count);
  } else {
    is_recycle = true;
    swb.set_old_swapchain(old_swapchain->swapchain);
    vkswapchain = swb.build();
  }

  if (is_recycle) {
    allocator.deallocate(std::span{&old_swapchain->swapchain, 1});
    for (auto& iv : old_swapchain->images) {
      allocator.deallocate(std::span{&iv.image_view, 1});
    }
  }

  auto images = *vkswapchain->get_images();
  auto views = *vkswapchain->get_image_views();

  old_swapchain->images.clear();

  for (uint32_t i = 0; i < (uint32_t)images.size(); i++) {
    vuk::ImageAttachment ia;
    ia.extent = {vkswapchain->extent.width, vkswapchain->extent.height, 1};
    ia.format = (vuk::Format)vkswapchain->image_format;
    ia.image = vuk::Image{images[i], nullptr};
    ia.image_view = vuk::ImageView{{0}, views[i]};
    ia.view_type = vuk::ImageViewType::e2D;
    ia.sample_count = vuk::Samples::e1;
    ia.base_level = ia.base_layer = 0;
    ia.level_count = ia.layer_count = 1;
    old_swapchain->images.push_back(ia);
  }

  old_swapchain->swapchain = vkswapchain->swapchain;
  old_swapchain->surface = surface;
  return std::move(*old_swapchain);
}

inline VkSurfaceKHR create_surface_glfw(const VkInstance instance, GLFWwindow* window) {
  VkSurfaceKHR surface = nullptr;
  const VkResult err = glfwCreateWindowSurface(instance, window, nullptr, &surface);
  if (err) {
    const char* error_msg;
    const int ret = glfwGetError(&error_msg);
    if (ret != 0) {
      OX_LOG_ERROR("GLFW error: {}", error_msg);
    }
    surface = nullptr;
  }
  return surface;
}

void VkContext::handle_resize(uint32 width, uint32 height) {
  if (width == 0 && height == 0) {
    suspend = true;
  } else {
    swapchain = make_swapchain(*superframe_allocator, vkb_device, swapchain->surface, std::move(swapchain), present_mode);
  }
}

void VkContext::set_vsync(bool enable) {
  const auto set_present_mode = enable ? vuk::PresentModeKHR::eFifo : vuk::PresentModeKHR::eImmediate;
  present_mode = set_present_mode;
}

bool VkContext::is_vsync() const { return present_mode == vuk::PresentModeKHR::eFifo; }

void VkContext::create_context(const AppSpec& spec) {
  OX_SCOPED_ZONE;
  vkb::InstanceBuilder builder;
  builder.set_app_name("Oxylus").set_engine_name("Oxylus").require_api_version(1, 3, 0).set_app_version(0, 1, 0);

  bool enable_validation = spec.command_line_args.contains("vulkan-validation");

  if (enable_validation) {
    OX_LOG_INFO("Enabled vulkan validation layers.");
    builder.request_validation_layers().set_debug_callback([](const VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                              const VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                              const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                              void* pUserData) -> VkBool32 {
      return debug_callback(messageSeverity, messageType, pCallbackData, pUserData);
    });
  }

  auto inst_ret = builder.build();
  if (!inst_ret) {
    OX_LOG_ERROR("Couldn't initialize the instance! Make sure your GPU drivers are up to date and it supports Vulkan 1.3");
  }

  vkb_instance = inst_ret.value();
  auto instance = vkb_instance.instance;
  vkb::PhysicalDeviceSelector selector{vkb_instance};
  surface = create_surface_glfw(instance, Window::get_glfw_window());
  selector.set_surface(surface)
    .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
    .require_present()
    .set_minimum_version(1, 2)
    .add_required_extension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME)
    .add_required_extension(VK_EXT_SHADER_VIEWPORT_INDEX_LAYER_EXTENSION_NAME);

  VkPhysicalDeviceFeatures2 vk10features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
  vk10features.features.geometryShader = true;
  vk10features.features.shaderInt64 = true;
  vk10features.features.shaderStorageImageWriteWithoutFormat = true;
  vk10features.features.depthClamp = true;
  vk10features.features.shaderStorageImageReadWithoutFormat = true;
  vk10features.features.fillModeNonSolid = true;
  vk10features.features.multiViewport = true;
  vk10features.features.samplerAnisotropy = true;
  vk10features.features.multiDrawIndirect = true;
  vk10features.features.shaderCullDistance = true;
  vk10features.features.shaderClipDistance = true;
  selector.set_required_features(vk10features.features);

  VkPhysicalDeviceVulkan11Features vk11features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
  vk11features.shaderDrawParameters = true;
  selector.set_required_features_11(vk11features);

  VkPhysicalDeviceVulkan12Features vk12features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
  vk12features.shaderInt8 = true;
  vk12features.uniformAndStorageBuffer8BitAccess = true;
  vk12features.storageBuffer8BitAccess = true;
  vk12features.timelineSemaphore = true;
  vk12features.descriptorBindingPartiallyBound = true;
  vk12features.descriptorBindingUpdateUnusedWhilePending = true;
  vk12features.runtimeDescriptorArray = true;
  vk12features.descriptorBindingVariableDescriptorCount = true;
  vk12features.hostQueryReset = true;
  vk12features.bufferDeviceAddress = true;
  vk12features.shaderOutputLayer = true;
  vk12features.descriptorIndexing = true;
  vk12features.samplerFilterMinmax = true;
  vk12features.shaderUniformBufferArrayNonUniformIndexing = true;
  vk12features.shaderSampledImageArrayNonUniformIndexing = true;
  vk12features.shaderStorageBufferArrayNonUniformIndexing = true;
  vk12features.shaderStorageImageArrayNonUniformIndexing = true;
  vk12features.shaderInputAttachmentArrayNonUniformIndexing = true;
  vk12features.shaderUniformTexelBufferArrayNonUniformIndexing = true;
  vk12features.shaderStorageTexelBufferArrayNonUniformIndexing = true;
  vk12features.shaderOutputViewportIndex = true;
  selector.set_required_features_12(vk12features);

  VkPhysicalDeviceSynchronization2FeaturesKHR sync_feat{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR,
                                                        .synchronization2 = true};

  selector.add_required_extension_features<>(sync_feat);

  auto phys_ret = selector.select();
  if (!phys_ret) {
    OX_LOG_ERROR("{}", phys_ret.full_error().type.message());
  } else {
    vkbphysical_device = phys_ret.value();
    device_name = phys_ret.value().name;
  }

  physical_device = vkbphysical_device.physical_device;
  vkb::DeviceBuilder device_builder{vkbphysical_device};

  auto dev_ret = device_builder.build();
  if (!dev_ret) {
    OX_LOG_ERROR("Couldn't create device");
  }
  vkb_device = dev_ret.value();
  graphics_queue = vkb_device.get_queue(vkb::QueueType::graphics).value();
  graphics_queue_family_index = vkb_device.get_queue_index(vkb::QueueType::graphics).value();
  transfer_queue = vkb_device.get_queue(vkb::QueueType::transfer).value();
  auto transfer_queue_family_index = vkb_device.get_queue_index(vkb::QueueType::transfer).value();
  device = vkb_device.device;
  vuk::FunctionPointers fps;
  fps.vkGetInstanceProcAddr = vkb_instance.fp_vkGetInstanceProcAddr;
  fps.vkGetDeviceProcAddr = vkb_instance.fp_vkGetDeviceProcAddr;
  fps.load_pfns(instance, device, true);
  std::vector<std::unique_ptr<vuk::Executor>> executors;

  executors.push_back(create_vkqueue_executor(fps, device, graphics_queue, graphics_queue_family_index, vuk::DomainFlagBits::eGraphicsQueue));
  executors.push_back(create_vkqueue_executor(fps, device, transfer_queue, transfer_queue_family_index, vuk::DomainFlagBits::eTransferQueue));
  executors.push_back(std::make_unique<vuk::ThisThreadExecutor>());

  runtime.emplace(vuk::RuntimeCreateParameters{instance, device, physical_device, std::move(executors), fps});

  set_vsync((bool)RendererCVar::cvar_vsync.get());

  superframe_resource.emplace(*runtime, num_inflight_frames);
  superframe_allocator.emplace(*superframe_resource);
  swapchain = make_swapchain(*superframe_allocator, vkb_device, surface, {}, present_mode);
  present_ready = vuk::Unique<std::array<VkSemaphore, 3>>(*superframe_allocator);
  render_complete = vuk::Unique<std::array<VkSemaphore, 3>>(*superframe_allocator);

  // runtime->set_shader_target_version(VK_API_VERSION_1_3);

  superframe_allocator->allocate_semaphores(*present_ready);
  superframe_allocator->allocate_semaphores(*render_complete);

  tracy_profiler = create_shared<TracyProfiler>();
  tracy_profiler->init_tracy_for_vulkan(this);

  Window::set_window_user_data(this);
  glfwSetWindowSizeCallback(Window::get_glfw_window(), [](GLFWwindow* window, const int width, const int height) {
    auto* ctx = reinterpret_cast<VkContext*>(glfwGetWindowUserPointer(window));
    ctx->handle_resize(width, height);
  });

  OX_LOG_INFO("Vulkan context initialized using device: {}", device_name);
}
} // namespace ox
