#include "VFS.hpp"
#include "Core/FileSystem.hpp"

namespace ox {
std::pair<std::string, std::string> split_path(std::string_view full_path) {
  const size_t found = full_path.find_first_of("/\\");
  return {
    std::string(full_path.substr(0, found + 1)), // virtual
    std::string(full_path.substr(found + 1)),    // rest
  };
}

void VFS::mount_dir(const std::string& virtual_dir, const std::string& physical_dir) { mapped_dirs.emplace(virtual_dir, physical_dir); }

void VFS::unmount_dir(const std::string& virtual_dir) { mapped_dirs.erase(virtual_dir); }

std::string VFS::resolve_physical_dir(const std::string& virtual_dir) {
  auto [virtual_part, rest] = split_path(virtual_dir);

  for (auto& [vd, pd] : mapped_dirs) {
    if (vd == virtual_part) {
      return fs::append_paths(pd, rest);
    }
  }

  return virtual_dir;
}
} // namespace ox
