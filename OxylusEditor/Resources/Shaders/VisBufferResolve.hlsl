#include "Globals.hlsli"
#include "VisBufferCommon.hlsli"

struct VOut {
  float4 position : SV_POSITION;
  float2 uv : UV;
  // uint32 material_id : MAT_ID;
};

VOut VSmain([[vk::builtin("BaseInstance")]] uint base_instance : DrawIndex, uint vertex_index : SV_VertexID) {
  VOut o;
  // o.material_id = base_instance;
  const float materialId = asfloat(0x3f7fffff - (uint(base_instance) & MESHLET_MATERIAL_ID_MASK));
  float2 pos = float2(vertex_index == 0, vertex_index == 2);
  o.uv = pos.xy * 2.0;
  o.position = float4(pos * 4.0 - 1.0, materialId, 1.0);
  return o;
}

struct POut {
  float4 albedo : SV_TARGET0;
  float4 normal : SV_TARGET1;
  float2 vertex_normal : SV_TARGET2;
  float3 metallic_roughness_ao : SV_TARGET3;
  float2 velocity : SV_TARGET4;
  float3 emission : SV_TARGET5;
};

struct PartialDerivatives {
  float3 lambda; // Barycentric coords
  float3 ddx;
  float3 ddy;
};

struct UvGradient {
  float2 uv;
  float2 ddx;
  float2 ddy;
};

PartialDerivatives compute_derivatives(const float4 clip[3], in float2 ndcUv, in float2 resolution) {
  PartialDerivatives result;
  const float3 invW = 1.0 / float3(clip[0].w, clip[1].w, clip[2].w);
  const float2 ndc0 = clip[0].xy * invW[0];
  const float2 ndc1 = clip[1].xy * invW[1];
  const float2 ndc2 = clip[2].xy * invW[2];

  const float invDet = 1.0 / determinant(float2x2(ndc2 - ndc1, ndc0 - ndc1));
  result.ddx = float3(ndc1.y - ndc2.y, ndc2.y - ndc0.y, ndc0.y - ndc1.y) * invDet * invW;
  result.ddy = float3(ndc2.x - ndc1.x, ndc0.x - ndc2.x, ndc1.x - ndc0.x) * invDet * invW;

  float ddxSum = dot(result.ddx, float3(1.0, 1.0, 1.0));
  float ddySum = dot(result.ddy, float3(1.0, 1.0, 1.0));

  const float2 deltaV = ndcUv - ndc0;
  const float interpInvW = invW.x + deltaV.x * ddxSum + deltaV.y * ddySum;
  const float interpW = 1.0 / interpInvW;

  result.lambda = float3(interpW * (deltaV.x * result.ddx.x + deltaV.y * result.ddy.x + invW.x),
                         interpW * (deltaV.x * result.ddx.y + deltaV.y * result.ddy.y),
                         interpW * (deltaV.x * result.ddx.z + deltaV.y * result.ddy.z));

  result.ddx *= 2.0f / resolution.x;
  result.ddy *= 2.0f / resolution.y;
  ddxSum *= 2.0f / resolution.x;
  ddySum *= 2.0f / resolution.y;

  const float interpDdxW = 1.0 / (interpInvW + ddxSum);
  const float interpDdyW = 1.0 / (interpInvW + ddySum);

  result.ddx = interpDdxW * (result.lambda * interpInvW + result.ddx) - result.lambda;
  result.ddy = interpDdyW * (result.lambda * interpInvW + result.ddy) - result.lambda;
  return result;
}

float3 interpolate(const PartialDerivatives derivatives, const float3 values) {
  const float3 v = float3(values[0], values[1], values[2]);
  return float3(dot(v, derivatives.lambda), dot(v, derivatives.ddx), dot(v, derivatives.ddy));
}

uint3 visbuffer_load_index_ids(const Meshlet meshlet, in uint primitiveId) {
  const uint indexOffset = meshlet.index_offset;
  const uint primitiveOffset = meshlet.primitive_offset;
  const uint primitiveIds[] = {uint(get_primitive(primitiveOffset + primitiveId * 3 + 0)),
                               uint(get_primitive(primitiveOffset + primitiveId * 3 + 1)),
                               uint(get_primitive(primitiveOffset + primitiveId * 3 + 2))};
  return uint3(get_index(indexOffset + primitiveIds[0]), get_index(indexOffset + primitiveIds[1]), get_index(indexOffset + primitiveIds[2]));
}

void visbuffer_load_position(uint3 indexIds, uint vertexOffset, out float3 positions[3]) {
  positions[0] = get_vertex(vertexOffset + indexIds.x).position.unpack();
  positions[1] = get_vertex(vertexOffset + indexIds.y).position.unpack();
  positions[2] = get_vertex(vertexOffset + indexIds.z).position.unpack();
}

void visbuffer_load_uv(uint3 indexIds, uint vertexOffset, out float2 uvs[3]) {
  uvs[0] = get_vertex(vertexOffset + indexIds.x).uv.unpack();
  uvs[1] = get_vertex(vertexOffset + indexIds.y).uv.unpack();
  uvs[2] = get_vertex(vertexOffset + indexIds.z).uv.unpack();
}

void visbuffer_load_normal(uint3 indexIds, uint vertexOffset, out float3 normals[3]) {
  normals[0] = oct_to_vec3(unpack_snorm2_x16(get_vertex(vertexOffset + indexIds.x).normal));
  normals[1] = oct_to_vec3(unpack_snorm2_x16(get_vertex(vertexOffset + indexIds.y).normal));
  normals[2] = oct_to_vec3(unpack_snorm2_x16(get_vertex(vertexOffset + indexIds.z).normal));
}

UvGradient make_uv_gradient(const PartialDerivatives derivatives, const float2 uvs[3]) {
  const float3 interpUvs[2] = {interpolate(derivatives, float3(uvs[0].x, uvs[1].x, uvs[2].x)),
                               interpolate(derivatives, float3(uvs[0].y, uvs[1].y, uvs[2].y))};

  UvGradient gradient;
  gradient.uv = float2(interpUvs[0].x, interpUvs[1].x);
  gradient.ddx = float2(interpUvs[0].y, interpUvs[1].y);
  gradient.ddy = float2(interpUvs[0].z, interpUvs[1].z);
  return gradient;
}

float3 interpolate_vec3(const PartialDerivatives derivatives, const float3 float3Data[3]) {
  return float3(interpolate(derivatives, float3(float3Data[0].x, float3Data[1].x, float3Data[2].x)).x,
                interpolate(derivatives, float3(float3Data[0].y, float3Data[1].y, float3Data[2].y)).x,
                interpolate(derivatives, float3(float3Data[0].z, float3Data[1].z, float3Data[2].z)).x);
}

float4 interpolate_vec4(const PartialDerivatives derivatives, const float4 float4Data[3]) {
  return float4(interpolate(derivatives, float3(float4Data[0].x, float4Data[1].x, float4Data[2].x)).x,
                interpolate(derivatives, float3(float4Data[0].y, float4Data[1].y, float4Data[2].y)).x,
                interpolate(derivatives, float3(float4Data[0].z, float4Data[1].z, float4Data[2].z)).x,
                interpolate(derivatives, float3(float4Data[0].w, float4Data[1].w, float4Data[2].w)).x);
}

float2 make_smooth_motion(const PartialDerivatives derivatives, float4 worldPosition[3], float4 worldPositionOld[3]) {
  // Probably not the most efficient way to do this, but this is a port of a shader that is known to work
  float4 v_curPos[3] = {mul(get_camera(0).projection_view, worldPosition[0]),
                        mul(get_camera(0).projection_view, worldPosition[1]),
                        mul(get_camera(0).projection_view, worldPosition[2])};

  float4 v_oldPos[3] = {mul(get_camera(0).projection_view, worldPositionOld[0]),
                        mul(get_camera(0).projection_view, worldPositionOld[1]),
                        mul(get_camera(0).projection_view, worldPositionOld[2])};

  float4 smoothCurPos = interpolate_vec4(derivatives, v_curPos);
  float4 smoothOldPos = interpolate_vec4(derivatives, v_oldPos);
  return ((smoothOldPos.xy / smoothOldPos.w) - (smoothCurPos.xy / smoothCurPos.w)) * 0.5;
}

float4 sample_base_color(Material material, SamplerState material_sampler, UvGradient uvGrad) {
  if (material.albedo_map_id == INVALID_ID) {
    return material.color;
  }

  float3 color = get_material_albedo_texture(material).SampleGrad(material_sampler, uvGrad.uv, uvGrad.ddx, uvGrad.ddy).rgb;
  return float4(color, 1.0f);
}

float2 sample_metallic_roughness(Material material, SamplerState material_sampler, UvGradient uvGrad) {
  float2 surface_map = 1;
  if (material.physical_map_id != INVALID_ID) {
    surface_map = get_material_physical_texture(material).SampleGrad(material_sampler, uvGrad.uv, uvGrad.ddx, uvGrad.ddy).bg;
  }

  surface_map.r *= material.metallic;
  surface_map.g *= material.roughness;

  return surface_map;
}

float sample_occlusion(Material material, SamplerState material_sampler, UvGradient uvGrad) {
  float occlusion = 1;
  if (material.ao_map_id != INVALID_ID) {
    occlusion = get_material_ao_texture(material).SampleGrad(material_sampler, uvGrad.uv, uvGrad.ddx, uvGrad.ddy).r;
  }
  return occlusion;
}

POut PSmain(VOut input, float4 pixelPosition : SV_Position) {
  POut o;

  Texture2D<uint> vis_texture = get_visibility_texture();
  CameraData cam = get_camera(0);

  const int2 position = int2(pixelPosition.xy);
  const uint payload = vis_texture.Load(int3(position, 0)).x;
  const uint meshlet_id = (payload >> MESHLET_PRIMITIVE_BITS) & MESHLET_ID_MASK;
  const uint primitiveId = payload & MESHLET_PRIMITIVE_MASK;
  const MeshletInstance meshlet_instance = get_meshlet_instance(meshlet_id);
  const Meshlet meshlet = get_meshlet(meshlet_instance.meshlet_id);
  const Material material = get_material(meshlet_instance.material_id);
  const float4x4 transform = get_transform(meshlet_instance.instance_id);
  // const float4x4 transformPrevious = get_instance(meshlet.instanceId).modelPrevious;

  uint3 index_ids = visbuffer_load_index_ids(meshlet, primitiveId);
  float3 raw_position[3];
  visbuffer_load_position(index_ids, meshlet.vertex_offset, raw_position);
  float2 raw_uv[3];
  visbuffer_load_uv(index_ids, meshlet.vertex_offset, raw_uv);
  float3 raw_normal[3];
  visbuffer_load_normal(index_ids, meshlet.vertex_offset, raw_normal);
  const float4 world_position[3] = {mul(transform, float4(raw_position[0], 1.0)),
                                    mul(transform, float4(raw_position[1], 1.0)),
                                    mul(transform, float4(raw_position[2], 1.0))};
  const float4 clip_position[3] = {mul(cam.projection_view, world_position[0]),
                                   mul(cam.projection_view, world_position[1]),
                                   mul(cam.projection_view, world_position[2])};

  // const float4 worldPositionPrevious[3] = {transformPrevious * float4(rawPosition[0], 1.0),
  //                                          transformPrevious * float4(rawPosition[1], 1.0),
  //                                          transformPrevious * float4(rawPosition[2], 1.0)};
  uint width, height, levels;
  vis_texture.GetDimensions(0, width, height, levels);
  const float2 resolution = float2(width, height);
  const PartialDerivatives partialDerivatives = compute_derivatives(clip_position, input.uv * 2.0 - 1.0, resolution);
  const UvGradient uv_grad = make_uv_gradient(partialDerivatives, raw_uv);
  const float3 flatNormal = normalize(cross(raw_position[1] - raw_position[0], raw_position[2] - raw_position[0]));

  const float3 smoothObjectNormal = normalize(interpolate_vec3(partialDerivatives, raw_normal));
  const float3x3 normalMatrix = transpose((float3x3)transform);
  const float3 smoothWorldNormal = normalize(mul(normalMatrix, smoothObjectNormal));
  float3 normal = smoothWorldNormal;

  // TODO: use view-space positions to maintain precision
  float3 iwp[] = {interpolate(partialDerivatives, float3(world_position[0].x, world_position[1].x, world_position[2].x)),
                  interpolate(partialDerivatives, float3(world_position[0].y, world_position[1].y, world_position[2].y)),
                  interpolate(partialDerivatives, float3(world_position[0].z, world_position[1].z, world_position[2].z))};

  const SamplerState material_sampler = Samplers[material.sampler];

  if (material.normal_map_id != INVALID_ID) {
    const float3 ddx_position = float3(iwp[0].y, iwp[1].y, iwp[2].y);
    const float3 ddy_position = float3(iwp[0].z, iwp[1].z, iwp[2].z);
    const float2 ddx_uv = uv_grad.ddx;
    const float2 ddy_uv = uv_grad.ddy;

    const float3 N = normal;
    const float3 T = normalize(ddx_position * ddy_uv.y - ddy_position * ddx_uv.y);
    const float3 B = -normalize(cross(N, T));

    float3x3 TBN = float3x3(T, B, N);

    float3 sampledNormal = float3(get_material_normal_texture(material).SampleGrad(material_sampler, uv_grad.uv, uv_grad.ddx, uv_grad.ddy).rg, 1.0f);
    sampledNormal = sampledNormal * 2.f - 1.f;
    normal = normalize(mul(sampledNormal, TBN));
  }

  o.albedo = sample_base_color(material, material_sampler, uv_grad);
  o.metallic_roughness_ao = float3(sample_metallic_roughness(material, material_sampler, uv_grad),
                                   sample_occlusion(material, material_sampler, uv_grad));
  o.normal.xy = vec3_to_oct(normal);
  o.normal.zw = vec3_to_oct(flatNormal);
  o.vertex_normal = vec3_to_oct(smoothWorldNormal);
  o.emission = 0; // o_emission = sample_emission(material, uvGrad);
  o.velocity = 0; // o_motion = make_smooth_motion(partialDerivatives, worldPosition, worldPositionPrevious);

  return o;
}
