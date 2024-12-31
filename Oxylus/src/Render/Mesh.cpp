#include "Mesh.hpp"

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/math.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <ktx.h>
#include <meshoptimizer.h>
#include <stb_image.h>
#include <vuk/Types.hpp>
#include <vuk/vsl/Core.hpp>

#include <execution>
#include <ranges>
#include <stack>

#include "Assets/AssetManager.hpp"
#include "Core/FileSystem.hpp"

#include "Scene/Components.hpp"

#include "Utils/Log.hpp"
#include "Utils/Profiler.hpp"
#include "Utils/Timer.hpp"

#include "Vulkan/VkContext.hpp"

namespace ox {
Mesh::Mesh(const std::string_view path) { load_from_file(path.data()); }

void Mesh::set_transforms() const {
  struct StackElement {
    Node* node;
    Mat4 parent_global_transform;
  };
  std::stack<StackElement> node_stack;

  for (auto* rootNode : root_nodes) {
    node_stack.emplace(rootNode, rootNode->get_local_transform());
  }

  // Traverse the scene
  while (!node_stack.empty()) {
    auto [node, parent_global_transform] = node_stack.top();
    node_stack.pop();

    const auto global_transform = parent_global_transform * node->get_local_transform();

    node->global_transform = global_transform;

    for (auto* child_node : node->children) {
      node_stack.emplace(child_node, global_transform);
    }
  }
}

Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32>& indices) {
  this->_vertices = vertices;
  this->_indices = indices;

  auto& context = App::get_vkcontext();
  auto compiler = vuk::Compiler{};

  auto [vBuffer, vBufferFut] = create_buffer(*context.superframe_allocator,
                                             vuk::MemoryUsage::eGPUonly,
                                             vuk::DomainFlagBits::eTransferOnGraphics,
                                             std::span(_vertices));

  vBufferFut.wait(*context.superframe_allocator, compiler);
  vertex_buffer = std::move(vBuffer);

  auto [iBuffer, iBufferFut] = create_buffer(*context.superframe_allocator,
                                             vuk::MemoryUsage::eGPUonly,
                                             vuk::DomainFlagBits::eTransferOnGraphics,
                                             std::span(_indices));

  iBufferFut.wait(*context.superframe_allocator, compiler);
  index_buffer = std::move(iBuffer);

  index_count = (uint32)indices.size();
}

static std::vector<Vertex> convert_vertex_buffer_format(const fastgltf::Asset& model,
                                                        std::size_t position_accessor_index,
                                                        std::size_t normal_accessor_index,
                                                        std::optional<std::size_t> texcoord_accessor_index,
                                                        std::optional<std::size_t> color_accessor_index) {
  OX_SCOPED_ZONE;
  std::vector<float3> positions;
  auto& position_accessor = model.accessors[position_accessor_index];
  positions.resize(position_accessor.count);
  fastgltf::iterateAccessorWithIndex<float3>(model, position_accessor, [&](float3 position, std::size_t idx) { positions[idx] = position; });

  std::vector<float3> normals;
  auto& normalAccessor = model.accessors[normal_accessor_index];
  normals.resize(normalAccessor.count);
  fastgltf::iterateAccessorWithIndex<float3>(model, normalAccessor, [&](float3 normal, std::size_t idx) { normals[idx] = normal; });

  std::vector<float2> texcoords;

  // Textureless meshes will use factors instead of textures
  if (texcoord_accessor_index.has_value()) {
    auto& texcoordAccessor = model.accessors[texcoord_accessor_index.value()];
    texcoords.resize(texcoordAccessor.count);
    fastgltf::iterateAccessorWithIndex<float2>(model, texcoordAccessor, [&](float2 texcoord, std::size_t idx) { texcoords[idx] = texcoord; });
  } else {
    // If no texcoord attribute, fill with empty texcoords to keep everything consistent and happy
    texcoords.resize(positions.size(), {});
  }

  std::vector<float4> colors; // TODO: Unused for now...
  if (color_accessor_index.has_value()) {
    auto& color_accessor = model.accessors[color_accessor_index.value()];
    colors.resize(color_accessor.count);
    fastgltf::iterateAccessorWithIndex<float3>(model, color_accessor, [&](float3 color, std::size_t idx) { colors[idx] = float4(color, 1.0f); });
  } else {
    colors.resize(positions.size(), float4(1.0f));
  }

  OX_ASSERT(positions.size() == normals.size() && positions.size() == texcoords.size() && positions.size() == colors.size());

  std::vector<Vertex> vertices;
  vertices.resize(positions.size());

  for (size_t i = 0; i < positions.size(); i++) {
    vertices[i] = {positions[i], glm::packSnorm2x16(math::float32x3_to_oct(normals[i])), texcoords[i]};
  }

  return vertices;
}

static std::vector<uint32> convert_index_buffer_format(const fastgltf::Asset& model, std::size_t indicesAccessorIndex) {
  OX_SCOPED_ZONE;
  auto indices = std::vector<uint32>();
  auto& accessor = model.accessors[indicesAccessorIndex];
  indices.resize(accessor.count);
  fastgltf::iterateAccessorWithIndex<uint32>(model, accessor, [&](uint32 index, size_t idx) { indices[idx] = index; });
  return indices;
}

glm::mat4 node_to_mat4(const fastgltf::Node& node) {
  glm::mat4 transform{1};

  if (auto* trs = std::get_if<fastgltf::TRS>(&node.transform)) {
    // Note: do not use glm::make_quat because glm and glTF use different quaternion component layouts (wxyz vs xyzw)!
    const auto rotation = glm::quat{trs->rotation[3], trs->rotation[0], trs->rotation[1], trs->rotation[2]};
    const auto scale = glm::make_vec3(trs->scale.data());
    const auto translation = glm::make_vec3(trs->translation.data());

    const glm::mat4 rotationMat = glm::mat4_cast(rotation);

    // T * R * S
    transform = glm::translate(Mat4(1.0f), translation) * rotationMat * glm::scale(Mat4(1.0f), scale);
  }
  // else if (auto* mat = std::get_if<fastgltf::Node::>(&node.transform)) {
  //   transform = glm::make_mat4(mat->data());
  // }

  return transform;
}

// TODO: move to cvars maybe
constexpr auto MAX_MESHLET_INDICES = 64u;
constexpr auto MAX_MESHLET_PRIMITIVES = 64u;
constexpr auto MESHLET_CONE_WEIGHT = 0.0f;

struct RawMesh {
  std::vector<Vertex> vertices;
  std::vector<uint32> indices;
  AABB bounding_box;
};

struct NodeTempData {
  struct Indices {
    size_t raw_mesh_index;
    size_t material_index;
  };
  std::vector<Indices> indices;
};

void Mesh::load_from_file(const std::string& file_path, glm::mat4 rootTransform) {
  OX_SCOPED_ZONE;

  Timer timer;

  const auto extension = fs::get_file_extension(file_path);
  const auto is_text = extension == "gltf";
  const auto is_binary = extension == "glb";

  if (!is_text && !is_binary) {
    OX_LOG_ERROR("Only glTF files(.gltf/.glb) are supported!");
    return;
  }

  auto maybeAsset = [&]() -> fastgltf::Expected<fastgltf::Asset> {
    OX_SCOPED_ZONE_N("Parse glTF");
    using fastgltf::Extensions;
    constexpr auto gltfExtensions = Extensions::KHR_texture_basisu | Extensions::KHR_mesh_quantization | Extensions::EXT_meshopt_compression |
                                    Extensions::KHR_lights_punctual | Extensions::KHR_materials_emissive_strength;
    auto parser = fastgltf::Parser(gltfExtensions);

    fastgltf::GltfDataBuffer data;
    data.FromPath(file_path);

    constexpr auto options = fastgltf::Options::LoadExternalBuffers | fastgltf::Options::LoadExternalImages;
    return parser.loadGltf(data, fs::get_directory(file_path), options);
  }();

  if (auto err = maybeAsset.error(); err != fastgltf::Error::None) {
    OX_LOG_ERROR("glTF error: {}", fastgltf::getErrorMessage(err));
    return;
  }

  const auto& asset = maybeAsset.get();

  OX_ASSERT(asset.scenes.size() == 1, "Multiple scenes are not supported for now...");

  auto images = load_images(asset);
  _materials = load_materials(asset, images);

  struct AccessorIndices {
    std::optional<std::size_t> positions_index;
    std::optional<std::size_t> normals_index;
    std::optional<std::size_t> texcoords_index;
    std::optional<std::size_t> colors_index;
    std::optional<std::size_t> indices_index;

    auto operator<=>(const AccessorIndices&) const = default;
  };

  std::vector<std::pair<AccessorIndices, std::size_t>> unique_accessor_combinations = {};

  // <node*, global transform>
  struct StackElement {
    Node* scene_node{};
    const fastgltf::Node* gltf_node{};
    size_t temp_data_index;
  };
  std::stack<StackElement> node_stack;

  std::vector<NodeTempData> temp_data = {};

  for (const fastgltf::Node& node : asset.nodes) {
    const auto name = node.name.empty() ? std::string("Node") : std::string(node.name);
    Node* scene_node = &nodes.emplace_back(name);
    scene_node->index = (uint32)nodes.size() - 1u;
    node_stack.emplace(scene_node, &node, temp_data.size());
    temp_data.emplace_back();
  }

  while (!node_stack.empty()) {
    decltype(node_stack)::value_type top = node_stack.top();
    const auto& [node, gltf_node, tempDataIndex] = top;
    node_stack.pop();

    const glm::mat4 local_transform = node_to_mat4(*gltf_node);

    fastgltf::math::fmat4x4 local_transform_array{};
    std::copy_n(&local_transform[0][0], 16, local_transform_array.data());
    fastgltf::math::fvec3 scale_array{};
    fastgltf::math::fquat rotation_array{};
    fastgltf::math::fvec3 translation_array{};
    fastgltf::math::decomposeTransformMatrix(local_transform_array, scale_array, rotation_array, translation_array);

    node->translation = glm::make_vec3(translation_array.data());
    node->rotation = {rotation_array[3], rotation_array[0], rotation_array[1], rotation_array[2]};
    node->scale = glm::make_vec3(scale_array.data());

    for (auto child_node_index : gltf_node->children) {
      const auto& asset_node = asset.nodes[child_node_index];
      Node* child_scene_node = &nodes[child_node_index];
      child_scene_node->parent = node;
      node->children.emplace_back(child_scene_node);
      node_stack.emplace(child_scene_node, &asset_node, temp_data.size());
      temp_data.emplace_back();
    }

    if (gltf_node->meshIndex.has_value()) {
      // Load each primitive in the mesh
      for (const fastgltf::Mesh& mesh = asset.meshes[gltf_node->meshIndex.value()]; const auto& primitive : mesh.primitives) {
        AccessorIndices accessor_indices;
        if (auto it = primitive.findAttribute("POSITION"); it != primitive.attributes.end()) {
          accessor_indices.positions_index = it->accessorIndex;
        } else {
          OX_ASSERT(false);
        }

        if (auto it = primitive.findAttribute("NORMAL"); it != primitive.attributes.end()) {
          accessor_indices.normals_index = it->accessorIndex;
        } else {
          // TODO: calculate normal
          OX_ASSERT(false);
        }

        if (auto it = primitive.findAttribute("TEXCOORD_0"); it != primitive.attributes.end()) {
          accessor_indices.texcoords_index = it->accessorIndex;
        } else {
          // Okay, texcoord can be safely missing
        }

        if (auto it = primitive.findAttribute("COLOR_0"); it != primitive.attributes.end()) {
          accessor_indices.colors_index = it->accessorIndex;
        }

        OX_ASSERT(primitive.indicesAccessor.has_value() && "Non-indexed meshes are not supported");
        accessor_indices.indices_index = *primitive.indicesAccessor;

        size_t raw_mesh_index = unique_accessor_combinations.size();

        // Only emplace and increment counter if combo does not exist
        if (auto it = std::ranges::find_if(unique_accessor_combinations, [&](const auto& p) { return p.first == accessor_indices; });
            it == unique_accessor_combinations.end()) {
          unique_accessor_combinations.emplace_back(accessor_indices, raw_mesh_index);
        } else {
          // Combo already exists
          raw_mesh_index = it->second;
        }

        const auto material_id = primitive.materialIndex.value_or(0);

        temp_data[tempDataIndex].indices.emplace_back(raw_mesh_index, material_id);
      }
    }
  }

  std::vector<RawMesh> raw_meshes = {};
  raw_meshes.resize(unique_accessor_combinations.size());

  std::transform(std::execution::par,
                 unique_accessor_combinations.begin(),
                 unique_accessor_combinations.end(),
                 raw_meshes.begin(),
                 [&](const std::pair<AccessorIndices, std::size_t>& keyValue) -> RawMesh {
    OX_SCOPED_ZONE_N("Convert vertices and indices");
    const auto& [accessorIndices, _] = keyValue;
    auto vertices = convert_vertex_buffer_format(asset,
                                                 accessorIndices.positions_index.value(),
                                                 accessorIndices.normals_index.value(),
                                                 accessorIndices.texcoords_index,
                                                 accessorIndices.colors_index);
    auto indices = convert_index_buffer_format(asset, accessorIndices.indices_index.value());

    const auto& positionAccessor = asset.accessors[accessorIndices.positions_index.value()];

    glm::vec3 bboxMin{};
    if (auto* dv = std::get_if<std::pmr::vector<double>>(&positionAccessor.min)) {
      bboxMin = {(*dv)[0], (*dv)[1], (*dv)[2]};
    }
    if (auto* iv = std::get_if<std::pmr::vector<int64_t>>(&positionAccessor.min)) {
      bboxMin = {(*iv)[0], (*iv)[1], (*iv)[2]};
    }

    glm::vec3 bboxMax{};
    if (auto* dv = std::get_if<std::pmr::vector<double>>(&positionAccessor.max)) {
      bboxMax = {(*dv)[0], (*dv)[1], (*dv)[2]};
    }
    if (auto* iv = std::get_if<std::pmr::vector<int64_t>>(&positionAccessor.max)) {
      bboxMax = {(*iv)[0], (*iv)[1], (*iv)[2]};
    }

    return RawMesh{
      .vertices = std::move(vertices),
      .indices = std::move(indices),
      .bounding_box = AABB{bboxMin, bboxMax},
    };
  });

  const auto baseVertexOffset = _vertices.size();
  const auto baseIndexOffset = _indices.size();
  const auto basePrimitiveOffset = _primitives.size();

  uint32_t vertex_offset = (uint32_t)baseVertexOffset;
  uint32_t index_offset = (uint32_t)baseIndexOffset;
  uint32_t primitive_offset = (uint32_t)basePrimitiveOffset;

  struct MeshletInfo {
    const RawMesh* rawMeshPtr{};
    std::span<const Vertex> vertices;
    std::vector<uint32_t> meshlet_indices;
    std::vector<uint8_t> meshlet_primitives;
    std::vector<meshopt_Meshlet> raw_meshlets;
  };

  std::vector<MeshletInfo> meshlet_infos(raw_meshes.size());

  std::transform(std::execution::par, raw_meshes.begin(), raw_meshes.end(), meshlet_infos.begin(), [](const RawMesh& mesh) -> MeshletInfo {
    OX_SCOPED_ZONE_N("Create meshlets for mesh");

    const auto max_meshlets = meshopt_buildMeshletsBound(mesh.indices.size(), MAX_MESHLET_INDICES, MAX_MESHLET_PRIMITIVES);

    MeshletInfo meshletInfo;
    auto& [raw_mesh_ptr, vertices, meshlet_indices, meshlet_primitives, raw_meshlets] = meshletInfo;

    raw_mesh_ptr = &mesh;
    vertices = mesh.vertices;
    meshlet_indices.resize(max_meshlets * MAX_MESHLET_INDICES);
    meshlet_primitives.resize(max_meshlets * MAX_MESHLET_PRIMITIVES * 3);
    raw_meshlets.resize(max_meshlets);

    const auto meshlet_count = [&] {
      OX_SCOPED_ZONE_N("meshopt_buildMeshlets");
      return meshopt_buildMeshlets(raw_meshlets.data(),
                                   meshlet_indices.data(),
                                   meshlet_primitives.data(),
                                   mesh.indices.data(),
                                   mesh.indices.size(),
                                   reinterpret_cast<const float*>(mesh.vertices.data()),
                                   mesh.vertices.size(),
                                   sizeof(Vertex),
                                   MAX_MESHLET_INDICES,
                                   MAX_MESHLET_PRIMITIVES,
                                   MESHLET_CONE_WEIGHT);
    }();

    const auto& last_meshlet = raw_meshlets[meshlet_count - 1];
    meshlet_indices.resize(last_meshlet.vertex_offset + last_meshlet.vertex_count);
    meshlet_primitives.resize(last_meshlet.triangle_offset + ((last_meshlet.triangle_count * 3 + 3) & ~3));
    raw_meshlets.resize(meshlet_count);

    return meshletInfo;
  });

  auto per_mesh_meshlets = std::vector<std::vector<uint32_t>>(meshlet_infos.size());

  // For each mesh, create "meshlet templates" (meshlets without per-instance data) and copy vertices, indices, and primitives to mega buffers
  for (size_t mesh_index = 0; mesh_index < meshlet_infos.size(); mesh_index++) {
    auto& [rawMesh, vert, meshlet_indices, meshlet_primitives, rawMeshlets] = meshlet_infos[mesh_index];
    per_mesh_meshlets[mesh_index].reserve(rawMeshlets.size());

    for (const auto& meshlet : rawMeshlets) {
      auto min = glm::vec3(std::numeric_limits<float>::max());
      auto max = glm::vec3(std::numeric_limits<float>::lowest());
      for (uint32_t i = 0; i < meshlet.triangle_count * 3; ++i) {
        const auto& vertex = rawMesh->vertices[meshlet_indices[meshlet.vertex_offset + meshlet_primitives[meshlet.triangle_offset + i]]];
        min = glm::min(min, vertex.position);
        max = glm::max(max, vertex.position);
      }

      auto meshlet_id = _meshlets.size();
      per_mesh_meshlets[mesh_index].emplace_back((uint32_t)meshlet_id);

      _meshlets.emplace_back(Mesh::Meshlet{
        .vertex_offset = vertex_offset,
        .index_offset = index_offset + meshlet.vertex_offset,
        .primitive_offset = primitive_offset + meshlet.triangle_offset,
        .index_count = meshlet.vertex_count,
        .primitive_count = meshlet.triangle_count,
        .aabbMin = {min.x, min.y, min.z},
        .aabbMax = {max.x, max.y, max.z},
      });
    }

    vertex_offset += (uint32_t)vert.size();
    index_offset += (uint32_t)meshlet_indices.size();
    primitive_offset += (uint32_t)meshlet_primitives.size();

    std::ranges::copy(vert, std::back_inserter(_vertices));
    std::ranges::copy(meshlet_indices, std::back_inserter(_indices));
    std::ranges::copy(meshlet_primitives, std::back_inserter(_primitives));
  }

  for (size_t i = 0; auto& node : nodes) {
    if (!node.parent)
      root_nodes.emplace_back(&node);

    for (const auto& [rawMeshIndex, materialId] : temp_data[i++].indices) {
      for (auto meshlet_index : per_mesh_meshlets[rawMeshIndex]) {
        // Instance index is determined each frame
        node.meshlet_indices.emplace_back(meshlet_index, 0, (uint32_t)materialId);
      }
    }
  }

  if (root_nodes.empty()) {
    root_nodes.emplace_back(&nodes.front());
  }

  set_transforms();

  index_count = (uint32)_indices.size();
  vertex_count = (uint32)_vertices.size();

// create buffers
#if 0
  const auto& ctx = App::get_vkcontext();
  auto compiler = vuk::Compiler{};

  auto [vBuffer, vBufferFut] = create_buffer(*ctx->superframe_allocator,
                                             vuk::MemoryUsage::eGPUonly,
                                             vuk::DomainFlagBits::eTransferOnGraphics,
                                             std::span(_vertices));

  vBufferFut.wait(*ctx->superframe_allocator, compiler);
  vertex_buffer = std::move(vBuffer);

  auto [iBuffer, iBufferFut] = create_buffer(*ctx->superframe_allocator,
                                             vuk::MemoryUsage::eGPUonly,
                                             vuk::DomainFlagBits::eTransferOnGraphics,
                                             std::span(_indices));

  iBufferFut.wait(*ctx->superframe_allocator, compiler);
  index_buffer = std::move(iBuffer);
#endif
  OX_LOG_INFO("Loaded mesh {0}:{1}", fs::get_name_with_extension(file_path), timer.get_elapsed_ms());
}

const Mesh* Mesh::bind_vertex_buffer(vuk::CommandBuffer& command_buffer) const {
  OX_SCOPED_ZONE;
  command_buffer.bind_vertex_buffer(0, *vertex_buffer, 0, vertex_pack);
  return this;
}

const Mesh* Mesh::bind_index_buffer(vuk::CommandBuffer& command_buffer) const {
  OX_SCOPED_ZONE;
  command_buffer.bind_index_buffer(*index_buffer, vuk::IndexType::eUint32);
  return this;
}

std::vector<Shared<Texture>> Mesh::load_images(const fastgltf::Asset& asset) {
  struct RawImageData {
    // Used for ktx and non-ktx images alike
    std::unique_ptr<std::byte[]> encoded_pixel_data = {};
    std::size_t encoded_pixel_size = 0;

    bool is_ktx = false;
    vuk::Format format_ktx;
    int width1 = 0;
    int height = 0;
    std::string name;

    // Non-ktx. Raw decoded pixel data
    std::unique_ptr<unsigned char[]> data = {};

    // ktx
    std::unique_ptr<ktxTexture2, decltype([](ktxTexture2* p) { ktxTexture_Destroy(ktxTexture(p)); })> ktx = {};
  };

  auto make_raw_image_data = [](const void* data, std::size_t data_size, fastgltf::MimeType mime_type, std::string_view name) -> RawImageData {
    auto data_copy = std::make_unique<std::byte[]>(data_size);
    std::copy_n(static_cast<const std::byte*>(data), data_size, data_copy.get());

    return RawImageData{
      .encoded_pixel_data = std::move(data_copy),
      .encoded_pixel_size = data_size,
      .is_ktx = mime_type == fastgltf::MimeType::KTX2,
      .name = std::string(name),
    };
  };

  const auto indices = std::ranges::iota_view((size_t)0, asset.images.size());

  // Load and decode image data locally, in parallel
  auto raw_image_datas = std::vector<RawImageData>(asset.images.size());

  std::transform(std::execution::par, indices.begin(), indices.end(), raw_image_datas.begin(), [&](size_t index) {
    OX_SCOPED_ZONE_N("Load Image");
    const fastgltf::Image& image = asset.images[index];
    OX_ZONE_NAME(image.name.c_str(), image.name.size());

    RawImageData rawImage = [&] {
      if (const auto* filePath = std::get_if<fastgltf::sources::URI>(&image.data)) {
        OX_ASSERT(filePath->fileByteOffset == 0, "File offsets are not supported.");
        OX_ASSERT(filePath->uri.isLocalPath(), "Only loading local files are supported.");

        const auto file_data = fs::read_file_binary(filePath->uri.path());

        return make_raw_image_data(file_data.data(), file_data.size(), filePath->mimeType, image.name);
      }

      if (const auto* vector = std::get_if<fastgltf::sources::Array>(&image.data)) {
        return make_raw_image_data(vector->bytes.data(), vector->bytes.size(), vector->mimeType, image.name);
      }

      if (const auto* view = std::get_if<fastgltf::sources::BufferView>(&image.data)) {
        auto& buffer_view = asset.bufferViews[view->bufferViewIndex];
        auto& buffer = asset.buffers[buffer_view.bufferIndex];
        if (const auto* vector = std::get_if<fastgltf::sources::Array>(&buffer.data)) {
          return make_raw_image_data(vector->bytes.data() + buffer_view.byteOffset, buffer_view.byteLength, view->mimeType, image.name);
        }
      }

      return RawImageData{};
    }();

    if (rawImage.is_ktx) {
      OX_SCOPED_ZONE_N("Decode KTX2");
      ktxTexture2* ktx{};
      if (const auto result = ktxTexture2_CreateFromMemory(reinterpret_cast<const ktx_uint8_t*>(rawImage.encoded_pixel_data.get()),
                                                           rawImage.encoded_pixel_size,
                                                           KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
                                                           &ktx);
          result != KTX_SUCCESS) {
        OX_LOG_ERROR("Couldn't load KTX2 file {}", ktxErrorString(result));
      }

      rawImage.format_ktx = vuk::Format::eBc7UnormBlock;
      constexpr ktx_transcode_fmt_e ktxTranscodeFormat = KTX_TTF_BC7_RGBA;

      // If the image needs is in a supercompressed encoding, transcode it to a desired format
      if (ktxTexture2_NeedsTranscoding(ktx)) {
        OX_SCOPED_ZONE_N("Transcode KTX 2 Texture");
        if (const auto result = ktxTexture2_TranscodeBasis(ktx, ktxTranscodeFormat, KTX_TF_HIGH_QUALITY); result != KTX_SUCCESS) {
          OX_LOG_ERROR("Couldn't transcode KTX2 file {}", ktxErrorString(result));
        }
      } else {
        // Use the format that the image is already in
        rawImage.format_ktx = vuk::Format(VkFormat(ktx->vkFormat));
      }

      rawImage.width1 = ktx->baseWidth;
      rawImage.height = ktx->baseHeight;
      rawImage.ktx.reset(ktx);
    } else {
      OX_SCOPED_ZONE_N("Decode JPEG/PNG");
      int x, y, comp;
      auto* pixels = stbi_load_from_memory(reinterpret_cast<const unsigned char*>(rawImage.encoded_pixel_data.get()),
                                           static_cast<int>(rawImage.encoded_pixel_size),
                                           &x,
                                           &y,
                                           &comp,
                                           4);

      OX_ASSERT(pixels != nullptr);

      rawImage.width1 = x;
      rawImage.height = y;
      rawImage.data.reset(pixels);
    }

    return rawImage;
  });

  // Upload image data to GPU
  std::vector<Shared<Texture>> loaded_images = {};
  loaded_images.reserve(raw_image_datas.size());

  for (const auto& image : raw_image_datas) {
    const vuk::Extent3D dims = {static_cast<uint32>(image.width1), static_cast<uint32>(image.height), 1u};

    const auto* ktx = image.ktx.get();

    auto ci = TextureLoadInfo{
      .path = {},
      .preset = Preset::eMap2D,
      .extent = dims,
      .format = vuk::Format::eR8G8B8A8Unorm,
      .data = image.is_ktx ? ktx->kvData : image.data.get(),
      .mime = image.is_ktx ? TextureLoadInfo::MimeType::KTX : TextureLoadInfo::MimeType::Generic,
    };

    const auto& t = loaded_images.emplace_back(AssetManager::get_texture_asset(image.name, ci));
    t->set_name(image.name);
  }

  return loaded_images;
}

std::vector<Shared<PBRMaterial>> Mesh::load_materials(const fastgltf::Asset& asset, const std::vector<Shared<Texture>>& images) {
  OX_SCOPED_ZONE;

  std::vector<Shared<PBRMaterial>> materials = {};
  materials.reserve(asset.materials.size());

  if (asset.materials.empty()) {
    const auto& ma = materials.emplace_back(create_shared<PBRMaterial>());
    ma->create();
  }

  for (auto& material : asset.materials) {
    const Shared<PBRMaterial>& ma = materials.emplace_back(create_shared<PBRMaterial>(material.name.c_str()));
    ma->create();

    if (material.pbrData.baseColorTexture.has_value()) {
      const auto& base_color_texture = asset.textures[material.pbrData.baseColorTexture->textureIndex];

      auto& image = images[base_color_texture.imageIndex.value()];
      ma->set_albedo_texture(image);

      // extract sampler
      if (!asset.samplers.empty()) {
        const auto extract_sampler = [](const fastgltf::Sampler& sampler) -> PBRMaterial::Sampler {
          switch (sampler.magFilter.value_or(fastgltf::Filter::Linear)) {
            case fastgltf::Filter::Nearest:
            case fastgltf::Filter::NearestMipMapNearest:
            case fastgltf::Filter::NearestMipMapLinear : return PBRMaterial::Sampler::Nearest;
            case fastgltf::Filter::Linear              :
            case fastgltf::Filter::LinearMipMapNearest :
            case fastgltf::Filter::LinearMipMapLinear  :
            default                                    : return PBRMaterial::Sampler::Anisotropy;
          }
        };

        const auto sampler = extract_sampler(asset.samplers.at(base_color_texture.samplerIndex.value_or(fastgltf::Filter::Linear)));
        ma->set_sampler(sampler);
      }
    }

    if (material.occlusionTexture.has_value()) {
      const auto& occlusion_texture = asset.textures[material.occlusionTexture->textureIndex];
      auto& image = images[occlusion_texture.imageIndex.value()];
      ma->set_ao_texture(image);
    }

    if (material.emissiveTexture.has_value()) {
      const auto& emissive_texture = asset.textures[material.emissiveTexture->textureIndex];
      auto& image = images[emissive_texture.imageIndex.value()];
      ma->set_emissive_texture(image);
    }

    if (material.normalTexture.has_value()) {
      const auto& normal_texture = asset.textures[material.normalTexture->textureIndex];
      auto& image = images[normal_texture.imageIndex.value()];
      ma->set_normal_texture(image);
      // ma->normalXyScale = material.normalTexture->scale; TODO:
    }

    if (material.pbrData.metallicRoughnessTexture.has_value()) {
      const auto& metallic_roughness_texture = asset.textures[material.pbrData.metallicRoughnessTexture->textureIndex];
      auto& image = images[metallic_roughness_texture.imageIndex.value()];
      ma->set_physical_texture(image);
    }

    // TODO:
    // - Anisotropy
    // - Clearcoat
    // - Sheen

    ma->set_color(glm::make_vec4(material.pbrData.baseColorFactor.data()))
      ->set_metallic(material.pbrData.metallicFactor)
      ->set_roughness(material.pbrData.roughnessFactor)
      ->set_emissive(float4(glm::make_vec3(material.emissiveFactor.data()), material.emissiveStrength))
      ->set_reflectance(0.04f)
      ->set_double_sided(material.doubleSided)
      ->set_alpha_mode((PBRMaterial::AlphaMode)(uint32)material.alphaMode)
      ->set_alpha_cutoff(material.alphaCutoff);
  }

  return materials;
}
} // namespace ox
