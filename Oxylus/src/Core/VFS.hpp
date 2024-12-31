#pragma once

#include <ankerl/unordered_dense.h>
#include "ESystem.hpp"

namespace ox {
class VFS : public ESystem {
public:
  void init() {}
  void deinit() {}

  void mount_dir(const std::string& virtual_dir, const std::string& physical_dir);
  void unmount_dir(const std::string& virtual_dir);

  std::string resolve_physical_dir(const std::string& virtual_dir);

private:
  ankerl::unordered_dense::map<std::string, std::string> mapped_dirs = {};
};
} // namespace ox
