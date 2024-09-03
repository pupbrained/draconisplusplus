#include <cstdlib>
#include <filesystem>
#include <fmt/core.h>
#include <stdexcept>
#include <string>

#include "config.h"

using rfl::Result;
namespace fs = std::filesystem;

inline fn GetConfigPath() -> std::string {
#ifdef _WIN32
  const char* localAppData = std::getenv("LOCALAPPDATA");

  if (!localAppData)
    throw std::runtime_error("Environment variable LOCALAPPDATA is not set");

  return localAppData;
#else
  const char* home = std::getenv("HOME");

  if (!home)
    throw std::runtime_error("Environment variable HOME is not set");

  return std::string(home) + "/.config";
#endif
}

fn Config::getInstance() -> Config {
  fs::path configPath = GetConfigPath();
  configPath /= "draconis++/config.toml";

  const Result<Config> result = rfl::toml::load<Config>(configPath.string());

  if (!result) {
    fmt::println(stderr, "Failed to load config file: {}", result.error()->what());
    exit(1);
  }

  return result.value();
}
