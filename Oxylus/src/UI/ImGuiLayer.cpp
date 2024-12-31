#include "ImGuiLayer.hpp"
#include <backends/imgui_impl_glfw.h>
#include <imgui.h>
#include <plf_colony.h>

#include <glm/common.hpp>

#include <icons/IconsMaterialDesignIcons.h>
#include <icons/MaterialDesign.inl>
#include <vuk/RenderGraph.hpp>
#include <vuk/runtime/CommandBuffer.hpp>
#include <vuk/runtime/vk/AllocatorHelpers.hpp>
#include <vuk/runtime/vk/Pipeline.hpp>
#include <vuk/vsl/Core.hpp>

#include "Core/App.hpp"
#include "ImGuizmo.h"
#include "imgui_frag.hpp"
#include "imgui_vert.hpp"

#include "GLFW/glfw3.h"

#include "Render/Utils/VukCommon.hpp"
#include "Render/Vulkan/VkContext.hpp"
#include "Render/Window.hpp"
#include "Utils/Profiler.hpp"

namespace ox {
static ImVec4 darken(ImVec4 c, float p) { return {glm::max(0.f, c.x - 1.0f * p), glm::max(0.f, c.y - 1.0f * p), glm::max(0.f, c.z - 1.0f * p), c.w}; }
static ImVec4 lighten(ImVec4 c, float p) {
  return {glm::max(0.f, c.x + 1.0f * p), glm::max(0.f, c.y + 1.0f * p), glm::max(0.f, c.z + 1.0f * p), c.w};
}

ImFont* ImGuiLayer::regular_font = nullptr;
ImFont* ImGuiLayer::small_font = nullptr;
ImFont* ImGuiLayer::bold_font = nullptr;

ImGuiLayer::ImGuiLayer() : Layer("ImGuiLayer") {}

void ImGuiLayer::on_attach(EventDispatcher&) {
  OX_SCOPED_ZONE;
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard /*| ImGuiConfigFlags_ViewportsEnable*/ | ImGuiConfigFlags_DockingEnable |
                    ImGuiConfigFlags_DpiEnableScaleFonts | ImGuiConfigFlags_DpiEnableScaleViewports;
  io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
  /*io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;*/

  init_for_vulkan();

  apply_theme();
  set_style();
}

void ImGuiLayer::add_icon_font(float font_size) {
  OX_SCOPED_ZONE;
  const ImGuiIO& io = ImGui::GetIO();
  static constexpr ImWchar ICONS_RANGES[] = {ICON_MIN_MDI, ICON_MAX_MDI, 0};
  ImFontConfig icons_config;
  // merge in icons from Font Awesome
  icons_config.MergeMode = true;
  icons_config.PixelSnapH = true;
  icons_config.GlyphOffset.y = 0.5f;
  icons_config.OversampleH = icons_config.OversampleV = 3;
  icons_config.GlyphMinAdvanceX = 4.0f;
  icons_config.SizePixels = font_size;

  io.Fonts->AddFontFromMemoryCompressedTTF(MaterialDesign_compressed_data, MaterialDesign_compressed_size, font_size, &icons_config, ICONS_RANGES);
}

void ImGuiLayer::init_for_vulkan() {
  OX_SCOPED_ZONE;
  ImGui_ImplGlfw_InitForVulkan(Window::get_glfw_window(), true);

  // Upload Fonts
  const auto regular_font_path = App::get_asset_directory("Fonts/FiraSans-Regular.ttf");
  const auto bold_font_path = App::get_asset_directory("Fonts/FiraSans-Bold.ttf");

  const ImGuiIO& io = ImGui::GetIO();
  constexpr float font_size = 16.0f;
  constexpr float font_size_small = 12.0f;

  ImFontConfig fonts_config;
  fonts_config.MergeMode = false;
  fonts_config.PixelSnapH = true;
  fonts_config.OversampleH = fonts_config.OversampleV = 3;
  fonts_config.GlyphMinAdvanceX = 4.0f;
  fonts_config.SizePixels = font_size;

  {
    OX_SCOPED_ZONE_N("Font Loading/Building");
    regular_font = io.Fonts->AddFontFromFileTTF(regular_font_path.c_str(), font_size, &fonts_config);
    add_icon_font(font_size);
    small_font = io.Fonts->AddFontFromFileTTF(regular_font_path.c_str(), font_size_small, &fonts_config);
    add_icon_font(font_size);
    bold_font = io.Fonts->AddFontFromFileTTF(bold_font_path.c_str(), font_size, &fonts_config);
    add_icon_font(font_size);
    io.Fonts->TexGlyphPadding = 1;
    for (int n = 0; n < io.Fonts->ConfigData.Size; n++) {
      ImFontConfig* font_config = &io.Fonts->ConfigData[n];
      font_config->RasterizerMultiply = 1.0f;
    }
    io.Fonts->Build();
  }

  imgui_impl_vuk_init(*App::get_vkcontext().superframe_allocator);
}

void ImGuiLayer::imgui_impl_vuk_init(vuk::Allocator& allocator) {
  OX_SCOPED_ZONE;
  auto& ctx = allocator.get_context();
  auto& io = ImGui::GetIO();
  io.BackendRendererName = "oxylus";
  io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset; // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.

  unsigned char* pixels;
  int width, height;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

  imgui_data.font_texture = create_shared<Texture>();
  imgui_data.font_texture->create_texture({(unsigned)width, (unsigned)height, 1}, pixels, vuk::Format::eR8G8B8A8Srgb, Preset::eRTT2DUnmipped);
  imgui_data.font_image = {true, *imgui_data.font_texture->get_view(), 0};
  io.Fonts->TexID = &imgui_data.font_image;

  vuk::PipelineBaseCreateInfo pci;
  pci.add_static_spirv(imgui_vert, sizeof(imgui_vert) / 4, "imgui.vert");
  pci.add_static_spirv(imgui_frag, sizeof(imgui_frag) / 4, "imgui.frag");
  ctx.create_named_pipeline("imgui", pci);
}

void ImGuiLayer::on_detach() {
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}

void ImGuiLayer::begin() {
  OX_SCOPED_ZONE;
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  sampled_images.clear();
  sampled_attachments.clear();
}

vuk::Value<vuk::ImageAttachment> ImGuiLayer::render_draw_data(vuk::Allocator& allocator,
                                                              vuk::Compiler& compiler,
                                                              vuk::Value<vuk::ImageAttachment> target) const {
  OX_SCOPED_ZONE;
  auto reset_render_state = [this](const ImGuiData& data, vuk::CommandBuffer& command_buffer, const vuk::Buffer& vertex, const vuk::Buffer& index) {
    command_buffer.bind_image(0, 0, *data.font_texture->get_view()).bind_sampler(0, 0, vuk::LinearSamplerRepeated);
    if (index.size > 0) {
      command_buffer.bind_index_buffer(index, sizeof(ImDrawIdx) == 2 ? vuk::IndexType::eUint16 : vuk::IndexType::eUint32);
    }
    command_buffer.bind_vertex_buffer(0, vertex, 0, vuk::Packed{vuk::Format::eR32G32Sfloat, vuk::Format::eR32G32Sfloat, vuk::Format::eR8G8B8A8Unorm});
    command_buffer.bind_graphics_pipeline("imgui");
    command_buffer.set_viewport(0, vuk::Rect2D::framebuffer());
    struct PC {
      float scale[2];
      float translate[2];
    } pc;
    pc.scale[0] = 2.0f / draw_data->DisplaySize.x;
    pc.scale[1] = 2.0f / draw_data->DisplaySize.y;
    pc.translate[0] = -1.0f - draw_data->DisplayPos.x * pc.scale[0];
    pc.translate[1] = -1.0f - draw_data->DisplayPos.y * pc.scale[1];
    command_buffer.push_constants(vuk::ShaderStageFlagBits::eVertex, 0, pc);
  };

  const size_t vertex_size = draw_data->TotalVtxCount * sizeof(ImDrawVert);
  const size_t index_size = draw_data->TotalIdxCount * sizeof(ImDrawIdx);
  auto imvert = *allocate_buffer(allocator, {vuk::MemoryUsage::eCPUtoGPU, vertex_size, 1});
  auto imind = *allocate_buffer(allocator, {vuk::MemoryUsage::eCPUtoGPU, index_size, 1});

  size_t vtx_dst = 0, idx_dst = 0;
  for (int n = 0; n < draw_data->CmdListsCount; n++) {
    const ImDrawList* cmd_list = draw_data->CmdLists[n];
    const auto imverto = imvert->add_offset(vtx_dst * sizeof(ImDrawVert));
    const auto imindo = imind->add_offset(idx_dst * sizeof(ImDrawIdx));

    // TODO:
    vuk::host_data_to_buffer(allocator, vuk::DomainFlagBits{}, imverto, std::span(cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size))
      .wait(allocator, compiler);
    vuk::host_data_to_buffer(allocator, vuk::DomainFlagBits{}, imindo, std::span(cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size))
      .wait(allocator, compiler);
    vtx_dst += cmd_list->VtxBuffer.Size;
    idx_dst += cmd_list->IdxBuffer.Size;
  }

  const auto sampled_images_array = vuk::declare_array("imgui_sampled", std::span(sampled_attachments));

  return vuk::make_pass("imgui",
                        [this, verts = imvert.get(), inds = imind.get(), reset_render_state](vuk::CommandBuffer& command_buffer,
                                                                                             VUK_IA(vuk::eColorWrite) color_rt,
                                                                                             VUK_ARG(vuk::ImageAttachment[],
                                                                                                     vuk::Access::eFragmentSampled) sis) {
    command_buffer.set_dynamic_state(vuk::DynamicStateFlagBits::eViewport | vuk::DynamicStateFlagBits::eScissor)
      .set_rasterization(vuk::PipelineRasterizationStateCreateInfo{})
      .set_color_blend(color_rt, vuk::BlendPreset::eAlphaBlend);

    reset_render_state(imgui_data, command_buffer, verts, inds);
    // Will project scissor/clipping rectangles into framebuffer space
    const ImVec2 clip_off = draw_data->DisplayPos;         // (0,0) unless using multi-viewports
    const ImVec2 clip_scale = draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

    // Render command lists
    // (Because we merged all buffers into a single one, we maintain our own offset into them)
    int global_vtx_offset = 0;
    int global_idx_offset = 0;
    for (int n = 0; n < draw_data->CmdListsCount; n++) {
      const ImDrawList* cmd_list = draw_data->CmdLists[n];
      for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
        const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
        if (pcmd->UserCallback != nullptr) {
          // User callback, registered via ImDrawList::AddCallback()
          // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
          if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
            reset_render_state(imgui_data, command_buffer, verts, inds);
          else
            pcmd->UserCallback(cmd_list, pcmd);
        } else {
          // Project scissor/clipping rectangles into framebuffer space
          ImVec4 clip_rect;
          clip_rect.x = (pcmd->ClipRect.x - clip_off.x) * clip_scale.x;
          clip_rect.y = (pcmd->ClipRect.y - clip_off.y) * clip_scale.y;
          clip_rect.z = (pcmd->ClipRect.z - clip_off.x) * clip_scale.x;
          clip_rect.w = (pcmd->ClipRect.w - clip_off.y) * clip_scale.y;

          const auto fb_width = command_buffer.get_ongoing_render_pass().extent.width;
          const auto fb_height = command_buffer.get_ongoing_render_pass().extent.height;
          if (clip_rect.x < fb_width && clip_rect.y < fb_height && clip_rect.z >= 0.0f && clip_rect.w >= 0.0f) {
            // Negative offsets are illegal for vkCmdSetScissor
            if (clip_rect.x < 0.0f)
              clip_rect.x = 0.0f;
            if (clip_rect.y < 0.0f)
              clip_rect.y = 0.0f;

            // Apply scissor/clipping rectangle
            vuk::Rect2D scissor;
            scissor.offset.x = (int32_t)(clip_rect.x);
            scissor.offset.y = (int32_t)(clip_rect.y);
            scissor.extent.width = (uint32_t)(clip_rect.z - clip_rect.x);
            scissor.extent.height = (uint32_t)(clip_rect.w - clip_rect.y);
            command_buffer.set_scissor(0, scissor);

            // Bind texture
            if (pcmd->TextureId) {
              const auto& img = *reinterpret_cast<ImGuiLayer::ImGuiImage*>(pcmd->TextureId);
              if (img.global)
                command_buffer.bind_image(0, 0, img.view)
                  .bind_sampler(0, 0, img.linear_sampling ? vuk::LinearSamplerRepeated : vuk::NearestSamplerRepeated);
              else
                command_buffer.bind_image(0, 0, sis[img.attachment_index])
                  .bind_sampler(0, 0, img.linear_sampling ? vuk::LinearSamplerRepeated : vuk::NearestSamplerRepeated);
            }

            command_buffer.draw_indexed(pcmd->ElemCount, 1, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset, 0);
          }
        }
      }
      global_idx_offset += cmd_list->IdxBuffer.Size;
      global_vtx_offset += cmd_list->VtxBuffer.Size;
    }
    return color_rt;
  })(target, sampled_images_array);
}

ImGuiLayer::ImGuiImage* ImGuiLayer::add_image(const vuk::ImageView& view, bool linear_sampling) {
  return &*sampled_images.emplace(true, view, 0, linear_sampling);
}

ImGuiLayer::ImGuiImage* ImGuiLayer::add_attachment(const vuk::Value<vuk::ImageAttachment>& attach, bool linear_sampling) {
  sampled_attachments.emplace_back(attach);
  return &*sampled_images.emplace(false, vuk::ImageView{}, (uint32_t)sampled_attachments.size() - 1u, linear_sampling);
}

void ImGuiLayer::end() {
  OX_SCOPED_ZONE;
  ImGui::Render();

  draw_data = ImGui::GetDrawData();

  const ImGuiIO& io = ImGui::GetIO();
  if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    GLFWwindow* backup_current_context = glfwGetCurrentContext();
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
    glfwMakeContextCurrent(backup_current_context);
  }
}

void ImGuiLayer::apply_theme(bool dark) {
  ImVec4* colors = ImGui::GetStyle().Colors;

  if (dark) {
    colors[ImGuiCol_Text] = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.42f, 0.42f, 0.42f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_Border] = ImVec4(0.178f, 0.178f, 0.178f, 1.000f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.178f, 0.178f, 0.178f, 1.000f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.34f, 0.34f, 0.34f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.43f, 0.43f, 0.43f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(1.00f, 0.56f, 0.00f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(1.00f, 0.56f, 0.00f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(1.00f, 0.56f, 0.00f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(1.00f, 0.56f, 0.00f, 0.82f);
    colors[ImGuiCol_Header] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.19f, 0.19f, 0.19f, 1.00f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.19f, 0.19f, 0.19f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_TabSelectedOverline] = ImVec4(0.34f, 0.34f, 0.34f, 1.00f);
    colors[ImGuiCol_DockingPreview] = ImVec4(1.00f, 0.56f, 0.00f, 0.22f);
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotLines] = ImVec4(1.00f, 0.56f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.56f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(1.00f, 0.56f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.56f, 0.00f, 1.00f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_TableRowBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.85f, 0.48f, 0.00f, 0.73f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 0.56f, 0.00f, 1.00f);
    colors[ImGuiCol_NavHighlight] = ImVec4(1.00f, 0.56f, 0.00f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);

    header_selected_color = ImVec4(1.00f, 0.56f, 0.00f, 0.50f);
    header_hovered_color = lighten(colors[ImGuiCol_HeaderActive], 0.1f);
    window_bg_color = colors[ImGuiCol_WindowBg];
    window_bg_alternative_color = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
    asset_icon_color = lighten(header_selected_color, 0.9f);
    text_color = colors[ImGuiCol_Text];
    text_disabled_color = colors[ImGuiCol_TextDisabled];
  } else {
    colors[ImGuiCol_Text] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.98f);
    colors[ImGuiCol_Border] = ImVec4(0.40f, 0.40f, 0.40f, 0.30f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.40f, 0.40f, 0.40f, 0.30f);
    colors[ImGuiCol_FrameBg] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.70f, 0.82f, 0.95f, 0.39f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.86f, 0.86f, 0.86f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.69f, 0.69f, 0.69f, 0.80f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.49f, 0.49f, 0.49f, 0.80f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.49f, 0.49f, 0.49f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.41f, 0.67f, 0.98f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.70f, 0.82f, 0.95f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.91f, 0.91f, 0.91f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.81f, 0.81f, 0.81f, 0.80f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.89f, 0.89f, 0.89f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.81f, 0.81f, 0.81f, 0.62f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.56f, 0.56f, 0.56f, 0.78f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.78f, 0.78f, 0.78f, 1.00f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.65f, 0.65f, 0.65f, 1.00f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.81f, 0.81f, 0.81f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.91f, 0.91f, 0.91f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.98f, 0.98f, 0.98f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.91f, 0.91f, 0.91f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
    colors[ImGuiCol_DockingPreview] = ImVec4(0.26f, 0.59f, 0.98f, 0.22f);
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
    colors[ImGuiCol_PlotLines] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.78f, 0.87f, 0.98f, 1.00f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.57f, 0.57f, 0.64f, 1.00f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.68f, 0.68f, 0.74f, 1.00f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.30f, 0.30f, 0.30f, 0.09f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);

    header_selected_color = ImVec4(0.26f, 0.59f, 0.98f, 0.65f);
    header_hovered_color = darken(colors[ImGuiCol_HeaderActive], 0.1f);
    window_bg_color = colors[ImGuiCol_WindowBg];
    window_bg_alternative_color = darken(window_bg_color, 0.04f);
    asset_icon_color = darken(header_selected_color, 0.9f);
    text_color = colors[ImGuiCol_Text];
    text_disabled_color = colors[ImGuiCol_TextDisabled];
  }
}

void ImGuiLayer::set_style() {
  {
    auto& style = ImGuizmo::GetStyle();
    style.TranslationLineThickness *= 1.3f;
    style.TranslationLineArrowSize *= 1.3f;
    style.RotationLineThickness *= 1.3f;
    style.RotationOuterLineThickness *= 1.3f;
    style.ScaleLineThickness *= 1.3f;
    style.ScaleLineCircleSize *= 1.3f;
    style.HatchedAxisLineThickness *= 1.3f;
    style.CenterCircleSize *= 1.3f;

    ImGuizmo::SetGizmoSizeClipSpace(0.2f);
  }

  {
    ImGuiStyle* style = &ImGui::GetStyle();

    style->AntiAliasedFill = true;
    style->AntiAliasedLines = true;
    style->AntiAliasedLinesUseTex = true;

    style->WindowPadding = ImVec2(4.0f, 4.0f);
    style->FramePadding = ImVec2(4.0f, 4.0f);
    style->TabMinWidthForCloseButton = 0.1f;
    style->CellPadding = ImVec2(8.0f, 4.0f);
    style->ItemSpacing = ImVec2(8.0f, 3.0f);
    style->ItemInnerSpacing = ImVec2(2.0f, 4.0f);
    style->TouchExtraPadding = ImVec2(0.0f, 0.0f);
    style->IndentSpacing = 12;
    style->ScrollbarSize = 14;
    style->GrabMinSize = 10;

    style->WindowBorderSize = 0.0f;
    style->ChildBorderSize = 0.0f;
    style->PopupBorderSize = 1.5f;
    style->FrameBorderSize = 0.0f;
    style->TabBorderSize = 1.0f;
    style->DockingSeparatorSize = 3.0f;

    style->WindowRounding = 6.0f;
    style->ChildRounding = 0.0f;
    style->FrameRounding = 2.0f;
    style->PopupRounding = 2.0f;
    style->ScrollbarRounding = 3.0f;
    style->GrabRounding = 2.0f;
    style->LogSliderDeadzone = 4.0f;
    style->TabRounding = 3.0f;

    style->WindowTitleAlign = ImVec2(0.0f, 0.5f);
    style->WindowMenuButtonPosition = ImGuiDir_None;
    style->ColorButtonPosition = ImGuiDir_Left;
    style->ButtonTextAlign = ImVec2(0.5f, 0.5f);
    style->SelectableTextAlign = ImVec2(0.0f, 0.0f);
    style->DisplaySafeAreaPadding = ImVec2(8.0f, 8.0f);

    ui_frame_padding = ImVec2(4.0f, 2.0f);
    popup_item_spacing = ImVec2(6.0f, 8.0f);

    constexpr ImGuiColorEditFlags color_edit_flags = ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf |
                                                     ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_InputRGB |
                                                     ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_Uint8;
    ImGui::SetColorEditOptions(color_edit_flags);

    style->ScaleAllSizes(1.0f);
  }
}
} // namespace ox
