#include "GTAO.hpp"

#include <glm/gtc/type_ptr.hpp>
#include "Render/Camera.hpp"

namespace ox {
void gtao_update_constants(GTAOConstants& consts,
                           const int viewport_width,
                           const int viewport_height,
                           const GTAOSettings& settings,
                           const Camera* camera,
                           const unsigned frameCounter) {
  consts.viewport_size = {viewport_width, viewport_height};
  consts.viewport_pixel_size = {1.0f / (float)viewport_width, 1.0f / (float)viewport_height};

  auto p = camera->get_projection_matrix();
  const auto proj_matrix = glm::value_ptr(p);

  constexpr auto row_major = true;

  // float depthLinearizeMul = ( clipFar * clipNear ) / ( clipFar - clipNear );
  float depth_linearize_mul = (row_major) ? (-proj_matrix[3 * 4 + 2]) : (-proj_matrix[3 + 2 * 4]);

  // float depthLinearizeAdd = clipFar / ( clipFar - clipNear );
  float depth_linearize_add = (row_major) ? (proj_matrix[2 * 4 + 2]) : (proj_matrix[2 + 2 * 4]);

  // correct the handedness issue. need to make sure this below is correct, but I think it is.
  if (depth_linearize_mul * depth_linearize_add < 0)
    depth_linearize_add = -depth_linearize_add;
  consts.depth_unpack_consts = {depth_linearize_mul, depth_linearize_add};

  float tan_half_fovy = 1.0f / ((row_major) ? (proj_matrix[1 * 4 + 1]) : (proj_matrix[1 + 1 * 4])); // = tanf( drawContext.Camera.GetYFOV( ) * 0.5f );
  float tanHalfFOVX = 1.0F / ((row_major) ? (proj_matrix[0 * 4 + 0]) : (proj_matrix[0 + 0 * 4])); // = tanHalfFOVY * drawContext.Camera.GetAspect( );
  consts.camera_tan_half_fov = {tanHalfFOVX, tan_half_fovy};

  consts.ndc_to_view_mul = {consts.camera_tan_half_fov.x * 2.0f, consts.camera_tan_half_fov.y * -2.0f};
  consts.ndc_to_view_add = {consts.camera_tan_half_fov.x * -1.0f, consts.camera_tan_half_fov.y * 1.0f};

  consts.ndc_to_view_mul_x_pixel_size = {consts.ndc_to_view_mul.x * consts.viewport_pixel_size.x,
                                         consts.ndc_to_view_mul.y * consts.viewport_pixel_size.y};

  consts.effect_radius = settings.radius;

  consts.effect_falloff_range = settings.falloff_range;

  // high value disables denoise - more elegant & correct way would be do set all edges to 0
  consts.denoise_blur_beta = settings.denoise_passes == 0 ? 1e4f : 1.2f;

  consts.radius_multiplier = settings.radius_multiplier;
  consts.sample_distribution_power = settings.sample_distribution_power;
  consts.thin_occluder_compensation = settings.thin_occluder_compensation;
  consts.final_value_power = settings.final_value_power;
  consts.depth_mip_sampling_offset = settings.depth_mip_sampling_offset;
  consts.noise_index = settings.denoise_passes > 0 ? frameCounter % 64u : 0u;
  consts.padding0 = 0;
}
} // namespace ox
