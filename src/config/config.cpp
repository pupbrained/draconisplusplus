#include <cstdlib>
#include <filesystem>
#include <stdexcept>

#include "config.h"

#include "src/util/macros.h"

namespace fs = std::filesystem;

namespace {
  fn GetConfigPath() -> fs::path {
    Vec<fs::path> possiblePaths;

#ifdef _WIN32
    if (auto result = GetEnv("LOCALAPPDATA"))
      possiblePaths.push_back(fs::path(*result) / "draconis++" / "config.toml");

    if (auto result = GetEnv("USERPROFILE")) {
      possiblePaths.push_back(fs::path(*result) / ".config" / "draconis++" / "config.toml");
      possiblePaths.push_back(fs::path(*result) / "AppData" / "Local" / "draconis++" / "config.toml");
    }

    if (auto result = GetEnv("APPDATA"))
      possiblePaths.push_back(fs::path(*result) / "draconis++" / "config.toml");

    possiblePaths.push_back(fs::path(".") / "config.toml");
#else
    if (Result<String, EnvError> result = GetEnv("XDG_CONFIG_HOME"))
      possiblePaths.emplace_back(fs::path(*result) / "draconis++" / "config.toml");

    if (Result<String, EnvError> result = GetEnv("HOME")) {
      possiblePaths.emplace_back(fs::path(*result) / ".config" / "draconis++" / "config.toml");
      possiblePaths.emplace_back(fs::path(*result) / ".draconis++" / "config.toml");
    }

    possiblePaths.emplace_back("/etc/draconis++/config.toml");
#endif

    for (const fs::path& path : possiblePaths)
      if (std::error_code errc; exists(path, errc) && !errc)
        return path;

    if (!possiblePaths.empty()) {
      const fs::path defaultDir = possiblePaths[0].parent_path();

      if (std::error_code errc; !exists(defaultDir, errc) && !errc) {
        create_directories(defaultDir, errc);
        if (errc)
          WARN_LOG("Warning: Failed to create config directory: {}", errc.message());
      }

      return possiblePaths[0];
    }

    throw std::runtime_error("Could not determine a valid config path");
  }

  fn CreateDefaultConfig(const fs::path& configPath) -> bool {
    try {
      std::error_code errc;
      create_directories(configPath.parent_path(), errc);
      if (errc) {
        ERROR_LOG("Failed to create config directory: {}", errc.message());
        return false;
      }

      toml::table root;

#ifdef _WIN32
      Array<char, 256> username;

      DWORD size = sizeof(username);

      String defaultName = GetUserNameA(username.data(), &size) ? username.data() : "User";
#else
      const struct passwd* pwd     = getpwuid(getuid());
      CStr                 pwdName = pwd ? pwd->pw_name : nullptr;

      const Result<String, EnvError> envUser    = GetEnv("USER");
      const Result<String, EnvError> envLogname = GetEnv("LOGNAME");

      String defaultName = (pwdName) ? pwdName : (envUser) ? *envUser : (envLogname) ? *envLogname : "User";
#endif

      toml::table* general = root.insert("general", toml::table {}).first->second.as_table();
      general->insert("name", defaultName);

      toml::table* nowPlaying = root.insert("now_playing", toml::table {}).first->second.as_table();
      nowPlaying->insert("enabled", false);

      toml::table* weather = root.insert("weather", toml::table {}).first->second.as_table();
      weather->insert("enabled", false);
      weather->insert("show_town_name", false);
      weather->insert("api_key", "");
      weather->insert("units", "metric");
      weather->insert("location", "London");

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

    if (!exists(configPath)) {
      INFO_LOG("Config file not found, creating defaults at {}", configPath.string());

      if (!CreateDefaultConfig(configPath)) {
        WARN_LOG("Failed to create default config, using in-memory defaults");
        return {};
      }
    }

    const toml::parse_result config = toml::parse_file(configPath.string());

    DEBUG_LOG("Config loaded from {}", configPath.string());
    return fromToml(config);
  } catch (const std::exception& e) {
    DEBUG_LOG("Config loading failed: {}, using defaults", e.what());
    return {};
  }
}
