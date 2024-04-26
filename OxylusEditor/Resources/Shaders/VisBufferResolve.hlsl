#include "Globals.hlsli"
#include "VisBufferCommon.hlsli"

struct VOut {
  float4 position : SV_POSITION;
  float2 uv : UV;
  uint32 material_id : MAT_ID;
};

VOut VSmain([[vk::builtin("BaseInstance")]] uint base_instance : DrawIndex, uint vertex_index : SV_VertexID) {
  VOut o;
  o.material_id = base_instance;
  const float materialId = asfloat(0x3f7fffff - (uint(base_instance) & MESHLET_MATERIAL_ID_MASK));
  float2 pos = float2(vertex_index == 0, vertex_index == 2);
  o.uv = pos.xy * 2.0;
  o.position = float4(pos * 4.0 - 1.0, materialId, 1.0);
  return o;
}

struct POut {
  float4 albedo : SV_TARGET0;
  float4 normal : SV_TARGET1;
  float3 metallic_roughness_ao : SV_TARGET2;
  float2 velocity : SV_TARGET3;
  float3 emission : SV_TARGET4;
};

struct PartialDerivatives {
  float3 lambda; // Barycentric coord for those whomst be wonderin'
  float3 ddx;
  float3 ddy;
};

struct UvGradient {
  float2 uv;
  float2 ddx;
  float2 ddy;
};

PartialDerivatives ComputeDerivatives(const float4 clip[3], in float2 ndcUv, in float2 resolution) {
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

  result.ddx *= 2.0 / resolution.x;
  result.ddy *= 2.0 / resolution.y;
  ddxSum *= 2.0 / resolution.x;
  ddySum *= 2.0 / resolution.y;

  const float interpDdxW = 1.0 / (interpInvW + ddxSum);
  const float interpDdyW = 1.0 / (interpInvW + ddySum);

  result.ddx = interpDdxW * (result.lambda * interpInvW + result.ddx) - result.lambda;
  result.ddy = interpDdyW * (result.lambda * interpInvW + result.ddy) - result.lambda;
  return result;
}

float3 Interpolate(const PartialDerivatives derivatives, const float3 values) {
  const float3 v = float3(values[0], values[1], values[2]);
  return float3(dot(v, derivatives.lambda), dot(v, derivatives.ddx), dot(v, derivatives.ddy));
}

uint3 VisbufferLoadIndexIds(const Meshlet meshlet, in uint primitiveId) {
  const uint indexOffset = meshlet.indexOffset;
  const uint primitiveOffset = meshlet.primitiveOffset;
  const uint primitiveIds[] = {uint(get_primitive(primitiveOffset + primitiveId * 3 + 0)),
                               uint(get_primitive(primitiveOffset + primitiveId * 3 + 1)),
                               uint(get_primitive(primitiveOffset + primitiveId * 3 + 2))};
  return uint3(get_index(indexOffset + primitiveIds[0]), get_index(indexOffset + primitiveIds[1]), get_index(indexOffset + primitiveIds[2]));
}

void VisbufferLoadPosition(uint3 indexIds, uint vertexOffset, out float3 positions[3]) {
  positions[0] = get_vertex(vertexOffset + indexIds.x).position;
  positions[1] = get_vertex(vertexOffset + indexIds.y).position;
  positions[2] = get_vertex(vertexOffset + indexIds.z).position;
}

void VisbufferLoadUv(uint3 indexIds, uint vertexOffset, out float2 uvs[3]) {
  uvs[0] = get_vertex(vertexOffset + indexIds.x).uv;
  uvs[1] = get_vertex(vertexOffset + indexIds.y).uv;
  uvs[2] = get_vertex(vertexOffset + indexIds.z).uv;
}

void VisbufferLoadNormal(uint3 indexIds, uint vertexOffset, out float3 normals[3]) {
  normals[0] = get_vertex(vertexOffset + indexIds[0]).normal;
  normals[1] = get_vertex(vertexOffset + indexIds[1]).normal;
  normals[2] = get_vertex(vertexOffset + indexIds[2]).normal;
}

UvGradient MakeUvGradient(const PartialDerivatives derivatives, const float2 uvs[3]) {
  const float3 interpUvs[2] = {Interpolate(derivatives, float3(uvs[0].x, uvs[1].x, uvs[2].x)),
                               Interpolate(derivatives, float3(uvs[0].y, uvs[1].y, uvs[2].y))};

  UvGradient gradient;
  gradient.ddx = float2(interpUvs[0].x, interpUvs[1].x);
  gradient.ddy = float2(interpUvs[0].y, interpUvs[1].y);
  gradient.uv = float2(interpUvs[0].z, interpUvs[1].z);
  return gradient;
}

float3 InterpolateVec3(const PartialDerivatives derivatives, const float3 float3Data[3]) {
  return float3(Interpolate(derivatives, float3(float3Data[0].x, float3Data[1].x, float3Data[2].x)).x,
                Interpolate(derivatives, float3(float3Data[0].y, float3Data[1].y, float3Data[2].y)).x,
                Interpolate(derivatives, float3(float3Data[0].z, float3Data[1].z, float3Data[2].z)).x);
}

float4 InterpolateVec4(const PartialDerivatives derivatives, const float4 float4Data[3]) {
  return float4(Interpolate(derivatives, float3(float4Data[0].x, float4Data[1].x, float4Data[2].x)).x,
                Interpolate(derivatives, float3(float4Data[0].y, float4Data[1].y, float4Data[2].y)).x,
                Interpolate(derivatives, float3(float4Data[0].z, float4Data[1].z, float4Data[2].z)).x,
                Interpolate(derivatives, float3(float4Data[0].w, float4Data[1].w, float4Data[2].w)).x);
}

float2 MakeSmoothMotion(const PartialDerivatives derivatives, float4 worldPosition[3], float4 worldPositionOld[3]) {
  // Probably not the most efficient way to do this, but this is a port of a shader that is known to work
  float4 v_curPos[3] = {mul(get_camera(0).projection_view, worldPosition[0]),
                        mul(get_camera(0).projection_view, worldPosition[1]),
                        mul(get_camera(0).projection_view, worldPosition[2])};

  float4 v_oldPos[3] = {mul(get_camera(0).projection_view, worldPositionOld[0]),
                        mul(get_camera(0).projection_view, worldPositionOld[1]),
                        mul(get_camera(0).projection_view, worldPositionOld[2])};

  float4 smoothCurPos = InterpolateVec4(derivatives, v_curPos);
  float4 smoothOldPos = InterpolateVec4(derivatives, v_oldPos);
  return ((smoothOldPos.xy / smoothOldPos.w) - (smoothCurPos.xy / smoothCurPos.w)) * 0.5;
}

float4 sample_base_color(Material material, SamplerState material_sampler, UvGradient uvGrad) {
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

  const int2 position = int2(pixelPosition.xy);
  const uint payload = vis_texture.Load(int3(position, 0)).x;
  const uint meshletId = (payload >> MESHLET_PRIMITIVE_BITS) & MESHLET_ID_MASK;
  const uint primitiveId = payload & MESHLET_PRIMITIVE_MASK;
  const Meshlet meshlet = get_meshlet(meshletId);
  const Material material = get_material(meshlet.materialId);
  const float4x4 transform = get_instance(meshlet.instanceId).transform;
  // const float4x4 transformPrevious = get_instance(meshlet.instanceId).modelPrevious;

  uint3 indexIDs = VisbufferLoadIndexIds(meshlet, primitiveId);
  float3 rawPosition[3];
  VisbufferLoadPosition(indexIDs, meshlet.vertexOffset, rawPosition);
  float2 rawUv[3];
  VisbufferLoadUv(indexIDs, meshlet.vertexOffset, rawUv);
  float3 rawNormal[3];
  VisbufferLoadNormal(indexIDs, meshlet.vertexOffset, rawNormal);
  const float4 worldPosition[3] = {mul(transform, float4(rawPosition[0], 1.0)),
                                   mul(transform, float4(rawPosition[1], 1.0)),
                                   mul(transform, float4(rawPosition[2], 1.0))};
  const float4 clipPosition[3] = {mul(get_camera(0).projection_view, worldPosition[0]),
                                  mul(get_camera(0).projection_view, worldPosition[1]),
                                  mul(get_camera(0).projection_view, worldPosition[2])};

  // const float4 worldPositionPrevious[3] = {transformPrevious * float4(rawPosition[0], 1.0),
  //                                          transformPrevious * float4(rawPosition[1], 1.0),
  //                                          transformPrevious * float4(rawPosition[2], 1.0)};
  uint width, height, levels;
  vis_texture.GetDimensions(0, width, height, levels);
  const float2 resolution = float2(width, height);
  const PartialDerivatives partialDerivatives = ComputeDerivatives(clipPosition, input.uv * 2.0 - 1.0, resolution);
  const UvGradient uvGrad = MakeUvGradient(partialDerivatives, rawUv);
  const float3 flatNormal = normalize(cross(rawPosition[1] - rawPosition[0], rawPosition[2] - rawPosition[0]));

  const float3 smoothObjectNormal = normalize(InterpolateVec3(partialDerivatives, rawNormal));
  const float3x3 normalMatrix = transpose((float3x3)transform);
  const float3 smoothWorldNormal = normalize(mul(normalMatrix, smoothObjectNormal));
  float3 normal = smoothWorldNormal;

  // TODO: use view-space positions to maintain precision
  float3 iwp[] = {Interpolate(partialDerivatives, float3(worldPosition[0].x, worldPosition[1].x, worldPosition[2].x)),
                  Interpolate(partialDerivatives, float3(worldPosition[0].y, worldPosition[1].y, worldPosition[2].y)),
                  Interpolate(partialDerivatives, float3(worldPosition[0].z, worldPosition[1].z, worldPosition[2].z))};

  SamplerState material_sampler = Samplers[material.sampler];

  if (material.normal_map_id != INVALID_ID) {
    float3x3 TBN = 0;
    const float3 ddx_position = float3(iwp[0].y, iwp[1].y, iwp[2].y);
    const float3 ddy_position = float3(iwp[0].z, iwp[1].z, iwp[2].z);
    const float2 ddx_uv = uvGrad.ddx;
    const float2 ddy_uv = uvGrad.ddy;

    const float3 N = normal;
    const float3 T = normalize(ddx_position * ddy_uv.y - ddy_position * ddx_uv.y);
    const float3 B = -normalize(cross(N, T));

    TBN = float3x3(T, B, N);

    float3 sampledNormal = normalize(get_material_normal_texture(material).SampleGrad(material_sampler, uvGrad.uv, uvGrad.ddx, uvGrad.ddy));
    normal = normalize(mul(TBN, sampledNormal));
  }

  o.albedo = sample_base_color(material, material_sampler, uvGrad);
  o.metallic_roughness_ao = float3(sample_metallic_roughness(material, material_sampler, uvGrad), sample_occlusion(material, material_sampler, uvGrad));
  o.normal = float4(normal, 1.0);
  // o_normalAndFaceNormal.zw = Vec3ToOct(flatNormal);
  // o_smoothVertexNormal = Vec3ToOct(smoothWorldNormal);
  o.emission = 0; // o_emission = SampleEmission(material, uvGrad);
  o.velocity = 0; // o_motion = MakeSmoothMotion(partialDerivatives, worldPosition, worldPositionPrevious);

  return o;
}
