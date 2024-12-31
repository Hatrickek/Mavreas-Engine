#include "FileDialogs.hpp"

#include "Profiler.hpp"

#include "Render/Window.hpp"

#include "Core/PlatformDetection.hpp"

namespace ox {
void FileDialogs::init() {
  OX_SCOPED_ZONE;
  NFD_Init();
}

void FileDialogs::deinit() {
  NFD_Quit();
}

std::string FileDialogs::open_file(const std::vector<nfdfilteritem_t>& filter) {
  nfdchar_t* out_path;
  const nfdresult_t result = NFD_OpenDialog(&out_path, filter.data(), (uint32_t)filter.size(), nullptr);
  if (result == NFD_OKAY) {
    return out_path;
  }
  return {};
}

std::string FileDialogs::save_file(const std::vector<nfdfilteritem_t>& filter, const char* defaultName) {
  nfdchar_t* out_path;
  const nfdresult_t result = NFD_SaveDialog(&out_path, filter.data(), (uint32_t)filter.size(), nullptr, defaultName);
  if (result == NFD_OKAY) {
    return out_path;
  }
  return {};
}

std::string FileDialogs::open_dir() {
  nfdchar_t* out_path;
  const nfdchar_t* default_dir = App::get()->get_specification().working_directory.c_str();
  const nfdresult_t result = NFD_PickFolder(&out_path, default_dir);
  if (result == NFD_OKAY) {
    return out_path;
  }
  return {};
}
}
