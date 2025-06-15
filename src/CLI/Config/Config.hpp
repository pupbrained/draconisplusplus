#pragma once

#ifdef _WIN32
  #include <windows.h> // GetUserNameA
#else
  #include <pwd.h>    // getpwuid, passwd
  #include <unistd.h> // getuid

  #include "DracUtils/Env.hpp"
#endif

#if !DRAC_PRECOMPILED_CONFIG
  #include <memory>                    // std::{make_unique, unique_ptr}
  #include <toml++/impl/node.hpp>      // toml::node
  #include <toml++/impl/node_view.hpp> // toml::node_view
  #include <toml++/impl/table.hpp>     // toml::table

  #include "DracUtils/Logging.hpp"
#endif // DRAC_PRECOMPILED_CONFIG

#if DRAC_ENABLE_WEATHER
  #include "Drac++/Services/Weather.hpp"
#endif // DRAC_ENABLE_WEATHER

#if DRAC_ENABLE_PACKAGECOUNT
  #include "Drac++/Services/PackageCounting.hpp"
#endif

#include "DracUtils/Definitions.hpp"
#include "DracUtils/Types.hpp"

/**
 * @struct General
 * @brief Holds general configuration settings.
 */
struct General {
  util::types::SZString name = getDefaultName(); ///< Default display name, retrieved from the system.

  /**
   * @brief Retrieves the default name for the user.
   * @return The default name for the user, either from the system or a fallback.
   *
   * Retrieves the default name for the user based on the operating system.
   * On Windows, it uses GetUserNameA to get the username.
   * On POSIX systems, it first tries to get the username using getpwuid,
   * then checks the USER and LOGNAME environment variables.
   */
  static fn getDefaultName() -> util::types::SZString {
#ifdef _WIN32
    using util::types::Array;

    Array<char, 256> username;

    DWORD size = username.size();

    return GetUserNameA(username.data(), &size) ? username.data() : "User";
#else
    using util::helpers::GetEnv;
    using util::types::CStr, util::types::SZString, util::types::Result;

    const passwd*        pwd        = getpwuid(getuid());
    CStr                 pwdName    = pwd ? pwd->pw_name : nullptr;
    const Result<SZString> envUser    = GetEnv("USER");
    const Result<SZString> envLogname = GetEnv("LOGNAME");

    return pwdName ? pwdName
      : envUser    ? *envUser
      : envLogname ? *envLogname
                   : "User";
#endif
  }

#if !DRAC_PRECOMPILED_CONFIG
  /**
   * @brief Parses a TOML table to create a General instance.
   * @param tbl The TOML table to parse, containing [general].
   * @return A General instance with the parsed values, or defaults otherwise.
   */
  static fn fromToml(const toml::table& tbl) -> General {
    using util::types::SZString;

    General gen;

    if (const toml::node_view<const toml::node> nameNode = tbl["name"])
      if (auto nameVal = nameNode.value<SZString>())
        gen.name = *nameVal;

    return gen;
  }
#endif // DRAC_PRECOMPILED_CONFIG
};

#if DRAC_ENABLE_NOWPLAYING
/**
 * @struct NowPlaying
 * @brief Holds configuration settings for the Now Playing feature.
 */
struct NowPlaying {
  bool enabled = false; ///< Flag to enable or disable the Now Playing feature.

  #if !DRAC_PRECOMPILED_CONFIG
  /**
   * @brief Parses a TOML table to create a NowPlaying instance.
   * @param tbl The TOML table to parse, containing [now_playing].
   * @return A NowPlaying instance with the parsed values, or defaults otherwise.
   */
  static fn fromToml(const toml::table& tbl) -> NowPlaying {
    return { .enabled = tbl["enabled"].value_or(false) };
  }
  #endif // DRAC_PRECOMPILED_CONFIG
};
#endif // DRAC_ENABLE_NOWPLAYING

#if DRAC_ENABLE_WEATHER
/**
 * @struct Weather
 * @brief Holds configuration settings for the Weather feature.
 */
struct Weather {
  weather::Location                        location; ///< Location for weather data, can be a city name or coordinates.
  util::types::Option<util::types::SZString> apiKey;   ///< API key for the weather service.
  weather::Unit                            units;    ///< Units for temperature, either "metric" or "imperial".

  bool                                      enabled      = false;   ///< Flag to enable or disable the Weather feature.
  bool                                      showTownName = false;   ///< Flag to show the town name in the output.
  std::unique_ptr<weather::IWeatherService> service      = nullptr; ///< Pointer to the weather service.

  #if !DRAC_PRECOMPILED_CONFIG
  /**
   * @brief Parses a TOML table to create a Weather instance.
   * @param tbl The TOML table to parse, containing [weather].
   * @return A Weather instance with the parsed values, or defaults otherwise.
   */
  static fn fromToml(const toml::table& tbl) -> Weather {
    using util::types::SZString;

    using matchit::match, matchit::is, matchit::_;

    #define SET_ERROR(...)       \
      do {                       \
        error_log(__VA_ARGS__);  \
        weather.enabled = false; \
      } while (false)

    Weather weather;

    weather.apiKey  = tbl["api_key"].value<SZString>();
    weather.enabled = tbl["enabled"].value_or<bool>(false);

    if (!weather.enabled)
      return weather;

    weather.showTownName = tbl["show_town_name"].value_or(false);
    SZString unitsStr      = tbl["units"].value_or("metric");

    match(unitsStr)(
      is | "metric"   = [&]() { weather.units = weather::Unit::METRIC; },
      is | "imperial" = [&]() { weather.units = weather::Unit::IMPERIAL; },
      is | _          = [&]() { SET_ERROR("Invalid units: '{}'. Accepted values are 'metric' and 'imperial'.", unitsStr); }
    );

    SZString provider = tbl["provider"].value_or("openweathermap");

    if (const toml::node_view<const toml::node> locationNode = tbl["location"]) {
      using matchit::app;

      // clang-format off
      match(locationNode)(
        is | app([](const toml::node_view<const toml::node>& node) { return node.is_string(); }, true) = [&]() { weather.location = *locationNode.value<SZString>(); },
        is | app([](const toml::node_view<const toml::node>& node) { return node.is_table(); }, true)  = [&]() {
          weather.location = weather::Coords {
            .lat = *locationNode.as_table()->get("lat")->value<double>(),
            .lon = *locationNode.as_table()->get("lon")->value<double>(),
          };
        },
        is | _ = [&]() { SET_ERROR("Invalid location format in config. Accepted values are a string (only if using OpenWeatherMap) or a table with 'lat' and 'lon' keys."); }
      );
      // clang-format on
    } else
      SET_ERROR("No location provided in config. Accepted values are a string (only if using OpenWeatherMap) or a table with 'lat' and 'lon' keys.");

    if (weather.enabled) {
      // clang-format off
      match(provider)(
        is | "openmeteo" = [&]() {
          if (std::holds_alternative<weather::Coords>(weather.location)) {
            const auto& coords = std::get<weather::Coords>(weather.location);
            weather.service = CreateWeatherService(weather::Provider::OPENMETEO, coords, weather.units);
          } else
            SET_ERROR("OpenMeteo requires coordinates (lat, lon) for location.");
        },
        is | "metno" = [&]() {
          if (std::holds_alternative<weather::Coords>(weather.location)) {
            const auto& coords = std::get<weather::Coords>(weather.location);
            weather.service = CreateWeatherService(weather::Provider::METNO, coords, weather.units);
          } else
            SET_ERROR("MetNo requires coordinates (lat, lon) for location.");
        },
        is | "openweathermap" = [&]() {
          if (weather.apiKey)
            weather.service = CreateWeatherService(weather::Provider::OPENWEATHERMAP, weather.location, *weather.apiKey, weather.units);
          else
            SET_ERROR("OpenWeatherMap requires an API key.");
        },
        is | _ = [&]() { SET_ERROR("Unknown weather provider: '{}'. Accepted values are 'openmeteo', 'metno', and 'openweathermap'.", provider); }
      );
      // clang-format on
    }

    return weather;
  }
  #endif // DRAC_PRECOMPILED_CONFIG
};
#endif // DRAC_ENABLE_WEATHER

/**
 * @struct Config
 * @brief Holds the application configuration settings.
 */
struct Config {
  General general; ///< General configuration settings.
#if DRAC_ENABLE_WEATHER
  Weather weather; ///< Weather configuration settings.
#endif
#if DRAC_ENABLE_NOWPLAYING
  NowPlaying nowPlaying; ///< Now Playing configuration settings.
#endif
#if DRAC_ENABLE_PACKAGECOUNT
  package::Manager enabledPackageManagers; ///< Enabled package managers.
#endif

  /**
   * @brief Default constructor for Config.
   */
  Config() = default;

#if !DRAC_PRECOMPILED_CONFIG
  /**
   * @brief Constructs a Config instance from a TOML table.
   * @param tbl The TOML table to parse, containing [general], [weather], and [now_playing].
   */
  explicit Config(const toml::table& tbl);
#endif // !DRAC_PRECOMPILED_CONFIG

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