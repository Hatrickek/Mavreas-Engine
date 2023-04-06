#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>

#include "VulkanCommandBuffer.h"
#include "VulkanSwapchain.h"
#include "VulkanFramebuffer.h"
#include "VulkanPipeline.h"
#include "Core/Components.h"

#include "Render/Camera.h"
#include "Render/RendererConfig.h"
#include "Render/RenderGraph.h"

namespace Oxylus {
  using vDT = vk::DescriptorType;
  using vSS = vk::ShaderStageFlagBits;

  class Entity;
  constexpr auto MAX_NUM_LIGHTS = 1000;
  constexpr auto MAX_NUM_MESHES = 1000;
  constexpr auto MAX_NUM_LIGHTS_PER_TILE = 128;
  constexpr auto MAX_NUM_FRUSTUMS = 20000;
  constexpr auto PIXELS_PER_TILE = 16;
  constexpr auto TILES_PER_THREADGROUP = 16;
  constexpr auto SHADOW_MAP_CASCADE_COUNT = 4;

  class VulkanRenderer {
  public:
    static struct RendererContext {
      RenderGraph RenderGraph;
      bool Initialized = false;

      vk::DescriptorPool DescriptorPool;
      std::vector<VulkanCommandBuffer> ComputeCommandBuffers;
      VulkanCommandBuffer TimelineCommandBuffer;
      VulkanCommandBuffer DirectShadowCommandBuffer;
      VulkanCommandBuffer PBRPassCommandBuffer;
      VulkanCommandBuffer PostProcessCommandBuffer;
      VulkanCommandBuffer BloomPassCommandBuffer;
      VulkanCommandBuffer FrustumCommandBuffer;
      VulkanCommandBuffer DepthPassCommandBuffer;
      VulkanCommandBuffer SSAOCommandBuffer;
      VulkanCommandBuffer SSRCommandBuffer;
      VulkanCommandBuffer CompositeCommandBuffer;

      vk::CommandPool CommandPool;

      //Camera
      Camera* CurrentCamera = nullptr;

      //Viewport
      ImVec2 ViewportSize;
    } s_RendererContext;

    static struct RendererData {
      struct Vertex {
        glm::vec3 Position{};
        glm::vec3 Normal{};
        glm::vec2 UV{};
        glm::vec4 Color{};
        glm::vec4 Joint0{};
        glm::vec4 Weight0{};
        glm::vec4 Tangent{};
      };

      struct Frustum {
        glm::vec4 planes[4] = {glm::vec4(0)};
      } Frustums[MAX_NUM_FRUSTUMS];

      struct UBOVS {
        glm::mat4 projection;
        glm::mat4 view;
        glm::vec3 camPos;
      } UBO_VS;

      struct PBRPassParams {
        int numLights = 0;
        int debugMode = 0;
        float lodBias = 1.0f;
        glm::ivec2 numThreads;
        glm::ivec2 screenDimensions;
        glm::ivec2 numThreadGroups;
      } UBO_PbrPassParams;

      struct UBOComposite {
        int Tonemapper = RendererConfig::TONEMAP_ACES;                  // 0- Aces 1- Uncharted 2-Filmic 3- Reinhard
        float Exposure = 1.0f;
        float Gamma = 2.5f;
        int EnableSSAO = 1;
        int EnableBloom = 1;
        int EnableSSR = 1;
        glm::vec2 _pad{};
        glm::vec4 VignetteColor = glm::vec4(0.0f, 0.0f, 0.0f, 0.25f);    // rgb: color, a: intensity
        glm::vec4 VignetteOffset = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);    // xy: offset, z: useMask, w: enable effect
      } UBO_CompositeParams;

      struct SSAOParamsUB {
        glm::vec4 ssaoSamples[64] = {};
        float radius = 0.2f;
      } UBO_SSAOParams;

      struct SSAOBlurParamsUB {
        glm::vec4 texelOffset = {};
        int texelRadius = 2;
      } UBO_SSAOBlur;

      struct BloomUB {
        glm::vec4 Params; // x: threshold, y: clamp, z: radius, w: unused
        glm::ivec2 Stage;  // x: stage, y: lod
      } UBO_Bloom;

      struct SSR_UBO {
        int Samples = 30;             
        int BinarySearchSamples = 8; 
        float MaxDist = 50.0f;           
      } UBO_SSR;

      struct DirectShadowUB {
        glm::mat4 cascadeViewProjMat[SHADOW_MAP_CASCADE_COUNT]{};
        float cascadeSplits[4]{};
      } UBO_DirectShadow;

      VulkanBuffer SkyboxBuffer;
      VulkanBuffer ParametersBuffer;
      VulkanBuffer VSBuffer;
      VulkanBuffer LightsBuffer;
      VulkanBuffer FrustumBuffer;
      VulkanBuffer LighIndexBuffer;
      VulkanBuffer LighGridBuffer;
      VulkanBuffer SSAOBuffer;
      VulkanBuffer CompositeParametersBuffer;
      VulkanBuffer DirectShadowBuffer;
      VulkanBuffer BloomBuffer;
      VulkanBuffer SSRBuffer;

      vk::DescriptorSetLayout ImageDescriptorSetLayout;

      VulkanImage SSAONoise;
    } s_RendererData;

    static struct RendererResources {
      VulkanImage CubeMap;
      VulkanImage LutBRDF;
      VulkanImage IrradianceCube;
      VulkanImage PrefilteredCube;
      VulkanImage DirectShadowsDepthArray;
    } s_Resources;

    static struct Pipelines {
      VulkanPipeline SkyboxPipeline;
      VulkanPipeline PBRPipeline;
      VulkanPipeline PostProcessPipeline;
      VulkanPipeline DepthPrePassPipeline;
      VulkanPipeline SSAOPassPipeline;
      VulkanPipeline SSAOHBlurPassPipeline;
      VulkanPipeline SSAOVBlurPassPipeline;
      VulkanPipeline QuadPipeline;
      VulkanPipeline FrustumGridPipeline;
      VulkanPipeline LightListPipeline;
      VulkanPipeline UIPipeline;
      VulkanPipeline DirectShadowDepthPipeline;
      VulkanPipeline GaussianBlurPipeline;
      VulkanPipeline UnlitPipeline;
      VulkanPipeline BloomPipeline;
      VulkanPipeline SSRPipeline;
      VulkanPipeline CompositePipeline;
    } s_Pipelines;

    static struct FrameBuffers {
      VulkanFramebuffer PBRPassFB;
      VulkanFramebuffer PostProcessPassFB;
      VulkanFramebuffer DepthNormalPassFB;
      VulkanImage SSAOPassImage;
      VulkanImage SSRPassImage;
      VulkanImage CompositePassImage;
      VulkanImage BloomPassImage;
      VulkanImage BloomUpsampleImage;
      VulkanImage BloomDownsampleImage;
      std::vector<VulkanFramebuffer> DirectionalCascadesFB;
    } s_FrameBuffers;

    static VulkanSwapchain SwapChain;

    static void Init();
    static void InitRenderGraph();

    static void Shutdown();

    static void UpdateUniformBuffers();
    static void CreateGraphicsPipelines();
    static void CreateFramebuffers();
    static void ResizeBuffers();
    static void UpdateSkyboxDescriptorSets();
    static void UpdateComputeDescriptorSets();
    static void UpdateSSAODescriptorSets();

    //Queue
    static void Submit(const std::function<void()>& submitFunc);
    static void SubmitOnce(const std::function<void(VulkanCommandBuffer& cmdBuffer)>& submitFunc);
    static void SubmitQueue(const VulkanCommandBuffer& commandBuffer);

    //Lighting
    static void SubmitLights(std::vector<Entity>&& lights);
    static void SubmitSkyLight(const Entity& entity);

    //Drawing
    static void Draw();
    static void DrawFullscreenQuad(const vk::CommandBuffer& commandBuffer, bool bindVertex = false);
    static void SubmitMesh(Mesh& mesh, const glm::mat4& transform, std::vector<Ref<Material>>& materials, uint32_t submeshIndex);
    static void SubmitQuad(const glm::mat4& transform, const Ref<VulkanImage>& image, const glm::vec4& color);

    static const VulkanImage& GetFinalImage();

    static void SetCamera(Camera& camera);

    static void OnResize();
    static void WaitDeviceIdle();
    static void GeneratePrefilter();
    static void WaitGraphicsQueueIdle();

  private:
    //Mesh
    struct MeshData {
      Mesh& MeshGeometry;
      std::vector<Ref<Material>>& Materials;
      glm::mat4 Transform;
      uint32_t SubmeshIndex = 0;

      MeshData(Mesh& mesh,
               const glm::mat4& transform,
               std::vector<Ref<Material>>& materials,
               const uint32_t submeshIndex) : MeshGeometry(mesh), Materials(materials), Transform(transform),
                                              SubmeshIndex(submeshIndex) {}
    };

    static std::vector<MeshData> s_MeshDrawList;

    static void RenderNode(const Mesh::Node* node,
                           const vk::CommandBuffer& commandBuffer,
                           const VulkanPipeline& pipeline,
                           const std::function<bool(Mesh::Primitive* prim)>& perMeshFunc);
    static void RenderMesh(const MeshData& mesh,
                           const vk::CommandBuffer& commandBuffer,
                           const VulkanPipeline& pipeline,
                           const std::function<bool(Mesh::Primitive* prim)>& perMeshFunc);

    //Lighting
    struct LightingData {
      glm::vec4 PositionAndIntensity;
      glm::vec4 ColorAndRadius;
      glm::vec4 Rotation;
    };

    static Entity s_Skylight;
    static std::vector<Entity> s_SceneLights;
    static std::vector<LightingData> s_PointLightsData;

    static void UpdateCascades(const glm::mat4& Transform, Camera* camera, RendererData::DirectShadowUB& cascadesUbo);
    static void UpdateLightingData();

    //Particle
    static constexpr uint32_t MAX_PARTICLE_COUNT = 800;

    struct QuadData {
      glm::mat4 Transform;
      const Ref<VulkanImage>& Image;
      const glm::vec4 Color;

      QuadData(const glm::mat4& transform, const Ref<VulkanImage>& image, const glm::vec4 color) : Transform(transform),
        Image(image), Color(color) { }
    };

    static std::vector<RendererData::Vertex> s_QuadVertexDataBuffer;
    static std::vector<QuadData> s_QuadDrawList;

    static void DrawQuad(const vk::CommandBuffer& commandBuffer);
    static void InitQuadVertexBuffer();
    static void FlushQuadVertexBuffer();

    //Config
    static RendererConfig s_RendererConfig;
  };
}
