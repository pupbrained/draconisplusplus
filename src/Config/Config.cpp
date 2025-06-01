#include "Config.hpp"

#include <format>                    // std::{format, format_error}
#include <toml++/impl/node_view.hpp> // toml::node_view
#include <toml++/impl/parser.hpp>    // toml::{parse_file, parse_result}
#include <toml++/impl/table.hpp>     // toml::table
#include <utility>

#include "Util/Definitions.hpp"
#include "Util/Logging.hpp"

#ifndef PRECOMPILED_CONFIG
  #include <filesystem>   // std::filesystem::{path, operator/, exists, create_directories}
  #include <fstream>      // std::{ifstream, ofstream, operator<<}
  #include <system_error> // std::error_code

  #include "Util/Env.hpp"
  #include "Util/Types.hpp"

namespace fs = std::filesystem;
#else
  #include "config.hpp" // user-defined config
#endif

#ifndef PRECOMPILED_CONFIG
namespace {
  using util::types::Vec, util::types::CStr, util::types::Exception;
  constexpr const char* defaultConfigTemplate = R"toml(# Draconis++ Configuration File

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
  )toml";

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

      if (std::error_code errc; !fs::exists(defaultDir, errc) || errc) {
        create_directories(defaultDir, errc);
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
#endif

Config::Config([[maybe_unused]] const toml::table& tbl) {
#ifndef PRECOMPILED_CONFIG
  const toml::node_view genTbl = tbl["general"];
  const toml::node_view npTbl  = tbl["now_playing"];
  const toml::node_view wthTbl = tbl["weather"];

  this->general    = genTbl.is_table() ? General::fromToml(*genTbl.as_table()) : General {};
  this->nowPlaying = npTbl.is_table() ? NowPlaying::fromToml(*npTbl.as_table()) : NowPlaying {};
  this->weather    = wthTbl.is_table() ? Weather::fromToml(*wthTbl.as_table()) : Weather {};
#else
  std::unreachable();
#endif // PRECOMPILED_CONFIG
}

fn Config::getInstance() -> Config {
#ifdef PRECOMPILED_CONFIG
  Config cfg; // Default construct. Members are default-initialized.

  cfg.general.name = config::DRAC_USERNAME;

  // Weather section
  #if DRAC_ENABLE_WEATHER
  cfg.weather.enabled      = true;
  cfg.weather.apiKey       = config::DRAC_API_KEY; // const char* to String conversion
  cfg.weather.showTownName = config::DRAC_SHOW_TOWN_NAME;

  if constexpr (config::DRAC_WEATHER_UNIT == config::WeatherUnit::IMPERIAL) {
    cfg.weather.units = "imperial";
  } else { // METRIC
    cfg.weather.units = "metric";
  }

  cfg.weather.location = config::DRAC_LOCATION; // config::DRAC_LOCATION is already a Location variant

  // Initialize weather service based on DRAC_WEATHER_PROVIDER
  if constexpr (config::DRAC_WEATHER_PROVIDER == config::WeatherProvider::OPENWEATHERMAP) {
    cfg.weather.service = std::make_unique<weather::OpenWeatherMapService>(
      config::DRAC_LOCATION, // OpenWeatherMapService constructor takes the variant directly
      config::DRAC_API_KEY,
      cfg.weather.units
    );
  } else if constexpr (config::DRAC_WEATHER_PROVIDER == config::WeatherProvider::OPENMETEO) {
    if (std::holds_alternative<weather::Coords>(config::DRAC_LOCATION)) {
      const auto& coords  = std::get<weather::Coords>(config::DRAC_LOCATION);
      cfg.weather.service = std::make_unique<weather::OpenMeteoService>(
        coords.lat,
        coords.lon,
        cfg.weather.units
      );
    } else {
      error_log("Precompiled OpenMeteo requires coordinates, but DRAC_LOCATION is not Coords.");
      cfg.weather.enabled = false;
    }
  } else if constexpr (config::DRAC_WEATHER_PROVIDER == config::WeatherProvider::METNO) {
    if (std::holds_alternative<weather::Coords>(config::DRAC_LOCATION)) {
      const auto& coords  = std::get<weather::Coords>(config::DRAC_LOCATION);
      cfg.weather.service = std::make_unique<weather::MetNoService>(
        coords.lat,
        coords.lon,
        cfg.weather.units
      );
    } else {
      error_log("Precompiled MetNo requires coordinates, but DRAC_LOCATION is not Coords.");
      cfg.weather.enabled = false;
    }
  } else {
    error_log("Unknown precompiled weather provider specified in DRAC_WEATHER_PROVIDER.");
    cfg.weather.enabled = false;
  }

  if (cfg.weather.enabled && !cfg.weather.service) {
    error_log("Failed to initialize precompiled weather service for the configured provider.");
    cfg.weather.enabled = false;
  }
  #else  // DRAC_ENABLE_WEATHER is not defined
  cfg.weather.enabled = false;
  cfg.weather.service = nullptr;
  #endif // DRAC_ENABLE_WEATHER

  // NowPlaying section
  #if DRAC_ENABLE_NOWPLAYING
  cfg.nowPlaying.enabled = true;
  debug_log("Precompiled: NowPlaying is ENABLED via DRAC_ENABLE_NOWPLAYING.");
  #else
  cfg.nowPlaying.enabled = false;
  debug_log("Precompiled: NowPlaying is DISABLED via DRAC_ENABLE_NOWPLAYING.");
  #endif // DRAC_ENABLE_NOWPLAYING

  debug_log("Using precompiled configuration.");
  return cfg;

#else  // PRECOMPILED_CONFIG is not defined
  // Existing code to load from toml::parse_file
  try {
    const fs::path configPath = GetConfigPath();

    std::error_code errc;

    const bool exists = fs::exists(configPath, errc);

    if (!exists) {
      info_log("Config file not found at {}, creating defaults.", configPath.string());

      if (!CreateDefaultConfig(configPath)) {
        return {}; // Return default-constructed Config
      }
    }

    const toml::table parsed_config = toml::parse_file(configPath.string());

    debug_log("Config loaded from {}", configPath.string());

    return Config(parsed_config); // Use the TOML-parsing constructor
  } catch (const Exception& e) {
    debug_log("Config loading failed: {}, using defaults", e.what());
    return {}; // Return default-constructed Config
  } catch (...) {
    error_log("An unexpected error occurred during config loading. Using in-memory defaults.");
    return {}; // Return default-constructed Config
  }
#endif // PRECOMPILED_CONFIG
}
