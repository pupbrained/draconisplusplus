#include <cstdlib>
#include <filesystem>
#include <fmt/core.h>
#include <stdexcept>

#include "config.h"

using rfl::Result;
namespace fs = std::filesystem;

namespace {
  fn GetConfigPath() -> fs::path {
#ifdef _WIN32
    char*  rawPtr     = nullptr;
    size_t bufferSize = 0;

    if (_dupenv_s(&rawPtr, &bufferSize, "LOCALAPPDATA") != 0 || !rawPtr)
      throw std::runtime_error("Environment variable LOCALAPPDATA is not set or could not be accessed");

    // Use unique_ptr with custom deleter to handle memory automatically
    const std::unique_ptr<char, decltype(&free)> localAppData(rawPtr, free);
    fs::path                                     path(localAppData.get());
    return path;
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

  // purely visual but whatever
#ifdef _WIN32
  configPath /= "draconis++\\config.toml";
#else
  configPath /= "draconis++/config.toml";
#endif

  const Result<Config> result = rfl::toml::load<Config>(configPath.string());

  if (!result) {
    DEBUG_LOG("Failed to load config file: {}", result.error().what());

    // Use default values
    return {};
  }

  return result.value();
}
