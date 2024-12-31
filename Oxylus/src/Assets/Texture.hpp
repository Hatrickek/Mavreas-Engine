#pragma once
#include <source_location>
#include <string>
#include <vuk/ImageAttachment.hpp>

#include "Asset.hpp"

#include "Core/Base.hpp"

using Preset = vuk::ImageAttachment::Preset;

namespace ox {
struct TextureLoadInfo {
  std::string path = {};
  Preset preset = Preset::eMap2D;
  vuk::Extent3D extent = {};
  vuk::Format format = vuk::Format::eR8G8B8A8Unorm;
  void* data = nullptr;
  enum class MimeType { Generic, KTX } mime = MimeType::Generic;
};

class Texture : public Asset {
public:
  Texture() = default;
  explicit Texture(const TextureLoadInfo& info, std::source_location loc = std::source_location::current());
  ~Texture() = default;

  void create_texture(vuk::Extent3D extent,
                      vuk::Format format = vuk::Format::eR8G8B8A8Unorm,
                      vuk::ImageAttachment::Preset preset = vuk::ImageAttachment::Preset::eGeneric2D,
                      std::source_location loc = std::source_location::current());
  void create_texture(const vuk::ImageAttachment& image_attachment, std::source_location loc = std::source_location::current());
  void create_texture(vuk::Extent3D extent,
                      const void* data,
                      vuk::Format format = vuk::Format::eR8G8B8A8Unorm,
                      Preset preset = Preset::eMap2D,
                      std::source_location loc = std::source_location::current());
  void load(const TextureLoadInfo& load_info, std::source_location loc = std::source_location::current());
  vuk::ImageAttachment as_attachment() const { return _attachment; }

  const vuk::Unique<vuk::Image>& get_image() const { return _image; }
  const vuk::Unique<vuk::ImageView>& get_view() const { return _view; }
  const vuk::Extent3D& get_extent() const { return _attachment.extent; }

  void set_name(std::string_view name, const std::source_location& loc = std::source_location::current());

  explicit operator uint64_t() { return _view->id; }

  static uint32_t get_mip_count(vuk::Extent3D extent) {
    return (uint32_t)log2f((float)std::max(std::max(extent.width, extent.height), extent.depth)) + 1;
  }

  static void create_white_texture();
  static Shared<Texture> get_white_texture() { return _white_texture; }

  /// Loads the given file using stb. Returned data must be freed manually.
  static uint8_t* load_stb_image(const std::string& filename,
                                 uint32_t* width = nullptr,
                                 uint32_t* height = nullptr,
                                 uint32_t* bits = nullptr,
                                 bool srgb = true);
  static uint8_t* load_stb_image_from_memory(void* buffer,
                                             size_t len,
                                             uint32_t* width = nullptr,
                                             uint32_t* height = nullptr,
                                             uint32_t* bits = nullptr,
                                             bool flipY = false,
                                             bool srgb = true);

  static uint8_t* get_magenta_texture(uint32_t width, uint32_t height, uint32_t channels);

  static uint8_t* convert_to_four_channels(uint32_t width, uint32_t height, const uint8_t* three_channel_data);

private:
  vuk::ImageAttachment _attachment;
  vuk::Unique<vuk::Image> _image;
  vuk::Unique<vuk::ImageView> _view;

  static Shared<Texture> _white_texture;
};
} // namespace ox
