#include <cstdlib>
#include <filesystem>
#include <fmt/core.h>
#include <stdexcept>

#include "config.h"

#include "src/util/macros.h"

namespace fs = std::filesystem;

namespace {
  fn GetConfigPath() -> fs::path {
#ifdef _WIN32
    char*  rawPtr     = nullptr;
    size_t bufferSize = 0;
    if (_dupenv_s(&rawPtr, &bufferSize, "LOCALAPPDATA") != 0 || !rawPtr)
      throw std::runtime_error("LOCALAPPDATA env var not found");
    std::unique_ptr<char, decltype(&free)> localAppData(rawPtr, free);
    return fs::path(localAppData.get()) / "draconis++" / "config.toml";
#else
    const char* home = std::getenv("HOME");
    if (!home)
      throw std::runtime_error("HOME env var not found");
    return fs::path(home) / ".config" / "draconis++" / "config.toml";
#endif
  }
}

fn Config::getInstance() -> Config {
  try {
    const fs::path configPath = GetConfigPath();
    if (!fs::exists(configPath)) {
      DEBUG_LOG("Config file not found, using defaults");
      return Config {};
    }

    auto config = toml::parse_file(configPath.string());
    return Config::fromToml(config);
  } catch (const std::exception& e) {
    DEBUG_LOG("Config loading failed: {}, using defaults", e.what());
    return Config {};
  }
}
