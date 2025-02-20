#include <cstdlib>
#include <filesystem>
#include <fmt/core.h>
#include <stdexcept>

#include "config.h"

using rfl::Result;
namespace fs = std::filesystem;

namespace {
  inline fn GetConfigPath() -> fs::path {
#ifdef _WIN32
    const char* localAppData = std::getenv("LOCALAPPDATA");

    if (!localAppData)
      throw std::runtime_error("Environment variable LOCALAPPDATA is not set");

    return fs::path(localAppData);
#else
    const char* home = std::getenv("HOME");

    if (!home)
      throw std::runtime_error("Environment variable HOME is not set");

    return fs::path(home) / ".config";
#endif
  }
}

fn Config::getInstance() -> Config {
  fs::path configPath = GetConfigPath();
  configPath /= "draconis++/config.toml";

  const Result<Config> result = rfl::toml::load<Config>(configPath.string());

  if (!result) {
    ERROR_LOG("Failed to load config file: {}", result.error().what());

    exit(1);
  }

  return result.value();
}
