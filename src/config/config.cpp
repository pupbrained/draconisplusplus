#include <cstdlib>
#include <filesystem>
#include <fmt/core.h>
#include <iostream>
#include <stdexcept>

#include "config.h"

#include "src/util/macros.h"

namespace fs = std::filesystem;

namespace {
  fn GetConfigPath() -> fs::path {
    std::vector<fs::path> possiblePaths;

#ifdef _WIN32
    // Windows possible paths in order of preference
    if (auto result = GetEnv("LOCALAPPDATA"); result)
      possiblePaths.push_back(fs::path(*result) / "draconis++" / "config.toml");

    if (auto result = GetEnv("USERPROFILE"); result) {
      // Support for .config style on Windows (some users prefer this)
      possiblePaths.push_back(fs::path(*result) / ".config" / "draconis++" / "config.toml");
      // Traditional Windows location alternative
      possiblePaths.push_back(fs::path(*result) / "AppData" / "Local" / "draconis++" / "config.toml");
    }

    if (auto result = GetEnv("APPDATA"); result)
      possiblePaths.push_back(fs::path(*result) / "draconis++" / "config.toml");

    // Portable app option - config in same directory as executable
    possiblePaths.push_back(fs::path(".") / "config.toml");
#else
    // Unix/Linux paths in order of preference
    if (auto result = GetEnv("XDG_CONFIG_HOME"); result)
      possiblePaths.emplace_back(fs::path(*result) / "draconis++" / "config.toml");

    if (auto result = GetEnv("HOME"); result) {
      possiblePaths.emplace_back(fs::path(*result) / ".config" / "draconis++" / "config.toml");
      possiblePaths.emplace_back(fs::path(*result) / ".draconis++" / "config.toml");
    }

    // System-wide config
    possiblePaths.emplace_back("/etc/draconis++/config.toml");
#endif

    // Check if any of these configs already exist
    for (const auto& path : possiblePaths)
      if (std::error_code errc; exists(path, errc) && !errc)
        return path;

    // If no config exists yet, return the default (first in priority)
    if (!possiblePaths.empty()) {
      // Create directory structure for the default path
      const fs::path defaultDir = possiblePaths[0].parent_path();

      if (std::error_code errc; !exists(defaultDir, errc) && !errc) {
        create_directories(defaultDir, errc);
        if (errc)
          WARN_LOG("Warning: Failed to create config directory: {}", errc.message());
      }

      return possiblePaths[0];
    }

    // Ultimate fallback if somehow we have no paths
    throw std::runtime_error("Could not determine a valid config path");
  }

  fn CreateDefaultConfig(const fs::path& configPath) -> bool {
    try {
      // Ensure the directory exists
      std::error_code errc;
      create_directories(configPath.parent_path(), errc);
      if (errc) {
        ERROR_LOG("Failed to create config directory: {}", errc.message());
        return false;
      }

      // Create a default TOML document
      toml::table root;

      // Get default username for General section
      std::string defaultName;
#ifdef _WIN32
      std::array<char, 256> username;
      DWORD                 size = sizeof(username);
      defaultName                = GetUserNameA(username.data(), &size) ? username.data() : "User";
#else
      if (struct passwd* pwd = getpwuid(getuid()); pwd)
        defaultName = pwd->pw_name;
      else if (const char* envUser = getenv("USER"))
        defaultName = envUser;
      else
        defaultName = "User";
#endif

      // General section
      toml::table* general = root.insert("general", toml::table {}).first->second.as_table();
      general->insert("name", defaultName);

      // Now Playing section
      toml::table* nowPlaying = root.insert("now_playing", toml::table {}).first->second.as_table();
      nowPlaying->insert("enabled", false);

      // Weather section
      toml::table* weather = root.insert("weather", toml::table {}).first->second.as_table();
      weather->insert("enabled", false);
      weather->insert("show_town_name", false);
      weather->insert("api_key", "");
      weather->insert("units", "metric");
      weather->insert("location", "London");

      // Write to file (using a stringstream for comments + TOML)
      std::ofstream file(configPath);
      if (!file) {
        ERROR_LOG("Failed to open config file for writing: {}", configPath.string());
        return false;
      }

      file << "# Draconis++ Configuration File\n\n";

      file << "# General settings\n";
      file << "[general]\n";
      file << "name = \"" << defaultName << "\"  # Your display name\n\n";

      file << "# Now Playing integration\n";
      file << "[now_playing]\n";
      file << "enabled = false  # Set to true to enable media integration\n\n";

      file << "# Weather settings\n";
      file << "[weather]\n";
      file << "enabled = false        # Set to true to enable weather display\n";
      file << "show_town_name = false # Show location name in weather display\n";
      file << "api_key = \"\"         # Your weather API key\n";
      file << "units = \"metric\"     # Use \"metric\" for °C or \"imperial\" for °F\n";
      file << "location = \"London\"  # Your city name\n\n";

      file << "# Alternatively, you can specify coordinates instead of a city name:\n";
      file << "# [weather.location]\n";
      file << "# lat = 51.5074\n";
      file << "# lon = -0.1278\n";

      INFO_LOG("Created default config file at {}", configPath.string());
      return true;
    } catch (const std::exception& e) {
      ERROR_LOG("Failed to create default config file: {}", e.what());
      return false;
    }
  }
}

fn Config::getInstance() -> Config {
  try {
    const fs::path configPath = GetConfigPath();

    // Check if the config file exists
    if (!exists(configPath)) {
      INFO_LOG("Config file not found, creating defaults at {}", configPath.string());

      // Create default config
      if (!CreateDefaultConfig(configPath)) {
        WARN_LOG("Failed to create default config, using in-memory defaults");
        return {};
      }
    }

    // Now we should have a config file to read
    const toml::parse_result config = toml::parse_file(configPath.string());
    return fromToml(config);
  } catch (const std::exception& e) {
    DEBUG_LOG("Config loading failed: {}, using defaults", e.what());
    return {};
  }
}
