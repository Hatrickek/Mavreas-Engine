#include "LuaMathBindings.hpp"

#include <sol/overload.hpp>
#include <sol/state.hpp>
#include <sol/types.hpp>

#include "LuaHelpers.hpp"

#include "Core/Types.hpp"

#include "Render/BoundingVolume.hpp"

#include "Utils/Profiler.hpp"

namespace ox::LuaBindings {
#define SET_MATH_FUNCTIONS(var, type, number)                                                                    \
  (var).set_function(sol::meta_function::division, [](const type& a, const type& b) { return a / b; });          \
  (var).set_function(sol::meta_function::equal_to, [](const type& a, const type& b) { return a == b; });         \
  (var).set_function(sol::meta_function::unary_minus, [](const type& v) -> type { return -v; });                 \
  (var).set_function(sol::meta_function::multiplication, sol::overload([](const type& a, const type& b) {        \
    return a * b;                                                                                                \
  }, [](const type& a, const number b) { return a * b; }, [](const number a, const type& b) { return a * b; })); \
  (var).set_function(sol::meta_function::subtraction, sol::overload([](const type& a, const type& b) {           \
    return a - b;                                                                                                \
  }, [](const type& a, const number b) { return a - b; }, [](const number a, const type& b) { return a - b; })); \
  (var).set_function(sol::meta_function::addition, sol::overload([](const type& a, const type& b) {              \
    return a + b;                                                                                                \
  }, [](const type& a, const number b) { return a + b; }, [](const number a, const type& b) { return a + b; }));

void bind_math(const Shared<sol::state>& state) {
  OX_SCOPED_ZONE;
  auto vec2 = state->new_usertype<Vec2>("Vec2", sol::constructors<Vec2(float, float), Vec2(float)>());
  SET_TYPE_FIELD(vec2, Vec2, x);
  SET_TYPE_FIELD(vec2, Vec2, y);
  SET_MATH_FUNCTIONS(vec2, Vec2, float)

  auto uvec2 = state->new_usertype<UVec2>("UVec2", sol::constructors<UVec2(uint32_t, uint32_t), UVec2(uint32_t)>());
  SET_TYPE_FIELD(uvec2, UVec2, x);
  SET_TYPE_FIELD(uvec2, UVec2, y);
  SET_MATH_FUNCTIONS(uvec2, UVec2, uint32_t)

  auto ivec2 = state->new_usertype<IVec2>("IVec2", sol::constructors<IVec2(int, int), IVec2(int)>());
  SET_TYPE_FIELD(ivec2, IVec2, x);
  SET_TYPE_FIELD(ivec2, IVec2, y);
  SET_MATH_FUNCTIONS(ivec2, IVec2, int)

  auto vec3 = state->new_usertype<Vec3>("Vec3", sol::constructors<sol::types<>, sol::types<float, float, float>, Vec3(float)>());
  SET_TYPE_FIELD(vec3, Vec3, x);
  SET_TYPE_FIELD(vec3, Vec3, y);
  SET_TYPE_FIELD(vec3, Vec3, z);
  SET_MATH_FUNCTIONS(vec3, Vec3, float)

  auto ivec3 = state->new_usertype<IVec3>("IVec3", sol::constructors<sol::types<>, sol::types<int, int, int>, IVec3(int)>());
  SET_TYPE_FIELD(ivec3, IVec3, x);
  SET_TYPE_FIELD(ivec3, IVec3, y);
  SET_TYPE_FIELD(ivec3, IVec3, z);
  SET_MATH_FUNCTIONS(ivec3, IVec3, int)

  auto uvec3 = state->new_usertype<UVec3>("UVec3", sol::constructors<sol::types<>, sol::types<uint32_t, uint32_t, uint32_t>, UVec3(uint32_t)>());
  SET_TYPE_FIELD(uvec3, UVec3, x);
  SET_TYPE_FIELD(uvec3, UVec3, y);
  SET_TYPE_FIELD(uvec3, UVec3, z);
  SET_MATH_FUNCTIONS(uvec3, UVec3, uint32_t)

  auto vec4 = state->new_usertype<Vec4>("Vec4", sol::constructors<sol::types<>, sol::types<float, float, float, float>, Vec4(float)>());
  SET_TYPE_FIELD(vec4, Vec4, x);
  SET_TYPE_FIELD(vec4, Vec4, y);
  SET_TYPE_FIELD(vec4, Vec4, z);
  SET_TYPE_FIELD(vec4, Vec4, w);
  SET_MATH_FUNCTIONS(vec4, Vec4, float)

  auto ivec4 = state->new_usertype<IVec4>("IVec4", sol::constructors<sol::types<>, sol::types<int, int, int, int>, IVec4(int)>());
  SET_TYPE_FIELD(ivec4, IVec4, x);
  SET_TYPE_FIELD(ivec4, IVec4, y);
  SET_TYPE_FIELD(ivec4, IVec4, z);
  SET_TYPE_FIELD(ivec4, IVec4, w);
  SET_MATH_FUNCTIONS(ivec4, IVec4, int)

  auto uvec4 = state->new_usertype<UVec4>("UVec4",
                                          sol::constructors<sol::types<>, sol::types<uint32_t, uint32_t, uint32_t, uint32_t>, UVec4(uint32_t)>());
  SET_TYPE_FIELD(uvec4, UVec4, x);
  SET_TYPE_FIELD(uvec4, UVec4, y);
  SET_TYPE_FIELD(uvec4, UVec4, z);
  SET_TYPE_FIELD(uvec4, UVec4, w);
  SET_MATH_FUNCTIONS(uvec4, UVec4, uint32_t)

  state->new_usertype<Mat3>("Mat3",
                            sol::constructors<Mat3(float, float, float, float, float, float, float, float, float), Mat3(float), Mat3()>(),
                            sol::meta_function::multiplication,
                            [](const Mat3& a, const Mat3& b) { return a * b; });

  state->new_usertype<Mat4>("Mat4", sol::constructors<Mat4(float), Mat4()>(), sol::meta_function::multiplication, [](const Mat4& a, const Mat4& b) {
    return a * b;
  }, sol::meta_function::addition, [](const Mat4& a, const Mat4& b) {
    return a + b;
  }, sol::meta_function::subtraction, [](const Mat4& a, const Mat4& b) { return a - b; });

  state->new_enum<Intersection>("Intersection", {{"Outside", Outside}, {"Intersects", Intersects}, {"Inside", Inside}});

  auto aabb = state->new_usertype<AABB>("AABB", sol::constructors<AABB(), AABB(AABB), AABB(Vec3, Vec3)>());
  SET_TYPE_FIELD(aabb, AABB, min);
  SET_TYPE_FIELD(aabb, AABB, max);
  SET_TYPE_FUNCTION(aabb, AABB, get_center);
  SET_TYPE_FUNCTION(aabb, AABB, get_extents);
  SET_TYPE_FUNCTION(aabb, AABB, get_size);
  SET_TYPE_FUNCTION(aabb, AABB, translate);
  SET_TYPE_FUNCTION(aabb, AABB, scale);
  SET_TYPE_FUNCTION(aabb, AABB, transform);
  SET_TYPE_FUNCTION(aabb, AABB, get_transformed);
  SET_TYPE_FUNCTION(aabb, AABB, merge);
  // TODO: Bind frustum and Plane
  // SET_TYPE_FUNCTION(aabb, AABB, is_on_frustum);
  // SET_TYPE_FUNCTION(aabb, AABB, is_on_or_forward_plane);
  aabb.set_function("intersects", sol::overload([](const AABB& self, const Vec3& point) {
    return self.intersects(point);
  }, [](const AABB& self, const AABB& box) { return self.intersects(box); }));

  // --- glm functions ---
  auto glm_table = state->create_table("glm");
  glm_table.set_function("translate", [](const Mat4& mat, const Vec3& vec) { return glm::translate(mat, vec); });

  glm_table.set_function("floor", sol::overload([](const float v) { return glm::floor(v); }, [](const Vec2& vec) {
    return glm::floor(vec);
  }, [](const Vec3& vec) { return glm::floor(vec); }, [](const Vec4& vec) { return glm::floor(vec); }));
  glm_table.set_function("ceil", sol::overload([](const float v) { return glm::ceil(v); }, [](const Vec2& vec) {
    return glm::ceil(vec);
  }, [](const Vec3& vec) { return glm::ceil(vec); }, [](const Vec4& vec) { return glm::ceil(vec); }));
  glm_table.set_function("round", sol::overload([](const float v) { return glm::round(v); }, [](const Vec2& vec) {
    return glm::round(vec);
  }, [](const Vec3& vec) { return glm::round(vec); }, [](const Vec4& vec) { return glm::round(vec); }));
  glm_table.set_function("length", sol::overload([](const Vec2& vec) { return glm::length(vec); }, [](const Vec3& vec) {
    return glm::length(vec);
  }, [](const Vec4& vec) { return glm::length(vec); }));
  glm_table.set_function("normalize", sol::overload([](const Vec2& vec) { return glm::normalize(vec); }, [](const Vec3& vec) {
    return glm::normalize(vec);
  }, [](const Vec4& vec) { return glm::normalize(vec); }));
  glm_table.set_function("distance", [](const Vec3& a, const Vec3& b) { return glm::distance(a, b); });
}
} // namespace ox::LuaBindings
