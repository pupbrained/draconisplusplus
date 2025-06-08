/**
 * @file config.example.hpp
 * @brief Example configuration file for the application.
 *
 * @details This file serves as a template for `config.hpp`.
 * Users should copy this file to `config.hpp` and customize the
 * settings according to their preferences.
 *
 * To enable these precompiled settings, `PRECOMPILED_CONFIG` must be defined
 * in your build system or `meson.options`.
 */
#pragma once

#ifdef PRECOMPILED_CONFIG

  #if DRAC_ENABLE_WEATHER || DRAC_ENABLE_PACKAGECOUNT
    #include "Config/Config.hpp" // Location

    #include "Services/Weather.hpp" // Coords

    #include "Util/ConfigData.hpp" // PackageManager, WeatherProvider, WeatherUnit
  #endif

namespace config {
  /**
   * @brief The username to display.
   * @details Used for the greeting message.
   */
  constexpr const char* DRAC_USERNAME = "User";

  #if DRAC_ENABLE_WEATHER
  /**
   * @brief Selects the weather service provider.
   *
   * @details
   * - `WeatherProvider::OPENWEATHERMAP`: Uses OpenWeatherMap API (requires `DRAC_API_KEY`).
   * - `WeatherProvider::OPENMETEO`:      Uses OpenMeteo API (no API key needed).
   * - `WeatherProvider::METNO`:          Uses Met.no API (no API key needed).
   *
   * @see DRAC_API_KEY
   * @see WeatherProvider
   */
  constexpr WeatherProvider DRAC_WEATHER_PROVIDER = WeatherProvider::OPENMETEO;

  /**
   * @brief Specifies the unit system for displaying weather information.
   *
   * @details
   * - `WeatherUnit::IMPERIAL`: Uses imperial units (e.g., Fahrenheit, mph).
   * - `WeatherUnit::METRIC`:   Uses metric units (e.g., Celsius, kph).
   *
   * @see WeatherUnit
   */
  constexpr WeatherUnit DRAC_WEATHER_UNIT = WeatherUnit::METRIC;

  /**
   * @brief Determines whether to display the town name in the weather output.
   *
   * @note If set to `true`, the weather condition/description might be hidden
   *       to save space, depending on the UI implementation.
   *
   * @details
   * - `true`:  Show the town name.
   * - `false`: Do not show the town name (default, may show condition instead).
   */
  constexpr bool DRAC_SHOW_TOWN_NAME = false;

  /**
   * @brief API key for the OpenWeatherMap service.
   *
   * @details
   * - This key is **only** required if `DRAC_WEATHER_PROVIDER` is set to `WeatherProvider::OPENWEATHERMAP`.
   * - Met.no and OpenMeteo providers do not require an API key; for these, this value can be `std::nullopt`.
   * - Obtain an API key from [OpenWeatherMap](https://openweathermap.org/api).
   *
   * @see DRAC_WEATHER_PROVIDER
   */
  constexpr std::optional<std::string> DRAC_API_KEY = std::nullopt;

  /**
   * @brief Specifies the location for weather forecasts.
   *
   * For `WeatherProvider::OPENWEATHERMAP`, this can be a city name (e.g., `"London,UK"`) or
   * `weather::Coords` for latitude/longitude.
   *
   * For `WeatherProvider::OPENMETEO` and `WeatherProvider::METNO`, this **must** be
   * `weather::Coords` (latitude and longitude).
   *
   * For New York City using coordinates:
   * @code{.cpp}
   * constexpr Location DRAC_LOCATION = weather::Coords { .lat = 40.730610, .lon = -73.935242 };
   * @endcode
   *
   * For New York City using a city name (OpenWeatherMap only):
   * @code{.cpp}
   * constexpr Location DRAC_LOCATION = "New York,US";
   * @endcode
   *
   * @see Location
   * @see weather::Coords
   * @see DRAC_WEATHER_PROVIDER
   *

   */
  constexpr Location DRAC_LOCATION = weather::Coords { .lat = 40.730610, .lon = -73.935242 };
  #endif

  #if DRAC_ENABLE_PACKAGECOUNT
  /**
   * @brief Configures which package managers' counts are displayed.
   *
   * This is a bitmask field. Combine multiple `PackageManager` enum values
   * using the bitwise OR operator (`|`).
   * The available `PackageManager` enum values are defined in `Util/ConfigData.hpp`
   * and may vary based on the operating system.
   *
   * @see PackageManager
   * @see HasPackageManager
   * @see Util/ConfigData.hpp
   *
   * To enable CARGO, PACMAN, and NIX package managers:
   * @code{.cpp}
   * constexpr PackageManager DRAC_ENABLED_PACKAGE_MANAGERS = PackageManager::CARGO | PackageManager::PACMAN | PackageManager::NIX;
   * @endcode
   *
   * To enable only CARGO:
   * @code{.cpp}
   * constexpr PackageManager DRAC_ENABLED_PACKAGE_MANAGERS = PackageManager::CARGO;
   * @endcode
   */
  constexpr PackageManager DRAC_ENABLED_PACKAGE_MANAGERS = PackageManager::CARGO;
  #endif
} // namespace config

#endif // PRECOMPILED_CONFIG
