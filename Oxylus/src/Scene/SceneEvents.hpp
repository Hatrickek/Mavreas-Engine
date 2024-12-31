#pragma once
#include "Assets/AssetManager.hpp"

namespace ox {
struct FutureMeshLoadEvent {
  std::string name = {};
  AssetTask<Mesh>* task = nullptr;
};
}
