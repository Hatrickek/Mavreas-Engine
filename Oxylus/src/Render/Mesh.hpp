#pragma once

#include <glm/detail/type_quat.hpp>
#include <glm/glm.hpp>
#include <string>
#include <vector>

#include "Assets/PBRMaterial.hpp"

#include <vuk/Buffer.hpp>

#include "BoundingVolume.hpp"
#include "MeshVertex.hpp"

#include "Core/Types.hpp"

namespace fastgltf {
class Asset;
}
namespace ox {
class Mesh : public Asset {
public:
  struct Meshlet {
    uint32_t vertex_offset = 0;
    uint32_t index_offset = 0;
    uint32_t primitive_offset = 0;
    uint32_t index_count = 0;
    uint32_t primitive_count = 0;
    float aabbMin[3];
    float aabbMax[3];
  };

  struct MeshletInstance {
    uint32_t meshletId;
    uint32_t instanceId;
    uint32_t materialId = 0;
  };

  struct Node {
    std::string name;

    Vec3 translation = {};
    Quat rotation = {};
    Vec3 scale = Vec3(1.0f);
    Mat4 local_transform = {};
    Mat4 global_transform = {};

    AABB aabb;

    uint32_t index = 0;
    Node* parent = nullptr;
    std::vector<Node*> children = {};
    std::vector<MeshletInstance> meshlet_indices;

    Mat4 get_local_transform() const { return translate(Mat4(1.0f), translation) * Mat4(rotation) * glm::scale(Mat4(1.0f), scale); }
  };

  std::vector<Node*> root_nodes;
  std::vector<Node> nodes;
  std::vector<Meshlet> _meshlets;
  std::vector<Vertex> _vertices;
  std::vector<uint32> _indices;
  std::vector<uint8_t> _primitives;
  std::vector<Shared<PBRMaterial>> _materials;

  uint32 index_count = 0;
  uint32 vertex_count = 0;
  vuk::Unique<vuk::Buffer> vertex_buffer;
  vuk::Unique<vuk::Buffer> index_buffer;

  Mesh() = default;
  ~Mesh() = default;
  Mesh(std::string_view path);
  Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);

  void load_from_file(const std::string& file_path, glm::mat4 rootTransform = glm::identity<glm::mat4>());

  void set_transforms() const;

  const Mesh* bind_vertex_buffer(vuk::CommandBuffer& command_buffer) const;
  const Mesh* bind_index_buffer(vuk::CommandBuffer& command_buffer) const;

private:
  [[nodiscard]] std::vector<Shared<Texture>> load_images(const fastgltf::Asset& asset);
  [[nodiscard]] std::vector<Shared<PBRMaterial>> load_materials(const fastgltf::Asset& asset, const std::vector<Shared<Texture>>& images);
};
} // namespace ox
