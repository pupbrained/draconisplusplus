#include "config.hpp"

#include <filesystem>                // std::filesystem::{path, operator/, exists, create_directories}
#include <format>                    // std::{format, format_error}
#include <fstream>                   // std::{ifstream, ofstream, operator<<}
#include <system_error>              // std::error_code
#include <toml++/impl/node_view.hpp> // toml::node_view
#include <toml++/impl/parser.hpp>    // toml::{parse_file, parse_result}
#include <toml++/impl/table.hpp>     // toml::table

#ifndef _WIN32
  #include <pwd.h>    // passwd, getpwuid
  #include <unistd.h> // getuid
#endif

#include "src/util/defs.hpp"
#include "src/util/helpers.hpp"
#include "src/util/logging.hpp"
#include "src/util/types.hpp"

namespace fs = std::filesystem;

namespace {
  using util::types::Vec, util::types::CStr, util::types::Exception;
  constexpr const char* defaultConfigTemplate = R"cfg(# Draconis++ Configuration File

# General settings
[general]
name = "{}" # Your display name

# Now Playing integration
[now_playing]
enabled = false # Set to true to enable media integration

# Weather settings
[weather]
enabled = false        # Set to true to enable weather display
show_town_name = false # Show location name in weather display
api_key = ""           # Your weather API key
units = "metric"       # Use "metric" for °C or "imperial" for °F
location = "London"    # Your city name

# Alternatively, you can specify coordinates instead of a city name:
# [weather.location]
# lat = 51.5074
# lon = -0.1278
  )cfg";

  fn GetConfigPath() -> fs::path {
    using util::helpers::GetEnv;

    Vec<fs::path> possiblePaths;

#ifdef _WIN32
    if (Result<String> result = GetEnv("LOCALAPPDATA"))
      possiblePaths.emplace_back(fs::path(*result) / "draconis++" / "config.toml");

    if (Result<String> result = GetEnv("USERPROFILE")) {
      possiblePaths.emplace_back(fs::path(*result) / ".config" / "draconis++" / "config.toml");
      possiblePaths.emplace_back(fs::path(*result) / "AppData" / "Local" / "draconis++" / "config.toml");
    }

    if (Result<String> result = GetEnv("APPDATA"))
      possiblePaths.emplace_back(fs::path(*result) / "draconis++" / "config.toml");
#else
    if (Result<String> result = GetEnv("XDG_CONFIG_HOME"))
      possiblePaths.emplace_back(fs::path(*result) / "draconis++" / "config.toml");

    if (Result<String> result = GetEnv("HOME")) {
      possiblePaths.emplace_back(fs::path(*result) / ".config" / "draconis++" / "config.toml");
      possiblePaths.emplace_back(fs::path(*result) / ".draconis++" / "config.toml");
    }
#endif

    possiblePaths.emplace_back(fs::path(".") / "config.toml");

    for (const fs::path& path : possiblePaths)
      if (std::error_code errc; fs::exists(path, errc) && !errc)
        return path;

    if (!possiblePaths.empty()) {
      const fs::path defaultDir = possiblePaths[0].parent_path();

      if (std::error_code errc; !fs::exists(defaultDir, errc) || !errc) {
        create_directories(defaultDir, errc);
        if (errc)
          warn_log("Warning: Failed to create config directory: {}", errc.message());
      }

      return possiblePaths[0];
    }

    warn_log("Could not determine a preferred config path. Falling back to './config.toml'");
    return fs::path(".") / "config.toml";
  }

  fn CreateDefaultConfig(const fs::path& configPath) -> bool {
    try {
      std::error_code errc;
      create_directories(configPath.parent_path(), errc);

      if (errc) {
        error_log("Failed to create config directory: {}", errc.message());
        return false;
      }

      std::ofstream file(configPath);
      if (!file) {
        error_log("Failed to open config file for writing: {}", configPath.string());
        return false;
      }

      try {
        const String defaultName = General::getDefaultName();

        const String formattedConfig = std::vformat(defaultConfigTemplate, std::make_format_args(defaultName));

        file << formattedConfig;
      } catch (const std::format_error& fmtErr) {
        error_log("Failed to format default config string: {}", fmtErr.what());
        return false;
      }

      if (!file) {
        error_log("Failed to write to config file: {}", configPath.string());
        return false;
      }

      info_log("Created default config file at {}", configPath.string());
      return true;
    } catch (const fs::filesystem_error& fsErr) {
      error_log("Filesystem error during default config creation: {}", fsErr.what());
      return false;
    } catch (const Exception& e) {
      error_log("Failed to create default config file: {}", e.what());
      return false;
    } catch (...) {
      error_log("An unexpected error occurred during default config creation.");
      return false;
    }
  }
} // namespace

Config::Config(const toml::table& tbl) {
  const toml::node_view genTbl = tbl["general"];
  const toml::node_view npTbl  = tbl["now_playing"];
  const toml::node_view wthTbl = tbl["weather"];

  this->general    = genTbl.is_table() ? General::fromToml(*genTbl.as_table()) : General {};
  this->nowPlaying = npTbl.is_table() ? NowPlaying::fromToml(*npTbl.as_table()) : NowPlaying {};
  this->weather    = wthTbl.is_table() ? Weather::fromToml(*wthTbl.as_table()) : Weather {};
}

fn Config::getInstance() -> Config {
  try {
    const fs::path configPath = GetConfigPath();

    std::error_code errc;

    const bool exists = fs::exists(configPath, errc);

    if (errc)
      warn_log("Failed to check if config file exists at {}: {}. Assuming it doesn't.", configPath.string(), errc.message());

    if (!exists) {
      info_log("Config file not found at {}, creating defaults.", configPath.string());

      if (!CreateDefaultConfig(configPath)) {
        warn_log("Failed to create default config file at {}. Using in-memory defaults.", configPath.string());
        return {};
      }
    }

    const toml::table config = toml::parse_file(configPath.string());

    debug_log("Config loaded from {}", configPath.string());

    return Config(config);
  } catch (const Exception& e) {
    debug_log("Config loading failed: {}, using defaults", e.what());

    return {};
  } catch (...) {
    error_log("An unexpected error occurred during config loading. Using in-memory defaults.");

    return {};
  }
}
