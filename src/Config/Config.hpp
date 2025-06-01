#pragma once

#include <memory>                    // std::{make_unique, unique_ptr}
#include <toml++/impl/node.hpp>      // toml::node
#include <toml++/impl/node_view.hpp> // toml::node_view
#include <toml++/impl/table.hpp>     // toml::table
#include <variant>                   // std::variant

#ifdef _WIN32
  #include <windows.h> // GetUserNameA
#else
  #include <pwd.h>    // getpwuid, passwd
  #include <unistd.h> // getuid

  #include "Util/Env.hpp"
#endif

#include "Services/Weather.hpp"
#include "Services/Weather/MetNoService.hpp"
#include "Services/Weather/OpenMeteoService.hpp"
#include "Services/Weather/OpenWeatherMapService.hpp"

#include "Util/Definitions.hpp"
#include "Util/Error.hpp"
#include "Util/Logging.hpp"
#include "Util/Types.hpp"

using util::error::DracError;
using util::types::CStr, util::types::String, util::types::StringView, util::types::Array, util::types::Option, util::types::Result;

using Location = std::variant<String, weather::Coords>;

/**
 * @struct General
 * @brief Holds general configuration settings.
 */
struct General {
  String name = getDefaultName(); ///< Default display name, retrieved from the system.

  /**
   * @brief Retrieves the default name for the user.
   * @return The default name for the user, either from the system or a fallback.
   *
   * Retrieves the default name for the user based on the operating system.
   * On Windows, it uses GetUserNameA to get the username.
   * On POSIX systems, it first tries to get the username using getpwuid,
   * then checks the USER and LOGNAME environment variables.
   */
  static fn getDefaultName() -> String {
#ifdef _WIN32
    Array<char, 256> username;

    DWORD size = username.size();

    return GetUserNameA(username.data(), &size) ? username.data() : "User";
#else
    using util::helpers::GetEnv;

    const passwd*        pwd        = getpwuid(getuid());
    CStr                 pwdName    = pwd ? pwd->pw_name : nullptr;
    const Result<String> envUser    = GetEnv("USER");
    const Result<String> envLogname = GetEnv("LOGNAME");

    return pwdName ? pwdName
      : envUser    ? *envUser
      : envLogname ? *envLogname
                   : "User";
#endif
  }

  /**
   * @brief Parses a TOML table to create a General instance.
   * @param tbl The TOML table to parse, containing [general].
   * @return A General instance with the parsed values, or defaults otherwise.
   */
  static fn fromToml(const toml::table& tbl) -> General {
    General gen;

    if (const toml::node_view<const toml::node> nameNode = tbl["name"])
      if (auto nameVal = nameNode.value<String>())
        gen.name = *nameVal;

    return gen;
  }
};

/**
 * @struct NowPlaying
 * @brief Holds configuration settings for the Now Playing feature.
 */
struct NowPlaying {
  bool enabled = false; ///< Flag to enable or disable the Now Playing feature.

  /**
   * @brief Parses a TOML table to create a NowPlaying instance.
   * @param tbl The TOML table to parse, containing [now_playing].
   * @return A NowPlaying instance with the parsed values, or defaults otherwise.
   */
  static fn fromToml(const toml::table& tbl) -> NowPlaying {
    return { .enabled = tbl["enabled"].value_or(false) };
  }
};

/**
 * @struct Weather
 * @brief Holds configuration settings for the Weather feature.
 */
struct Weather {
  Location location; ///< Location for weather data, can be a city name or coordinates.
  String   apiKey;   ///< API key for the weather service.
  String   units;    ///< Units for temperature, either "metric" or "imperial".

  bool                                      enabled      = false;   ///< Flag to enable or disable the Weather feature.
  bool                                      showTownName = false;   ///< Flag to show the town name in the output.
  std::unique_ptr<weather::IWeatherService> service      = nullptr; ///< Pointer to the weather service.

  /**
   * @brief Parses a TOML table to create a Weather instance.
   * @param tbl The TOML table to parse, containing [weather].
   * @return A Weather instance with the parsed values, or defaults otherwise.
   */
  static fn fromToml(const toml::table& tbl) -> Weather {
    Weather weather;

    const Option<String> apiKeyOpt = tbl["api_key"].value<String>();
    weather.enabled                = tbl["enabled"].value_or<bool>(false) && apiKeyOpt;

    if (!weather.enabled)
      return weather;

    weather.apiKey       = *apiKeyOpt;
    weather.showTownName = tbl["show_town_name"].value_or(false);
    weather.units        = tbl["units"].value_or("metric");

    String provider = tbl["provider"].value_or("openweathermap");

    if (const toml::node_view<const toml::node> locationNode = tbl["location"]) {
      if (locationNode.is_string())
        weather.location = *locationNode.value<String>();
      else if (locationNode.is_table())
        weather.location = weather::Coords {
          .lat = *locationNode.as_table()->get("lat")->value<double>(),
          .lon = *locationNode.as_table()->get("lon")->value<double>(),
        };
      else {
        error_log("Invalid location format in config.");
        weather.enabled = false;
      }
    } else {
      error_log("No location provided in config.");
      weather.enabled = false;
    }

    if (weather.enabled) {
      if (provider == "openmeteo") {
        if (std::holds_alternative<weather::Coords>(weather.location)) {
          const auto& coords = std::get<weather::Coords>(weather.location);
          weather.service    = std::make_unique<weather::OpenMeteoService>(coords.lat, coords.lon, weather.units);
        } else {
          error_log("OpenMeteo requires coordinates for location.");
          weather.enabled = false;
        }
      } else if (provider == "metno") {
        if (std::holds_alternative<weather::Coords>(weather.location)) {
          const auto& coords = std::get<weather::Coords>(weather.location);
          weather.service    = std::make_unique<weather::MetNoService>(coords.lat, coords.lon, weather.units);
        } else {
          error_log("MetNo requires coordinates for location.");
          weather.enabled = false;
        }
      } else if (provider == "openweathermap") {
        weather.service = std::make_unique<weather::OpenWeatherMapService>(weather.location, weather.apiKey, weather.units);
      } else {
        error_log("Unknown weather provider: {}", provider);
        weather.enabled = false;
      }
    }

    return weather;
  }
};

/**
 * @struct Config
 * @brief Holds the application configuration settings.
 */
struct Config {
  General    general;    ///< General configuration settings.
  Weather    weather;    ///< Weather configuration settings.
  NowPlaying nowPlaying; ///< Now Playing configuration settings.

  /**
   * @brief Default constructor for Config.
   */
  Config() = default;

  /**
   * @brief Constructs a Config instance from a TOML table.
   * @param tbl The TOML table to parse, containing [general], [weather], and [now_playing].
   */
  explicit Config(const toml::table& tbl);

  /**
   * @brief Retrieves the path to the configuration file.
   * @return The path to the configuration file.
   *
   * This function constructs the path to the configuration file based on
   * the operating system and user directory. It returns a std::filesystem::path
   * object representing the configuration file path.
   */
  static fn getInstance() -> Config;
};
