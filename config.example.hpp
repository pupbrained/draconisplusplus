#pragma once

#ifdef PRECOMPILED_CONFIG

  #include "Config/Config.hpp" // Location

  #if DRAC_ENABLE_WEATHER || DRAC_ENABLE_PACKAGECOUNT
    #include "Util/ConfigData.hpp"
  #endif // DRAC_ENABLE_WEATHER || DRAC_ENABLE_PACKAGECOUNT

namespace config {
  /**
   * @brief The username to use in the output.
   */
  constexpr const char* DRAC_USERNAME = "Mars";

  #if DRAC_ENABLE_WEATHER
  /**
   * @brief The weather provider to use.
   *
   * @details
   * - WeatherProvider::OPENWEATHERMAP: Uses OpenWeatherMap API.
   * - WeatherProvider::OPENMETEO:      Uses OpenMeteo API.
   * - WeatherProvider::METNO:          Uses Met.no API.
   */
  constexpr WeatherProvider DRAC_WEATHER_PROVIDER = WeatherProvider::OPENMETEO;

  /**
   * @brief The weather unit to use.
   *
   * @details
   * - WeatherUnit::IMPERIAL: Uses imperial units.
   * - WeatherUnit::METRIC:   Uses metric units.
   */
  constexpr WeatherUnit DRAC_WEATHER_UNIT = WeatherUnit::METRIC;

  /**
   * @brief Whether to show the town name in the output.
   * @note If enabled, condition/description will not be shown.
   *
   * @details
   * - true:  Show the town name.
   * - false: Do not show the town name.
   */
  constexpr bool DRAC_SHOW_TOWN_NAME = false;

  /**
   * @brief The API key to use for the weather provider.
   *
   * @details
   * - Only used with OpenWeatherMap.
   */
  constexpr const char* DRAC_API_KEY = "";

  /**
   * @brief The location to use for the weather provider.
   *
   * @details
   * - Only OpenWeatherMap supports using a town name for the location.
   * - For other providers, use coordinates.
   */
  constexpr Location DRAC_LOCATION = "New York";
  #endif

  #if DRAC_ENABLE_PACKAGECOUNT
  /**
   * @brief The package managers that should be used for counting.
   */
  constexpr PackageManager DRAC_ENABLED_PACKAGE_MANAGERS = PackageManager::CARGO;
  #endif
} // namespace config

#endif // PRECOMPILED_CONFIG
