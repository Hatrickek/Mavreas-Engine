#pragma once
#include "Core/Types.hpp"

namespace ox {
class Camera;

// Global consts that need to be visible from both shader and cpp side
#define XE_GTAO_DEPTH_MIP_LEVELS 5 // this one is hard-coded to 5 for now
#define XE_GTAO_NUMTHREADS_X 8     // these can be changed
#define XE_GTAO_NUMTHREADS_Y 8     // these can be changed

struct GTAOConstants {
  IVec2 viewport_size;
  Vec2 viewport_pixel_size; // .zw == 1.0 / ViewportSize.xy

  Vec2 depth_unpack_consts;
  Vec2 camera_tan_half_fov;

  Vec2 ndc_to_view_mul;
  Vec2 ndc_to_view_add;

  Vec2 ndc_to_view_mul_x_pixel_size;
  float effect_radius; // world (viewspace) maximum size of the shadow
  float effect_falloff_range;

  float radius_multiplier;
  float padding0;
  float final_value_power;
  float denoise_blur_beta;

  float sample_distribution_power;
  float thin_occluder_compensation;
  float depth_mip_sampling_offset;
  int noise_index; // frameIndex % 64 if using TAA or 0 otherwise
};

// some constants reduce performance if provided as dynamic values; if these constants are not required to be dynamic and they match default values,
// set XE_GTAO_USE_DEFAULT_CONSTANTS and the code will compile into a more efficient shader
#define XE_GTAO_DEFAULT_RADIUS_MULTIPLIER \
  (1.0f) // allows us to use different value as compared to ground truth radius to counter inherent screen space biases
#define XE_GTAO_DEFAULT_FALLOFF_RANGE (0.615f)            // distant samples contribute less
#define XE_GTAO_DEFAULT_SAMPLE_DISTRIBUTION_POWER (2.0f)  // small crevices more important than big surfaces
#define XE_GTAO_DEFAULT_THIN_OCCLUDER_COMPENSATION (0.0f) // the new 'thickness heuristic' approach
#define XE_GTAO_DEFAULT_FINAL_VALUE_POWER \
  (1.5f)  // modifies the final ambient occlusion value using power function - this allows some of the above heuristics to do different things
#define XE_GTAO_DEFAULT_DEPTH_MIP_SAMPLING_OFFSET \
  (3.30f) // main trade-off between performance (memory bandwidth) and quality (temporal stability is the first affected, thin objects next)

#define XE_GTAO_OCCLUSION_TERM_SCALE \
  (1.5f)  // for packing in UNORM (because raw, pre-denoised occlusion term can overshoot 1 but will later average out to 1)

// From https://www.shadertoy.com/view/3tB3z3 - except we're using R2 here
#define XE_HILBERT_LEVEL 6U
#define XE_HILBERT_WIDTH ((1U << XE_HILBERT_LEVEL))
#define XE_HILBERT_AREA (XE_HILBERT_WIDTH * XE_HILBERT_WIDTH)

uint32_t hilbert_index(uint32_t posX, uint32_t posY);

struct GTAOSettings {
  int quality_level = 2;  // 0: low; 1: medium; 2: high; 3: ultra
  int denoise_passes = 1; // 0: disabled; 1: sharp; 2: medium; 3: soft
  float radius = 0.5f;    // [0.0,  ~ ]   World (view) space size of the occlusion sphere.

  // auto-tune-d settings
  float radius_multiplier = XE_GTAO_DEFAULT_RADIUS_MULTIPLIER;
  float falloff_range = XE_GTAO_DEFAULT_FALLOFF_RANGE;
  float sample_distribution_power = XE_GTAO_DEFAULT_SAMPLE_DISTRIBUTION_POWER;
  float thin_occluder_compensation = XE_GTAO_DEFAULT_THIN_OCCLUDER_COMPENSATION;
  float final_value_power = XE_GTAO_DEFAULT_FINAL_VALUE_POWER;
  float depth_mip_sampling_offset = XE_GTAO_DEFAULT_DEPTH_MIP_SAMPLING_OFFSET;
};

// If using TAA then set noiseIndex to frameIndex % 64 - otherwise use 0
// glm is column major by default
void gtao_update_constants(GTAOConstants& consts,
                           int viewport_width,
                           int viewport_height,
                           const GTAOSettings& settings,
                           const ox::Camera* camera,
                           unsigned int frameCounter);
} // namespace ox
