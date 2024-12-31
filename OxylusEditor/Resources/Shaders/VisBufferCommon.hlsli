#include "Common.hlsli"

#define MAX_INDICES 64
#define MAX_PRIMITIVES 64
#define MESHLET_ID_BITS 24u
#define MESHLET_MATERIAL_ID_BITS 23u
#define MESHLET_PRIMITIVE_BITS 8u
#define MESHLET_ID_MASK ((1u << MESHLET_ID_BITS) - 1u)
#define MESHLET_MATERIAL_ID_MASK ((1u << MESHLET_MATERIAL_ID_BITS) - 1u)
#define MESHLET_PRIMITIVE_MASK ((1u << MESHLET_PRIMITIVE_BITS) - 1u)

struct DispatchParams {
  uint32 groupCountX;
  uint32 groupCountY;
  uint32 groupCountZ;
};

[[vk::binding(0, 2)]] ByteAddressBuffer buffers[];
[[vk::binding(1, 2)]] RWByteAddressBuffer buffers_rw[];

#define MESHLET_DATA_BUFFERS_INDEX 0
#define INDEX_BUFFER_INDEX 1
#define VERTEX_BUFFER_INDEX 2
#define PRIMITIVES_BUFFER_INDEX 3
#define TRANSFORMS_BUFFER_INDEX 4
#define MESHLET_INSTANCE_BUFFERS_INDEX 5

#define VISIBLE_MESHLETS_BUFFER_INDEX 0
#define CULL_TRIANGLES_DISPATCH_PARAMS_BUFFERS_INDEX 1
#define INDIRECT_COMMAND_BUFFER_INDEX 2
#define INSTANCED_INDEX_BUFFER_INDEX 3

Meshlet get_meshlet(uint32 index) { return buffers[MESHLET_DATA_BUFFERS_INDEX].Load<Meshlet>(index * sizeof(Meshlet)); }
MeshletInstance get_meshlet_instance(uint32 index) {
  return buffers[MESHLET_INSTANCE_BUFFERS_INDEX].Load<MeshletInstance>(index * sizeof(MeshletInstance));
}
Vertex get_vertex(uint32 index) { return buffers[VERTEX_BUFFER_INDEX].Load<Vertex>(index * sizeof(Vertex)); }
uint32 get_primitive(uint32 index) { return buffers[PRIMITIVES_BUFFER_INDEX].Load<uint32>(index * sizeof(uint32)); }
uint32 get_index(uint32 index) { return buffers[INDEX_BUFFER_INDEX].Load<uint32>(index * sizeof(uint32)); }

void set_index(uint32 index, uint32 value) { buffers_rw[INSTANCED_INDEX_BUFFER_INDEX].Store<uint32>(index * sizeof(uint32), value); }

uint32 get_visible_meshlet(uint32 index) { return buffers_rw[VISIBLE_MESHLETS_BUFFER_INDEX].Load<uint32>(index * sizeof(uint32)); }

bool rect_intersect_rect(float2 bottomLeft0, float2 topRight0, float2 bottomLeft1, float2 topRight1) {
  return !(any(topRight0 < bottomLeft1) || any(bottomLeft0 > topRight1));
}
