#include <cstdlib>
#include <filesystem>
#include <fmt/core.h>
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
  try {
    fs::path configPath = GetConfigPath();
    configPath /= "draconis++/config.toml";

    const Result<Config> result = rfl::toml::load<Config>(configPath.string());

    if (result)
      return result.value();

    fmt::println("Failed to load config file: {}", result.error().value().what());
  } catch (const std::exception& e) { fmt::println("Error getting config path: {}", e.what()); }

  return {};
}
