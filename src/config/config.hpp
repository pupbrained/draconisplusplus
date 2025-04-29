#pragma once

#ifdef _WIN32
  #include <windows.h> // GetUserNameA
#else
  #include <pwd.h>    // getpwuid, passwd
  #include <unistd.h> // getuid
#endif

#include <stdexcept>                 // std::runtime_error
#include <string>                    // std::string (String)
#include <toml++/impl/node.hpp>      // toml::node
#include <toml++/impl/node_view.hpp> // toml::node_view
#include <toml++/impl/table.hpp>     // toml::table
#include <variant>                   // std::variant

#include "src/core/util/defs.hpp"
#include "src/core/util/error.hpp"
#include "src/core/util/logging.hpp"
#include "src/core/util/types.hpp"

#include "weather.hpp"

using util::error::DraconisError;
using util::types::String, util::types::Array, util::types::Option, util::types::Result;

/// Alias for the location type used in Weather config, can be a city name (String) or coordinates (Coords).
using Location = std::variant<String, weather::Coords>;

/**
 * @struct General
 * @brief Holds general configuration settings.
 */
struct General {
  String name; ///< Default display name, retrieved from the system.

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
    // Try to get the username using GetUserNameA
    Array<char, 256> username;
    DWORD            size = sizeof(username);
    return GetUserNameA(username.data(), &size) ? username.data() : "User";
#else
    // Try to get the username using getpwuid
    if (const passwd* pwd = getpwuid(getuid()))
      return pwd->pw_name;

    // Try to get the username using environment variables
    if (Result<String, DraconisError> envUser = util::helpers::GetEnv("USER"))
      return *envUser;

    // Finally, try to get the username using LOGNAME
    if (Result<String, DraconisError> envLogname = util::helpers::GetEnv("LOGNAME"))
      return *envLogname;

    // If all else fails, return a default name
    return "User";
#endif
  }

  /**
   * @brief Parses a TOML table to create a General instance.
   * @param tbl The TOML table to parse, containing [general].
   * @return A General instance with the parsed values, or defaults otherwise.
   */
  static fn fromToml(const toml::table& tbl) -> General {
    const toml::node_view<const toml::node> nameNode = tbl["name"];
    return { .name = nameNode ? *nameNode.value<String>() : getDefaultName() };
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
  static fn fromToml(const toml::table& tbl) -> NowPlaying { return { .enabled = tbl["enabled"].value_or(false) }; }
};

/**
 * @struct Weather
 * @brief Holds configuration settings for the Weather feature.
 */
struct Weather {
  Location location; ///< Location for weather data, can be a city name or coordinates.
  String   api_key;  ///< API key for the weather service.
  String   units;    ///< Units for temperature, either "metric" or "imperial".

  bool enabled        = false; ///< Flag to enable or disable the Weather feature.
  bool show_town_name = false; ///< Flag to show the town name in the output.

  /**
   * @brief Parses a TOML table to create a Weather instance.
   * @param tbl The TOML table to parse, containing [weather].
   * @return A Weather instance with the parsed values, or defaults otherwise.
   */
  static fn fromToml(const toml::table& tbl) -> Weather {
    Weather weather;

    const Option<String> apiKey = tbl["api_key"].value<String>();

    weather.enabled = tbl["enabled"].value_or<bool>(false) && apiKey;

    if (!weather.enabled)
      return weather;

    weather.api_key        = *apiKey;
    weather.show_town_name = tbl["show_town_name"].value_or(false);
    weather.units          = tbl["units"].value_or("metric");

    if (const toml::node_view<const toml::node> location = tbl["location"]) {
      if (location.is_string())
        weather.location = *location.value<String>();
      else if (location.is_table())
        weather.location = weather::Coords {
          .lat = *location.as_table()->get("lat")->value<double>(),
          .lon = *location.as_table()->get("lon")->value<double>(),
        };
      else {
        error_log("Invalid location format in config.");
        weather.enabled = false;
      }
    }

    return weather;
  }

  /**
   * @brief Retrieves the weather information based on the configuration.
   * @return The weather information as a WeatherOutput object.
   *
   * This function fetches the weather data based on the configured location,
   * API key, and units. It returns a WeatherOutput object containing the
   * retrieved weather data.
   */
  [[nodiscard]] fn getWeatherInfo() const -> weather::Output;
};

/**
 * @struct Config
 * @brief Holds the application configuration settings.
 */
struct Config {
  General    general;     ///< General configuration settings.
  Weather    weather;     ///< Weather configuration settings.`
  NowPlaying now_playing; ///< Now Playing configuration settings.

  Config() = default;

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
