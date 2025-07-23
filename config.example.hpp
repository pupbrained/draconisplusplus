/**
 * @file config.example.hpp
 * @brief Example configuration file for the application.
 *
 * @details This file serves as a template for `config.hpp`.
 * Users should copy this file to `config.hpp` and customize the
 * settings according to their preferences.
 *
 * To enable these precompiled settings, `DRAC_PRECOMPILED_CONFIG` must be defined
 * in your build system or `meson.options`.
 */
#pragma once

#if DRAC_PRECOMPILED_CONFIG

  #if DRAC_ENABLE_WEATHER
    #include <Drac++/Services/Weather.hpp>
  #endif

  #if DRAC_ENABLE_PACKAGECOUNT
    #include <Drac++/Services/Packages.hpp>
  #endif

namespace draconis::config {
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
   * - `draconis::services::weather::Provider::OpenWeatherMap`: Uses OpenWeatherMap API (requires `DRAC_API_KEY`).
   * - `draconis::services::weather::Provider::OpenMeteo`:      Uses OpenMeteo API (no API key needed).
   * - `draconis::services::weather::Provider::Metno`:          Uses Met.no API (no API key needed).
   *
   * @see DRAC_API_KEY
   * @see draconis::services::weather::Provider
   */
  constexpr services::weather::Provider DRAC_WEATHER_PROVIDER = services::weather::Provider::OpenMeteo;

  /**
   * @brief Specifies the unit system for displaying weather information.
   *
   * @details
   * - `draconis::services::weather::UnitSystem::Imperial`: Uses imperial units (e.g., Fahrenheit, mph).
   * - `draconis::services::weather::UnitSystem::Metric`:   Uses metric units (e.g., Celsius, kph).
   *
   * @see draconis::services::weather::UnitSystem
   */
  constexpr services::weather::UnitSystem DRAC_WEATHER_UNIT = services::weather::UnitSystem::Metric;

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
   * - This key is **only** required if `DRAC_WEATHER_PROVIDER` is set to `draconis::services::weather::Provider::OpenWeatherMap`.
   * - Met.no and OpenMeteo providers do not require an API key; for these, this value can be `std::nullopt`.
   * - Obtain an API key from [OpenWeatherMap](https://openweathermap.org/api).
   *
   * @see DRAC_WEATHER_PROVIDER
   */
  constexpr std::optional<std::string> DRAC_API_KEY = std::nullopt;

  /**
   * @brief Specifies the location for weather forecasts.
   *
   * For `draconis::services::weather::Provider::OpenWeatherMap`, this can be a city name (e.g., `"London,UK"`) or
   * `draconis::services::weather::Coords` for latitude/longitude.
   *
   * For `draconis::services::weather::Provider::OpenMeteo` and `draconis::services::weather::Provider::Metno`, this **must** be
   * `draconis::services::weather::Coords` (latitude and longitude).
   *
   * For New York City using coordinates:
   * @code{.cpp}
   * constexpr draconis::services::weather::Location DRAC_LOCATION = draconis::services::weather::Coords { .lat = 40.730610, .lon = -73.935242 };
   * @endcode
   *
   * For New York City using a city name (OpenWeatherMap only):
   * @code{.cpp}
   * constexpr draconis::services::weather::Location DRAC_LOCATION = "New York, US";
   * @endcode
   *
   * @see draconis::services::weather::Location
   * @see draconis::services::weather::Coords
   * @see DRAC_WEATHER_PROVIDER
   *
   */
  constexpr services::weather::Location DRAC_LOCATION = services::weather::Coords { .lat = 40.730610, .lon = -73.935242 };
  #endif

  #if DRAC_ENABLE_PACKAGECOUNT
  /**
   * @brief Configures which package managers' counts are displayed.
   *
   * This is a bitmask field. Combine multiple `Manager` enum values
   * using the bitwise OR operator (`|`).
   * The available `Manager` enum values are defined in `Util/ConfigData.hpp`
   * and may vary based on the operating system.
   *
   * @see Manager
   * @see HasPackageManager
   * @see Util/ConfigData.hpp
   *
   * To enable Cargo, Pacman, and Nix package managers:
   * @code{.cpp}
   * constexpr Manager DRAC_ENABLED_PACKAGE_MANAGERS = Manager::Cargo | Manager::Pacman | Manager::Nix;
   * @endcode
   *
   * To enable only Cargo:
   * @code{.cpp}
   * constexpr Manager DRAC_ENABLED_PACKAGE_MANAGERS = Manager::Cargo;
   * @endcode
   */
  constexpr services::packages::Manager DRAC_ENABLED_PACKAGE_MANAGERS = services::packages::Manager::Cargo;
  #endif
} // namespace draconis::config

#endif // DRAC_PRECOMPILED_CONFIG
