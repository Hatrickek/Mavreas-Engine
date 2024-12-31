#pragma once

#include <glm/ext/quaternion_float.hpp>
#include <glm/fwd.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace ox {
using int8 = std::int8_t;
using int16 = std::int16_t;
using int32 = std::int32_t;
using int64 = std::int64_t;

using uint8 = std::uint8_t;
using uint16 = std::uint16_t;
using uint32 = std::uint32_t;
using uint64 = std::uint64_t;

using Vec2 = glm::vec2;
using float2 = glm::vec2;

using DVec2 = glm::dvec2;

using IVec2 = glm::ivec2;
using int2 = glm::ivec2;

using UVec2 = glm::uvec2;
using uint2 = glm::uvec2;

using Vec3 = glm::vec3;
using float3 = glm::vec3;

using DVec3 = glm::dvec3;

using IVec3 = glm::ivec3;
using int3 = glm::ivec3;

using UVec3 = glm::uvec3;
using uint3 = glm::uvec3;

using Vec4 = glm::vec4;
using float4 = glm::vec4;

using DVec4 = glm::dvec4;

using IVec4 = glm::ivec4;
using int4 = glm::ivec4;

using UVec4 = glm::uvec4;
using uint4 = glm::uvec4;

using Mat3 = glm::mat3;
using float3x3 = glm::mat3;

using IMat3 = glm::imat3x3;
using int3x3 = glm::imat3x3;

using UMat3 = glm::umat3x3;
using uint3x3 = glm::umat3x3;

using Mat4 = glm::mat4;
using float4x4 = glm::mat4;

using IMat4 = glm::imat4x4;
using int4x4 = glm::imat4x4;

using UMat4 = glm::umat4x4;
using uint4x4 = glm::umat4x4;

using Quat = glm::quat;
} // namespace ox
