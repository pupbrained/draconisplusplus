#include "Config.hpp"

#include <Drac++/Utils/Logging.hpp>

#if !DRAC_PRECOMPILED_CONFIG
  #include <filesystem>                // std::filesystem::{path, operator/, exists, create_directories}
  #include <fstream>                   // std::{ifstream, ofstream, operator<<}
  #include <system_error>              // std::error_code
  #include <toml++/impl/node_view.hpp> // toml::node_view
  #include <toml++/impl/parser.hpp>    // toml::{parse_file, parse_result}
  #include <toml++/impl/table.hpp>     // toml::table

  #include <Drac++/Services/Packages.hpp>

  #include <Drac++/Utils/Env.hpp>
  #include <Drac++/Utils/Types.hpp>

namespace fs = std::filesystem;
#else
  #include <Drac++/Services/Weather.hpp>

  #include "../config.hpp" // user-defined config
#endif

#if !DRAC_PRECOMPILED_CONFIG
using namespace draconis::utils::types;
using draconis::utils::env::GetEnv;

namespace {
  fn GetConfigPath() -> fs::path {
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

      const String defaultName   = draconis::config::General::getDefaultName();
      String       configContent = std::format(R"toml(# Draconis++ Configuration File

# General settings
[general]
name = "{}" # Your display name
)toml",
                                         defaultName);

  #if DRAC_ENABLE_NOWPLAYING
      configContent += R"toml(
# Now Playing integration
[now_playing]
enabled = true # Set to true to enable media integration
)toml";
  #endif

  #if DRAC_ENABLE_WEATHER
      configContent += R"toml(
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
  #endif

  #if DRAC_ENABLE_PACKAGECOUNT
      configContent += R"toml(
# Package counting settings
[packages]
enabled = [] # List of package managers to count, e.g. ["cargo", "nix", "pacman"]

# Possible values depend on your OS: cargo, nix, apk, dpkg, moss, pacman, rpm, xbps, homebrew, macports, winget, chocolatey, scoop, pkgng, pkgsrc, haikupkg
# If you don't want to count any package managers, leave the list empty.
)toml";
  #endif

      std::ofstream file(configPath);
      file << configContent;

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

#endif // !DRAC_PRECOMPILED_CONFIG

namespace draconis::config {
  fn Config::getInstance() -> Config {
#if DRAC_PRECOMPILED_CONFIG
    using namespace draconis::config;

    Config cfg;
    cfg.general.name = DRAC_USERNAME;

    if constexpr (DRAC_ENABLE_WEATHER) {
      using namespace draconis::services::weather;
      using enum draconis::services::weather::Provider;

      cfg.weather.enabled      = true;
      cfg.weather.apiKey       = DRAC_API_KEY;
      cfg.weather.showTownName = DRAC_SHOW_TOWN_NAME;
      cfg.weather.units        = DRAC_WEATHER_UNIT;
      cfg.weather.location     = DRAC_LOCATION;

      if constexpr (DRAC_WEATHER_PROVIDER == OpenWeatherMap) {
        if (!cfg.weather.apiKey) {
          error_log("OpenWeatherMap requires an API key.");
          cfg.weather.enabled = false;
        }

        cfg.weather.service = CreateWeatherService(
          OpenWeatherMap,
          DRAC_LOCATION,
          cfg.weather.units,
          cfg.weather.apiKey
        );
      } else if constexpr (DRAC_WEATHER_PROVIDER == OpenMeteo) {
        if (std::holds_alternative<Coords>(DRAC_LOCATION)) {
          const auto& coords = std::get<Coords>(DRAC_LOCATION);

          cfg.weather.service = CreateWeatherService(
            OpenMeteo,
            coords,
            cfg.weather.units
          );
        } else {
          error_log("Precompiled OpenMeteo requires coordinates, but DRAC_LOCATION is not Coords.");
          cfg.weather.enabled = false;
        }
      } else if constexpr (DRAC_WEATHER_PROVIDER == MetNo) {
        if (std::holds_alternative<Coords>(DRAC_LOCATION)) {
          const auto& coords = std::get<Coords>(DRAC_LOCATION);

          cfg.weather.service = CreateWeatherService(
            MetNo,
            coords,
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
    }

    if constexpr (DRAC_ENABLE_PACKAGECOUNT)
      cfg.enabledPackageManagers = config::DRAC_ENABLED_PACKAGE_MANAGERS;

    if constexpr (DRAC_ENABLE_NOWPLAYING)
      cfg.nowPlaying.enabled = true;

    debug_log("Using precompiled configuration.");
    return cfg;
#else
    try {
      const fs::path configPath = GetConfigPath();

      std::error_code errc;

      const bool exists = fs::exists(configPath, errc);

      if (!exists) {
        info_log("Config file not found at {}, creating defaults.", configPath.string());

        if (!CreateDefaultConfig(configPath))
          return {};
      }

      const toml::table parsedConfig = toml::parse_file(configPath.string());

      debug_log("Config loaded from {}", configPath.string());

      return Config(parsedConfig);
    } catch (const Exception& e) {
      debug_log("Config loading failed: {}, using defaults", e.what());
      return {};
    } catch (...) {
      error_log("An unexpected error occurred during config loading. Using in-memory defaults.");
      return {};
    }
#endif // DRAC_PRECOMPILED_CONFIG
  }

#if !DRAC_PRECOMPILED_CONFIG
  Config::Config(const toml::table& tbl) {
    const toml::node_view genTbl = tbl["general"];
    this->general                = genTbl.is_table() ? General::fromToml(*genTbl.as_table()) : General {};

    if (!this->general.name)
      this->general.name = General::getDefaultName();

    if constexpr (DRAC_ENABLE_NOWPLAYING) {
      const toml::node_view npTbl = tbl["now_playing"];
      this->nowPlaying            = npTbl.is_table() ? NowPlaying::fromToml(*npTbl.as_table()) : NowPlaying {};
    }

    if constexpr (DRAC_ENABLE_WEATHER) {
      const toml::node_view wthTbl = tbl["weather"];
      this->weather                = wthTbl.is_table() ? Weather::fromToml(*wthTbl.as_table()) : Weather {};
    }

    if constexpr (DRAC_ENABLE_PACKAGECOUNT) {
      const toml::node_view pkgTbl = tbl["packages"];

      if (pkgTbl.is_table()) {
        const auto enabledNode = pkgTbl["enabled"];

        if (enabledNode.is_array()) {
          using enum draconis::services::packages::Manager;

          this->enabledPackageManagers = None;

          for (const auto& elem : *enabledNode.as_array()) {
            if (auto valOpt = elem.value<String>()) {
              String val = *valOpt;

              if (val == "cargo")
                this->enabledPackageManagers |= Cargo;
  #if defined(__linux__) || defined(__APPLE__)
              else if (val == "nix")
                this->enabledPackageManagers |= Nix;
  #endif
  #ifdef __linux__
              else if (val == "apk")
                this->enabledPackageManagers |= Apk;
              else if (val == "dpkg")
                this->enabledPackageManagers |= Dpkg;
              else if (val == "moss")
                this->enabledPackageManagers |= Moss;
              else if (val == "pacman")
                this->enabledPackageManagers |= Pacman;
              else if (val == "rpm")
                this->enabledPackageManagers |= Rpm;
              else if (val == "xbps")
                this->enabledPackageManagers |= Xbps;
  #endif
  #ifdef __APPLE__
              else if (val == "homebrew")
                this->enabledPackageManagers |= Homebrew;
              else if (val == "macports")
                this->enabledPackageManagers |= Macports;
  #endif
  #ifdef _WIN32
              else if (val == "winget")
                this->enabledPackageManagers |= Winget;
              else if (val == "chocolatey")
                this->enabledPackageManagers |= Chocolatey;
              else if (val == "scoop")
                this->enabledPackageManagers |= Scoop;
  #endif
  #if defined(__FreeBSD__) || defined(__DragonFly__)
              else if (val == "pkgng")
                this->enabledPackageManagers |= PkgNg;
  #endif
  #ifdef __NetBSD__
              else if (val == "pkgsrc")
                this->enabledPackageManagers |= PkgSrc;
  #endif
  #ifdef __HAIKU__
              else if (val == "haikupkg")
                this->enabledPackageManagers |= HaikuPkg;
  #endif
              else
                warn_log("Unknown package manager in config: {}", val);
            }
          }
        }
      }
    }
  }
#endif // !DRAC_PRECOMPILED_CONFIG
} // namespace draconis::config
