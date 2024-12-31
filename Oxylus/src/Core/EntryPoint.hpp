#pragma once

#include "Utils/Log.hpp"
#include "App.hpp"

int main(int argc, char** argv) {
  ox::Log::init(argc, argv);

  const auto app = ox::create_application({argc, argv});

  app->run();

  delete app;

  return 0;
}
