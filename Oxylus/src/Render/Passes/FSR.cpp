#include "FSR.hpp"

#include <glm/gtc/packing.hpp>
#include <vuk/RenderGraph.hpp>
#include <vuk/ShaderSource.hpp>
#include <vuk/runtime/CommandBuffer.hpp>
#include <vuk/runtime/vk/Pipeline.hpp>
#include <vuk/vsl/Core.hpp>

#include "Audio/AudioListener.hpp"
#include "Core/App.hpp"
#include "Core/FileSystem.hpp"
#include "Render/Camera.hpp"
#include "Thread/TaskScheduler.hpp"

#include "Render/Vulkan/VkContext.hpp"

#define FFX_FSR2_RESOURCE_IDENTIFIER_AUTO_EXPOSURE 28
#define FFX_FSR2_RESOURCE_IDENTIFIER_AUTO_EXPOSURE_MIPMAP_4 32
#define FFX_FSR2_SHADING_CHANGE_MIP_LEVEL (FFX_FSR2_RESOURCE_IDENTIFIER_AUTO_EXPOSURE_MIPMAP_4 - FFX_FSR2_RESOURCE_IDENTIFIER_AUTO_EXPOSURE)

#define FFX_FSR2_AUTOREACTIVEFLAGS_APPLY_TONEMAP 1
#define FFX_FSR2_AUTOREACTIVEFLAGS_APPLY_INVERSETONEMAP 2
#define FFX_FSR2_AUTOREACTIVEFLAGS_APPLY_THRESHOLD 4
#define FFX_FSR2_AUTOREACTIVEFLAGS_USE_COMPONENTS_MAX 8

#define LOCK_LIFETIME_REMAINING 0
#define LOCK_TEMPORAL_LUMA 1
#define LOCK_TRUST 2

static constexpr float FFX_PI = 3.141592653589793f;
/// An epsilon value for floating point numbers.
static constexpr float FFX_EPSILON = 1e-06f;

static constexpr int FFX_FSR2_MAXIMUM_BIAS_TEXTURE_WIDTH = 16;
static constexpr int FFX_FSR2_MAXIMUM_BIAS_TEXTURE_HEIGHT = 16;
static constexpr float ffxFsr2MaximumBias[] = {
  2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   1.876f, 1.809f, 1.772f, 1.753f, 1.748f, 2.0f,   2.0f,
  2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   1.869f, 1.801f, 1.764f, 1.745f, 1.739f, 2.0f,   2.0f,   2.0f,   2.0f,
  2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   1.976f, 1.841f, 1.774f, 1.737f, 1.716f, 1.71f,  2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,
  2.0f,   2.0f,   2.0f,   2.0f,   1.914f, 1.784f, 1.716f, 1.673f, 1.649f, 1.641f, 2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,
  2.0f,   2.0f,   1.793f, 1.676f, 1.604f, 1.562f, 1.54f,  1.533f, 2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   1.802f,
  1.619f, 1.536f, 1.492f, 1.467f, 1.454f, 1.449f, 2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   1.812f, 1.575f, 1.496f, 1.456f,
  1.432f, 1.416f, 1.408f, 1.405f, 2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   1.555f, 1.479f, 1.438f, 1.413f, 1.398f, 1.387f,
  1.381f, 1.379f, 2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   1.812f, 1.555f, 1.474f, 1.43f,  1.404f, 1.387f, 1.376f, 1.368f, 1.363f, 1.362f,
  2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   1.802f, 1.575f, 1.479f, 1.43f,  1.401f, 1.382f, 1.369f, 1.36f,  1.354f, 1.351f, 1.35f,  2.0f,   2.0f,
  1.976f, 1.914f, 1.793f, 1.619f, 1.496f, 1.438f, 1.404f, 1.382f, 1.367f, 1.357f, 1.349f, 1.344f, 1.341f, 1.34f,  1.876f, 1.869f, 1.841f, 1.784f,
  1.676f, 1.536f, 1.456f, 1.413f, 1.387f, 1.369f, 1.357f, 1.347f, 1.341f, 1.336f, 1.333f, 1.332f, 1.809f, 1.801f, 1.774f, 1.716f, 1.604f, 1.492f,
  1.432f, 1.398f, 1.376f, 1.36f,  1.349f, 1.341f, 1.335f, 1.33f,  1.328f, 1.327f, 1.772f, 1.764f, 1.737f, 1.673f, 1.562f, 1.467f, 1.416f, 1.387f,
  1.368f, 1.354f, 1.344f, 1.336f, 1.33f,  1.326f, 1.323f, 1.323f, 1.753f, 1.745f, 1.716f, 1.649f, 1.54f,  1.454f, 1.408f, 1.381f, 1.363f, 1.351f,
  1.341f, 1.333f, 1.328f, 1.323f, 1.321f, 1.32f,  1.748f, 1.739f, 1.71f,  1.641f, 1.533f, 1.449f, 1.405f, 1.379f, 1.362f, 1.35f,  1.34f,  1.332f,
  1.327f, 1.323f, 1.32f,  1.319f,

};

namespace ox {
static float lanczos2(float value) {
  return abs(value) < FFX_EPSILON ? 1.f : sinf(FFX_PI * value) / (FFX_PI * value) * (sinf(0.5f * FFX_PI * value) / (0.5f * FFX_PI * value));
}
// Calculate halton number for index and base.
static float halton(int32_t index, int32_t base) {
  float f = 1.0f, result = 0.0f;

  for (int32_t currentIndex = index; currentIndex > 0;) {

    f /= (float)base;
    result = result + f * (float)(currentIndex % base);
    currentIndex = (uint32_t)floorf((float)currentIndex / (float)base);
  }

  return result;
}

int32_t ffxFsr2GetJitterPhaseCount(int32_t render_width, int32_t display_width) {
  constexpr float base_phase_count = 8.0f;
  const int32_t jitter_phase_count = int32_t(base_phase_count * pow(float(display_width) / render_width, 2.0f));
  return jitter_phase_count;
}

void SpdSetup(uint2& dispatchThreadGroupCountXY,             // CPU side: dispatch thread group count xy
              uint2& workGroupOffset,                        // GPU side: pass in as constant
              uint2& numWorkGroupsAndMips,                   // GPU side: pass in as constant
              UVec4 rectInfo,                                // left, top, width, height
              int32_t mips)                                  // optional: if -1, calculate based on rect width and height
{
  workGroupOffset[0] = rectInfo[0] / 64;                     // rectInfo[0] = left
  workGroupOffset[1] = rectInfo[1] / 64;                     // rectInfo[1] = top

  uint32_t endIndexX = (rectInfo[0] + rectInfo[2] - 1) / 64; // rectInfo[0] = left, rectInfo[2] = width
  uint32_t endIndexY = (rectInfo[1] + rectInfo[3] - 1) / 64; // rectInfo[1] = top, rectInfo[3] = height

  dispatchThreadGroupCountXY[0] = endIndexX + 1 - workGroupOffset[0];
  dispatchThreadGroupCountXY[1] = endIndexY + 1 - workGroupOffset[1];

  numWorkGroupsAndMips[0] = (dispatchThreadGroupCountXY[0]) * (dispatchThreadGroupCountXY[1]);

  if (mips >= 0) {
    numWorkGroupsAndMips[1] = uint32_t(mips);
  } else {
    // calculate based on rect width and height
    uint32_t resolution = std::max(rectInfo[2], rectInfo[3]);
    numWorkGroupsAndMips[1] = uint32_t((std::min(floor(log2(float(resolution))), float(12))));
  }
}

void SpdSetup(uint2& dispatchThreadGroupCountXY, // CPU side: dispatch thread group count xy
              uint2& workGroupOffset,            // GPU side: pass in as constant
              uint2& numWorkGroupsAndMips,       // GPU side: pass in as constant
              UVec4 rectInfo)                    // left, top, width, height
{
  SpdSetup(dispatchThreadGroupCountXY, workGroupOffset, numWorkGroupsAndMips, rectInfo, -1);
}

void FsrRcasCon(UVec4& con, float sharpness) {
  // Transform from stops to linear value.
  sharpness = exp2(-sharpness);
  Vec2 hSharp = {sharpness, sharpness};
  con[0] = uint32_t(sharpness);
  con[1] = packHalf2x16(hSharp);
  con[2] = 0;
  con[3] = 0;
}

Vec2 FSR::get_jitter() const {
  const int32_t phase_count = ffxFsr2GetJitterPhaseCount(fsr2_constants.renderSize[0], fsr2_constants.displaySize[0]);
  float x = halton(fsr2_constants.frameIndex % phase_count + 1, 2) - 0.5f;
  float y = halton(fsr2_constants.frameIndex % phase_count + 1, 3) - 0.5f;
  x = 2 * x / (float)fsr2_constants.renderSize[0];
  y = -2 * y / (float)fsr2_constants.renderSize[1];
  return {x, y};
}

void FSR::load_pipelines(vuk::Allocator& allocator, vuk::PipelineBaseCreateInfo& pipeline_ci) {
#define SHADER_FILE(path) fs::read_shader_file(path), fs::get_shader_path(path)

  auto* task_scheduler = App::get_system<TaskScheduler>();

  task_scheduler->add_task([=]() mutable {
    vuk::PipelineBaseCreateInfo ci;
    ci.add_hlsl(SHADER_FILE("FFX/FSR2/ffx_fsr2_autogen_reactive_pass.hlsl"), vuk::HlslShaderStage::eCompute);
    TRY(allocator.get_context().create_named_pipeline("autogen_reactive_pass", ci))
  });

  task_scheduler->add_task([=]() mutable {
    vuk::PipelineBaseCreateInfo ci;
    ci.add_hlsl(SHADER_FILE("FFX/FSR2/ffx_fsr2_compute_luminance_pyramid_pass.hlsl"), vuk::HlslShaderStage::eCompute);
    TRY(allocator.get_context().create_named_pipeline("luminance_pyramid_pass", ci))
  });

  task_scheduler->add_task([=]() mutable {
    vuk::PipelineBaseCreateInfo ci;
    ci.add_hlsl(SHADER_FILE("FFX/FSR2/ffx_fsr2_prepare_input_color_pass.hlsl"), vuk::HlslShaderStage::eCompute);
    TRY(allocator.get_context().create_named_pipeline("prepare_input_color_pass", ci))
  });

  task_scheduler->add_task([=]() mutable {
    vuk::PipelineBaseCreateInfo ci;
    ci.add_hlsl(SHADER_FILE("FFX/FSR2/ffx_fsr2_reconstruct_previous_depth_pass.hlsl"), vuk::HlslShaderStage::eCompute);
    TRY(allocator.get_context().create_named_pipeline("reconstruct_previous_depth_pass", ci))
  });

  task_scheduler->add_task([=]() mutable {
    vuk::PipelineBaseCreateInfo ci;
    ci.add_hlsl(SHADER_FILE("FFX/FSR2/ffx_fsr2_depth_clip_pass.hlsl"), vuk::HlslShaderStage::eCompute);
    TRY(allocator.get_context().create_named_pipeline("depth_clip_pass", ci))
  });

  task_scheduler->add_task([=]() mutable {
    vuk::PipelineBaseCreateInfo ci;
    ci.add_hlsl(SHADER_FILE("FFX/FSR2/ffx_fsr2_lock_pass.hlsl"), vuk::HlslShaderStage::eCompute);
    TRY(allocator.get_context().create_named_pipeline("lock_pass", ci))
  });

  task_scheduler->add_task([=]() mutable {
    vuk::PipelineBaseCreateInfo ci;
    ci.add_hlsl(SHADER_FILE("FFX/FSR2/ffx_fsr2_accumulate_pass.hlsl"), vuk::HlslShaderStage::eCompute);
    TRY(allocator.get_context().create_named_pipeline("accumulate_pass", ci))
  });

  task_scheduler->add_task([=]() mutable {
    vuk::PipelineBaseCreateInfo ci;
    ci.add_hlsl(SHADER_FILE("FFX/FSR2/ffx_fsr2_rcas_pass.hlsl"), vuk::HlslShaderStage::eCompute);
    TRY(allocator.get_context().create_named_pipeline("rcas_pass", ci))
  });

  task_scheduler->wait_for_all();
}

void FSR::create_fs2_resources(vuk::Extent3D render_resolution, vuk::Extent3D presentation_resolution) {
  _render_res = render_resolution;
  _present_res = presentation_resolution;

  constexpr uint32_t lanczos2_lut_width = 128;
  int16_t lanczos2_weights[lanczos2_lut_width] = {};

  for (uint32_t index = 0; index < lanczos2_lut_width; index++) {
    const float x = 2.0f * (float)index / float(lanczos2_lut_width - 1);
    const float y = lanczos2(x);
    lanczos2_weights[index] = int16_t(roundf(y * 32767.0f));
  }

  lanczos_lut.create_texture({lanczos2_lut_width, 1u, 1u}, &lanczos2_weights, vuk::Format::eR16Snorm, Preset::eSTT2DUnmipped);

  // upload path only supports R16_SNORM, let's go and convert
  int16_t maximum_bias[FFX_FSR2_MAXIMUM_BIAS_TEXTURE_WIDTH * FFX_FSR2_MAXIMUM_BIAS_TEXTURE_HEIGHT];
  for (uint32_t i = 0; i < FFX_FSR2_MAXIMUM_BIAS_TEXTURE_WIDTH * FFX_FSR2_MAXIMUM_BIAS_TEXTURE_HEIGHT; ++i) {
    maximum_bias[i] = int16_t(roundf(ffxFsr2MaximumBias[i] / 2.0f * 32767.0f));
  }

  maximum_bias_lut.create_texture({(uint32_t)FFX_FSR2_MAXIMUM_BIAS_TEXTURE_WIDTH, (uint32_t)FFX_FSR2_MAXIMUM_BIAS_TEXTURE_HEIGHT, 1u},
                                  maximum_bias,
                                  vuk::Format::eR16Snorm,
                                  Preset::eSTT2DUnmipped);

  fsr2_constants.renderSize[0] = render_resolution.width;
  fsr2_constants.renderSize[1] = render_resolution.height;
  fsr2_constants.displaySize[0] = presentation_resolution.width;
  fsr2_constants.displaySize[1] = presentation_resolution.height;
  fsr2_constants.displaySizeRcp[0] = 1.0f / presentation_resolution.width;
  fsr2_constants.displaySizeRcp[1] = 1.0f / presentation_resolution.height;

  adjusted_color.create_texture({render_resolution.width, render_resolution.height, 1u}, vuk::Format::eR16G16B16A16Unorm, Preset::eSTT2DUnmipped);
  exposure.create_texture({1u, 1u, 1u}, vuk::Format::eR32G32Sfloat, Preset::eSTT2DUnmipped);

  vuk::ImageAttachment luminance_ia = vuk::ImageAttachment::from_preset(Preset::eSTT2DUnmipped,
                                                                        vuk::Format::eR32G32Sfloat,
                                                                        {render_resolution.width / 2u, render_resolution.height / 2u, 1u},
                                                                        vuk::Samples::e1);
  luminance_ia.level_count = Texture::get_mip_count(luminance_ia.extent);
  luminance_current.create_texture(luminance_ia);

  luminance_history.create_texture({render_resolution.width, render_resolution.height, 1u}, vuk::Format::eR8G8B8A8Unorm, Preset::eSTT2DUnmipped);
  previous_depth.create_texture({render_resolution.width, render_resolution.height, 1u}, vuk::Format::eR32Uint, Preset::eSTT2DUnmipped);
  dilated_depth.create_texture({render_resolution.width, render_resolution.height, 1u}, vuk::Format::eR16Sfloat, Preset::eSTT2DUnmipped);
  dilated_motion.create_texture({render_resolution.width, render_resolution.height, 1u}, vuk::Format::eR16G16Sfloat, Preset::eSTT2DUnmipped);
  dilated_reactive.create_texture({render_resolution.width, render_resolution.height, 1u}, vuk::Format::eR8G8Unorm, Preset::eSTT2DUnmipped);
  disocclusion_mask.create_texture({render_resolution.width, render_resolution.height, 1u}, vuk::Format::eR8Unorm, Preset::eSTT2DUnmipped);
  reactive_mask.create_texture({render_resolution.width, render_resolution.height, 1u}, vuk::Format::eR8Unorm, Preset::eSTT2DUnmipped);
  lock_status[0].create_texture({presentation_resolution.width, presentation_resolution.height, 1u},
                                vuk::Format::eB10G11R11UfloatPack32,
                                Preset::eSTT2DUnmipped);
  lock_status[1].create_texture({presentation_resolution.width, presentation_resolution.height, 1u},
                                vuk::Format::eB10G11R11UfloatPack32,
                                Preset::eSTT2DUnmipped);
  output_internal[0].create_texture({presentation_resolution.width, presentation_resolution.height, 1u},
                                    vuk::Format::eR16G16B16A16Sfloat,
                                    Preset::eSTT2DUnmipped);
  output_internal[1].create_texture({presentation_resolution.width, presentation_resolution.height, 1u},
                                    vuk::Format::eR16G16B16A16Sfloat,
                                    Preset::eSTT2DUnmipped);
  spd_global_atomic.create_texture({1u, 1u, 1u}, vuk::Format::eR32Uint, Preset::eSTT2DUnmipped);
}

vuk::Value<vuk::ImageAttachment> FSR::dispatch(vuk::Value<vuk::ImageAttachment>& input_color_post_alpha,
                                               vuk::Value<vuk::ImageAttachment>& input_color_pre_alpha,
                                               vuk::Value<vuk::ImageAttachment>& output,
                                               vuk::Value<vuk::ImageAttachment>& input_depth,
                                               vuk::Value<vuk::ImageAttachment>& input_velocity,
                                               Camera& camera,
                                               double dt,
                                               float sharpness,
                                               uint32_t frame_index) {
  struct Fsr2SpdConstants {
    uint32_t mips;
    uint32_t numworkGroups;
    uint32_t workGroupOffset[2];
    uint32_t renderSize[2];
  };
  struct Fsr2RcasConstants {
    UVec4 rcasConfig;
  };

  fsr2_constants.jitterOffset[0] = camera.get_jitter().x * fsr2_constants.renderSize[0] * 0.5f;
  fsr2_constants.jitterOffset[1] = camera.get_jitter().y * fsr2_constants.renderSize[1] * -0.5f;

  // compute the horizontal FOV for the shader from the vertical one.
  const float aspectRatio = (float)fsr2_constants.renderSize[0] / (float)fsr2_constants.renderSize[1];
  const float cameraAngleHorizontal = atan(tan(camera.get_fov() / 2) * aspectRatio) * 2;
  fsr2_constants.tanHalfFOV = tanf(cameraAngleHorizontal * 0.5f);

  // reversed depth
  fsr2_constants.deviceToViewDepth[0] = FLT_EPSILON;
  fsr2_constants.deviceToViewDepth[1] = -1.00000000f;
  fsr2_constants.deviceToViewDepth[2] = 0.100000001f;
  fsr2_constants.deviceToViewDepth[3] = FLT_EPSILON;

  // To be updated if resource is larger than the actual image size
  fsr2_constants.depthClipUVScale[0] = float(fsr2_constants.renderSize[0]) / disocclusion_mask.get_extent().width;
  fsr2_constants.depthClipUVScale[1] = float(fsr2_constants.renderSize[1]) / disocclusion_mask.get_extent().height;
  fsr2_constants.postLockStatusUVScale[0] = float(fsr2_constants.displaySize[0]) / lock_status[0].get_extent().width;
  fsr2_constants.postLockStatusUVScale[1] = float(fsr2_constants.displaySize[1]) / lock_status[0].get_extent().height;
  fsr2_constants.reactiveMaskDimRcp[0] = 1.0f / float(reactive_mask.get_extent().width);
  fsr2_constants.reactiveMaskDimRcp[1] = 1.0f / float(reactive_mask.get_extent().height);
  fsr2_constants.downscaleFactor[0] = float(fsr2_constants.renderSize[0]) / float(fsr2_constants.displaySize[0]);
  fsr2_constants.downscaleFactor[1] = float(fsr2_constants.renderSize[1]) / float(fsr2_constants.displaySize[1]);
  static float preExposure = 0;
  fsr2_constants.preExposure = preExposure != 0 ? preExposure : 1.0f;

  // motion vector data
  const bool enable_display_resolution_motion_vectors = false;
  const int32_t* motionVectorsTargetSize = enable_display_resolution_motion_vectors ? fsr2_constants.displaySize : fsr2_constants.renderSize;

  fsr2_constants.motionVectorScale[0] = 1;
  fsr2_constants.motionVectorScale[1] = 1;

  // lock data, assuming jitter sequence length computation for now
  const int32_t jitterPhaseCount = ffxFsr2GetJitterPhaseCount(fsr2_constants.renderSize[0], fsr2_constants.displaySize[0]);

  static const float lockInitialLifetime = 1.0f;
  fsr2_constants.lockInitialLifetime = lockInitialLifetime;

  // init on first frame
  const bool resetAccumulation = fsr2_constants.frameIndex == 0;
  if (resetAccumulation || fsr2_constants.jitterPhaseCount == 0) {
    fsr2_constants.jitterPhaseCount = (float)jitterPhaseCount;
  } else {
    const int32_t jitterPhaseCountDelta = (int32_t)(jitterPhaseCount - fsr2_constants.jitterPhaseCount);
    if (jitterPhaseCountDelta > 0) {
      fsr2_constants.jitterPhaseCount++;
    } else if (jitterPhaseCountDelta < 0) {
      fsr2_constants.jitterPhaseCount--;
    }
  }

  const int32_t maxLockFrames = (int32_t)fsr2_constants.jitterPhaseCount + 1;
  fsr2_constants.lockTickDelta = lockInitialLifetime / maxLockFrames;

  // convert delta time to seconds and clamp to [0, 1].
  // context->constants.deltaTime = std::max(0.0f, std::min(1.0f, params->frameTimeDelta / 1000.0f));
  fsr2_constants.deltaTime = std::max(0.0f, std::min(1.0f, (float)dt / 1000.0f));

  fsr2_constants.frameIndex++;

  // shading change usage of the SPD mip levels.
  fsr2_constants.lumaMipLevelToUse = uint32_t(FFX_FSR2_SHADING_CHANGE_MIP_LEVEL);

  const float mipDiv = float(2 << fsr2_constants.lumaMipLevelToUse);
  fsr2_constants.lumaMipDimensions[0] = uint32_t(fsr2_constants.renderSize[0] / mipDiv);
  fsr2_constants.lumaMipDimensions[1] = uint32_t(fsr2_constants.renderSize[1] / mipDiv);
  fsr2_constants.lumaMipRcp = float(fsr2_constants.lumaMipDimensions[0] * fsr2_constants.lumaMipDimensions[1]) /
                              float(fsr2_constants.renderSize[0] * fsr2_constants.renderSize[1]);

  // reactive mask bias
  const int32_t threadGroupWorkRegionDim = 8;
  const int32_t dispatch_src_x = (fsr2_constants.renderSize[0] + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;
  const int32_t dispatch_src_y = (fsr2_constants.renderSize[1] + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;
  const int32_t dispatch_dst_x = (fsr2_constants.displaySize[0] + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;
  const int32_t dispatch_dst_y = (fsr2_constants.displaySize[1] + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;

  // Auto exposure
  uint2 dispatch_thread_group_count_xy;
  uint2 workGroupOffset;
  uint2 numWorkGroupsAndMips;
  UVec4 rectInfo = {0, 0, (uint32_t)fsr2_constants.renderSize[0], (uint32_t)fsr2_constants.renderSize[1]};
  SpdSetup(dispatch_thread_group_count_xy, workGroupOffset, numWorkGroupsAndMips, rectInfo);

  // downsample
  Fsr2SpdConstants luminance_pyramid_constants;
  luminance_pyramid_constants.numworkGroups = numWorkGroupsAndMips[0];
  luminance_pyramid_constants.mips = numWorkGroupsAndMips[1];
  luminance_pyramid_constants.workGroupOffset[0] = workGroupOffset[0];
  luminance_pyramid_constants.workGroupOffset[1] = workGroupOffset[1];
  luminance_pyramid_constants.renderSize[0] = fsr2_constants.renderSize[0];
  luminance_pyramid_constants.renderSize[1] = fsr2_constants.renderSize[1];

  // compute the constants.
  Fsr2RcasConstants rcasConsts = {};
  const float sharpenessRemapped = -2.0f * sharpness + 2.0f;
  FsrRcasCon(rcasConsts.rcasConfig, sharpenessRemapped);

  auto adjusted_color_ia = vuk::acquire_ia("adjusted_color", adjusted_color.as_attachment(), vuk::eNone);
  auto luminance_current_ia = vuk::acquire_ia("luminance_current", luminance_current.as_attachment(), vuk::eNone);
  auto luminance_history_ia = vuk::acquire_ia("luminance_history", luminance_history.as_attachment(), vuk::eNone);
  auto exposure_ia = vuk::acquire_ia("exposure", exposure.as_attachment(), vuk::eNone);
  auto previous_depth_ia = vuk::acquire_ia("previous_depth", previous_depth.as_attachment(), vuk::eNone);
  auto dilated_depth_ia = vuk::acquire_ia("dilated_depth", dilated_depth.as_attachment(), vuk::eNone);
  auto dilated_motion_ia = vuk::acquire_ia("dilated_motion", dilated_motion.as_attachment(), vuk::eNone);
  auto dilated_reactive_ia = vuk::acquire_ia("dilated_reactive", dilated_reactive.as_attachment(), vuk::eNone);
  auto disocclusion_mask_ia = vuk::acquire_ia("disocclusion_mask", disocclusion_mask.as_attachment(), vuk::eNone);
  auto reactive_mask_ia = vuk::acquire_ia("reactive_mask", reactive_mask.as_attachment(), vuk::eNone);
  auto spd_global_atomic_ia = vuk::acquire_ia("spd_global_atomic", spd_global_atomic.as_attachment(), vuk::eNone);

  vuk::Value<vuk::ImageAttachment> output_ints[2] = {vuk::acquire_ia("output_internal0", output_internal[0].as_attachment(), vuk::eNone),
                                                     vuk::acquire_ia("output_internal1", output_internal[1].as_attachment(), vuk::eNone)};

  vuk::Value<vuk::ImageAttachment> locks[2] = {vuk::acquire_ia("lock_status0", lock_status[0].as_attachment(), vuk::eNone),
                                               vuk::acquire_ia("lock_status1", lock_status[1].as_attachment(), vuk::eNone)};

  if (resetAccumulation) {
    adjusted_color_ia = vuk::clear_image(adjusted_color_ia, vuk::Black<float>);
    luminance_current_ia = vuk::clear_image(luminance_current_ia, vuk::Black<float>);
    luminance_history_ia = vuk::clear_image(luminance_history_ia, vuk::Black<float>);
    exposure_ia = vuk::clear_image(exposure_ia, vuk::Black<float>);
    previous_depth_ia = vuk::clear_image(previous_depth_ia, vuk::Black<float>);
    dilated_depth_ia = vuk::clear_image(dilated_depth_ia, vuk::Black<float>);
    dilated_motion_ia = vuk::clear_image(dilated_motion_ia, vuk::Black<float>);
    dilated_reactive_ia = vuk::clear_image(dilated_reactive_ia, vuk::Black<float>);
    disocclusion_mask_ia = vuk::clear_image(disocclusion_mask_ia, vuk::Black<float>);
    reactive_mask_ia = vuk::clear_image(reactive_mask_ia, vuk::Black<float>);
    output_ints[0] = vuk::clear_image(output_ints[0], vuk::Black<float>);
    output_ints[1] = vuk::clear_image(output_ints[1], vuk::Black<float>);
    spd_global_atomic_ia = vuk::clear_image(spd_global_atomic_ia, vuk::Black<float>);

    float clearValuesLockStatus[4]{};
    clearValuesLockStatus[LOCK_LIFETIME_REMAINING] = lockInitialLifetime * 2.0f;
    clearValuesLockStatus[LOCK_TEMPORAL_LUMA] = 0.0f;
    clearValuesLockStatus[LOCK_TRUST] = 1.0f;
    uint32_t clear_lock_pk = glm::packF2x11_1x10(Vec3(clearValuesLockStatus[0], clearValuesLockStatus[1], clearValuesLockStatus[2]));
    locks[0] = vuk::clear_image(locks[0], vuk::Clear{vuk::ClearColor(clear_lock_pk, clear_lock_pk, clear_lock_pk, clear_lock_pk)});
    locks[1] = vuk::clear_image(locks[1], vuk::Clear{vuk::ClearColor(clear_lock_pk, clear_lock_pk, clear_lock_pk, clear_lock_pk)});
  }

  const int r_idx = fsr2_constants.frameIndex % 2;
  const int rw_idx = (fsr2_constants.frameIndex + 1) % 2;

  const auto& r_lock = locks[r_idx];
  const auto& rw_lock = locks[rw_idx];
  const auto& r_output = output_ints[r_idx];
  const auto& rw_output = output_ints[rw_idx];

  auto gen_reactive_mask = vuk::make_pass("gen_reactive_mask",
                                          [this](vuk::CommandBuffer& command_buffer,
                                                 VUK_IA(vuk::eComputeRW) output,
                                                 VUK_IA(vuk::eComputeSampled) _input_color_pre_alpha,
                                                 VUK_IA(vuk::eComputeSampled) _input_color_post_alpha) {
    command_buffer.bind_compute_pipeline("autogen_reactive_pass")
      .bind_image(0, 0, _input_color_pre_alpha)
      .bind_image(0, 1, _input_color_post_alpha)
      .bind_image(0, 2, output);

    struct Fsr2GenerateReactiveConstants {
      float scale;
      float threshold;
      float binaryValue;
      uint32_t flags;
    };
    Fsr2GenerateReactiveConstants reactive_constants;
    static float scale = 1.0f;
    static float threshold = 0.2f;
    static float binaryValue = 0.9f;
    reactive_constants.scale = scale;
    reactive_constants.threshold = threshold;
    reactive_constants.binaryValue = binaryValue;
    reactive_constants.flags = 0;
    reactive_constants.flags |= FFX_FSR2_AUTOREACTIVEFLAGS_APPLY_TONEMAP;
    // constants.flags |= FFX_FSR2_AUTOREACTIVEFLAGS_APPLY_INVERSETONEMAP;
    // constants.flags |= FFX_FSR2_AUTOREACTIVEFLAGS_USE_COMPONENTS_MAX;
    // constants.flags |= FFX_FSR2_AUTOREACTIVEFLAGS_APPLY_THRESHOLD;

    auto* buff = command_buffer.scratch_buffer<Fsr2GenerateReactiveConstants>(0, 3);
    *buff = reactive_constants;

    auto* constants = command_buffer.scratch_buffer<Fsr2Constants>(0, 4);
    *constants = fsr2_constants;

    constexpr int32_t thread_group_work_region_dim = 8;
    const int32_t src_x = (fsr2_constants.renderSize[0] + (thread_group_work_region_dim - 1)) / thread_group_work_region_dim;
    const int32_t src_y = (fsr2_constants.renderSize[1] + (thread_group_work_region_dim - 1)) / thread_group_work_region_dim;

    command_buffer.dispatch(src_x, src_y, 1);

    return output;
  });

  vuk::Value<vuk::ImageAttachment> reactive_mask_output = gen_reactive_mask(reactive_mask_ia, input_color_pre_alpha, input_color_post_alpha);

  auto luminance_pyramid_pass = vuk::make_pass("luminance_pyramid",
                                               [this,
                                                luminance_pyramid_constants,
                                                dispatch_thread_group_count_xy](vuk::CommandBuffer& command_buffer,
                                                                                VUK_IA(vuk::eComputeSampled) _input_color_post_alpha,
                                                                                VUK_IA(vuk::eComputeRW) _spd_global,
                                                                                VUK_IA(vuk::eComputeRW) _luminance_curr,
                                                                                VUK_IA(vuk::eComputeRW) _luminance_mip5,
                                                                                VUK_IA(vuk::eComputeRW) _exposure) {
    command_buffer.bind_compute_pipeline("luminance_pyramid_pass");

    auto* constants = command_buffer.scratch_buffer<Fsr2Constants>(0, 0);
    *constants = fsr2_constants;

    auto* spdconstants = command_buffer.scratch_buffer<Fsr2SpdConstants>(0, 1);
    *spdconstants = luminance_pyramid_constants;

    command_buffer.bind_image(0, 2, _input_color_post_alpha)
      .bind_image(0, 3, _spd_global)
      .bind_image(0, 4, _luminance_curr)
      .bind_image(0, 5, _luminance_mip5)
      .bind_image(0, 6, _exposure)
      .dispatch(dispatch_thread_group_count_xy[0], dispatch_thread_group_count_xy[1], 1);

    return std::make_tuple(_spd_global, _luminance_curr, _luminance_mip5, _exposure);
  });

  auto [sdp_global_output, luminance_current_output, luminance_mip5, exposure_output] = luminance_pyramid_pass(input_color_post_alpha,
                                                                                                               spd_global_atomic_ia,
                                                                                                               luminance_current_ia,
                                                                                                               luminance_current_ia.mip(5),
                                                                                                               exposure_ia);

  auto adjust_input_color_pass = vuk::make_pass("adjust_input_color",
                                                [dispatch_src_x, dispatch_src_y, this](vuk::CommandBuffer& command_buffer,
                                                                                       VUK_IA(vuk::eComputeSampled) _input_color_post_alpha,
                                                                                       VUK_IA(vuk::eComputeSampled) _exposure,
                                                                                       VUK_IA(vuk::eComputeRW) _previous_depth,
                                                                                       VUK_IA(vuk::eComputeRW) _luminance_history,
                                                                                       VUK_IA(vuk::eComputeRW) _adjusted_color) {
    command_buffer.bind_compute_pipeline("prepare_input_color_pass")
      .bind_image(0, 0, _input_color_post_alpha)
      .bind_image(0, 1, _exposure)
      .bind_image(0, 2, _previous_depth)
      .bind_image(0, 3, _adjusted_color)
      .bind_image(0, 4, _luminance_history);

    auto* constants = command_buffer.scratch_buffer<Fsr2Constants>(0, 5);
    *constants = fsr2_constants;

    command_buffer.dispatch(dispatch_src_x, dispatch_src_y, 1);

    return std::make_tuple(_previous_depth, _luminance_history, _adjusted_color);
  });

  auto [prev_depth_output, luminance_history_output, adjusted_color_output] = adjust_input_color_pass(input_color_post_alpha,
                                                                                                      exposure_output,
                                                                                                      previous_depth_ia,
                                                                                                      luminance_history_ia,
                                                                                                      adjusted_color_ia);

  auto reconstruct_dilate_pass = vuk::make_pass("reconstruct_dilate",
                                                [this, dispatch_src_y, dispatch_src_x](vuk::CommandBuffer& command_buffer,
                                                                                       VUK_IA(vuk::eComputeSampled) _input_velocity,
                                                                                       VUK_IA(vuk::eComputeSampled) _input_depth,
                                                                                       VUK_IA(vuk::eComputeSampled) _reactive_mask,
                                                                                       VUK_IA(vuk::eComputeSampled) _input_post_alpha,
                                                                                       VUK_IA(vuk::eComputeSampled) _adjusted_color,
                                                                                       VUK_IA(vuk::eComputeRW) _previous_depth,
                                                                                       VUK_IA(vuk::eComputeRW) _dilated_motion,
                                                                                       VUK_IA(vuk::eComputeRW) _dilated_depth,
                                                                                       VUK_IA(vuk::eComputeRW) _dilated_reactive) {
    command_buffer.bind_compute_pipeline("reconstruct_previous_depth_pass")
      .bind_image(0, 0, _input_velocity)
      .bind_image(0, 1, _input_depth)
      .bind_image(0, 2, _reactive_mask)
      .bind_image(0, 3, _input_post_alpha)
      .bind_image(0, 4, _adjusted_color)
      .bind_image(0, 5, _previous_depth)
      .bind_image(0, 6, _dilated_motion)
      .bind_image(0, 7, _dilated_depth)
      .bind_image(0, 8, _dilated_reactive);

    auto* constants = command_buffer.scratch_buffer<Fsr2Constants>(0, 9);
    *constants = fsr2_constants;

    command_buffer.dispatch(dispatch_src_x, dispatch_src_y, 1);

    return std::make_tuple(_previous_depth, _dilated_motion, _dilated_depth, _dilated_reactive);
  });

  auto [previous_depth_output, dilated_motion_output, dilated_depth_output, dilated_reactive_output] = reconstruct_dilate_pass(input_velocity,
                                                                                                                               input_depth,
                                                                                                                               reactive_mask_output,
                                                                                                                               input_color_post_alpha,
                                                                                                                               adjusted_color_output,
                                                                                                                               previous_depth_ia,
                                                                                                                               dilated_motion_ia,
                                                                                                                               dilated_depth_ia,
                                                                                                                               dilated_reactive_ia);

  auto depth_clip_pass = vuk::make_pass("depth_clip",
                                        [this, dispatch_src_x, dispatch_src_y](vuk::CommandBuffer& command_buffer,
                                                                               VUK_IA(vuk::eComputeSampled) _previous_depth,
                                                                               VUK_IA(vuk::eComputeSampled) _dilated_motion,
                                                                               VUK_IA(vuk::eComputeSampled) _dilated_depth,
                                                                               VUK_IA(vuk::eComputeRW) _disocclusion_mask) {
    command_buffer.bind_compute_pipeline("depth_clip_pass")
      .bind_image(0, 0, _previous_depth)
      .bind_image(0, 1, _dilated_motion)
      .bind_image(0, 2, _dilated_depth)
      .bind_image(0, 3, _disocclusion_mask);

    auto* constants = command_buffer.scratch_buffer<Fsr2Constants>(0, 4);
    *constants = fsr2_constants;

    command_buffer.dispatch(dispatch_src_x, dispatch_src_y, 1);

    return _disocclusion_mask;
  });

  auto disocclusion_mask_output = depth_clip_pass(previous_depth_output, dilated_motion_output, dilated_depth_output, disocclusion_mask_ia);

  auto create_locks_pass = vuk::make_pass("create_locks",
                                          [dispatch_src_y, dispatch_src_x, this](vuk::CommandBuffer& command_buffer,
                                                                                 VUK_IA(vuk::eComputeSampled) _r_lock,
                                                                                 VUK_IA(vuk::eComputeSampled) _adjusted_color,
                                                                                 VUK_IA(vuk::eComputeRW) _rw_lock) {
    command_buffer.bind_compute_pipeline("lock_pass").bind_image(0, 0, _r_lock).bind_image(0, 1, _adjusted_color).bind_image(0, 2, _rw_lock);

    auto* constants = command_buffer.scratch_buffer<Fsr2Constants>(0, 3);
    *constants = fsr2_constants;

    command_buffer.dispatch(dispatch_src_x, dispatch_src_y, 1);

    return _rw_lock;
  });

  auto rw_lock_output = create_locks_pass(r_lock, adjusted_color_output, rw_lock);

  auto reproject_accumulate_pass = vuk::make_pass("reproject_accumulate",
                                                  [this, dispatch_dst_x, dispatch_dst_y](vuk::CommandBuffer& command_buffer,
                                                                                         VUK_IA(vuk::eComputeSampled) _exposure,
                                                                                         VUK_IA(vuk::eComputeSampled) _dilated_motion,
                                                                                         VUK_IA(vuk::eComputeSampled) _r_output,
                                                                                         VUK_IA(vuk::eComputeSampled) _r_lock,
                                                                                         VUK_IA(vuk::eComputeSampled) _disocclusion_mask,
                                                                                         VUK_IA(vuk::eComputeSampled) _adjusted_color,
                                                                                         VUK_IA(vuk::eComputeSampled) _luminance_history,
                                                                                         VUK_IA(vuk::eComputeSampled) _lanczos_lut,
                                                                                         VUK_IA(vuk::eComputeSampled) _maximum_bias_lut,
                                                                                         VUK_IA(vuk::eComputeSampled) _dilated_reactive,
                                                                                         VUK_IA(vuk::eComputeSampled) _luminance_current,
                                                                                         VUK_IA(vuk::eComputeRW) _rw_output,
                                                                                         VUK_IA(vuk::eComputeRW) _rw_lock) {
    command_buffer.bind_compute_pipeline("accumulate_pass")
      .bind_image(0, 0, _exposure)
      .bind_image(0, 1, _dilated_motion)
      .bind_image(0, 2, _r_output)
      .bind_image(0, 3, _r_lock)
      .bind_image(0, 4, _disocclusion_mask)
      .bind_image(0, 5, _adjusted_color)
      .bind_image(0, 6, _luminance_history)
      .bind_image(0, 7, _lanczos_lut)
      .bind_image(0, 8, _maximum_bias_lut)
      .bind_image(0, 9, _dilated_reactive)
      .bind_image(0, 10, _luminance_current)
      .bind_image(0, 11, _rw_output)
      .bind_image(0, 12, _rw_lock);

    auto* constants = command_buffer.scratch_buffer<Fsr2Constants>(0, 13);
    *constants = fsr2_constants;

    command_buffer.dispatch(dispatch_dst_x, dispatch_dst_y, 1);

    return std::make_tuple(_rw_output, _rw_lock);
  });

  auto lanczos_ia = vuk::acquire_ia("lanczos_lut", lanczos_lut.as_attachment(), vuk::eComputeSampled);
  auto maximum_bias_ia = vuk::acquire_ia("maximum_bias_lut", maximum_bias_lut.as_attachment(), vuk::eComputeSampled);

  auto [rw_output_output, rw_lock_output2] = reproject_accumulate_pass(exposure_output,
                                                                       dilated_motion_output,
                                                                       r_output,
                                                                       r_lock,
                                                                       disocclusion_mask_output,
                                                                       adjusted_color_output,
                                                                       luminance_history_output,
                                                                       lanczos_ia,
                                                                       maximum_bias_ia,
                                                                       dilated_reactive_output,
                                                                       luminance_current_output,
                                                                       rw_output,
                                                                       rw_lock_output);

  auto rcas_pass = vuk::make_pass("sharpen(RCAS)",
                                  [this, rcasConsts](vuk::CommandBuffer& command_buffer,
                                                     VUK_IA(vuk::eComputeSampled) _exposure,
                                                     VUK_IA(vuk::eComputeSampled) _rw_output,
                                                     VUK_IA(vuk::eComputeRW) _output) {
    command_buffer.bind_compute_pipeline("rcas_pass").bind_image(0, 0, _exposure).bind_image(0, 1, _rw_output).bind_image(0, 2, _output);

    auto* constants = command_buffer.scratch_buffer<Fsr2Constants>(0, 3);
    *constants = fsr2_constants;
    auto* rcas_constants = command_buffer.scratch_buffer<Fsr2RcasConstants>(0, 4);
    *rcas_constants = rcasConsts;

    const int32_t thread_group_work_region_dim_rcas = 16;
    const int32_t dispatch_x = (fsr2_constants.displaySize[0] + (thread_group_work_region_dim_rcas - 1)) / thread_group_work_region_dim_rcas;
    const int32_t dispatch_y = (fsr2_constants.displaySize[1] + (thread_group_work_region_dim_rcas - 1)) / thread_group_work_region_dim_rcas;

    command_buffer.dispatch(dispatch_x, dispatch_y, 1);

    return _output;
  });

  return rcas_pass(exposure_output, rw_output_output, output);
}
} // namespace ox
