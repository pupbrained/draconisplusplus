/**
 * @file ConfigData.hpp
 * @brief Defines enums and helper functions for application configuration.
 *
 * @details This file contains enums like `WeatherProvider`, `WeatherUnit`, and `PackageManager`,
 * which are used throughout the application to manage configurable features.
 * It also provides helper functions for working with these enums, such as bitwise
 * operations for `PackageManager` flags.
 * The availability of certain enums and enum values can be conditional based on
 * preprocessor definitions like `DRAC_ENABLE_WEATHER`, `DRAC_ENABLE_PACKAGECOUNT`,
 * and OS-specific macros.
 */

#pragma once

#include <format>
#include <matchit.hpp>

#include "Util/Definitions.hpp"
#include "Util/Types.hpp"

namespace config {
  using util::types::String, util::types::u8;

#if DRAC_ENABLE_WEATHER
  /**
   * @brief Specifies the weather service provider.
   * @see config::DRAC_WEATHER_PROVIDER in `config.example.hpp` or `config.hpp`.
   */
  enum class WeatherProvider : u8 {
    OPENWEATHERMAP, ///< OpenWeatherMap API. Requires an API key. @see config::DRAC_API_KEY
    OPENMETEO,      ///< OpenMeteo API. Does not require an API key.
    METNO,          ///< Met.no API. Does not require an API key.
  };

  /**
   * @brief Specifies the unit system for weather information.
   * @see config::DRAC_WEATHER_UNIT in `config.example.hpp` or `config.hpp`.
   */
  enum class WeatherUnit : u8 {
    METRIC,   ///< Metric units (Celsius, kph, etc.).
    IMPERIAL, ///< Imperial units (Fahrenheit, mph, etc.).
  };
#endif

#if DRAC_ENABLE_PACKAGECOUNT
  /**
   * @brief Represents available package managers for package counting.
   *
   * @details This enum is used as a bitmask. Individual values can be combined
   * using the bitwise OR operator (`|`). The availability of specific package managers
   * is conditional on the operating system detected at compile time.
   *
   * @see config::DRAC_ENABLED_PACKAGE_MANAGERS in `config.example.hpp` or `config.hpp`.
   * @see config::operator|
   * @see config::HasPackageManager
   */
  enum class PackageManager : u8 {
    NONE  = 0,      ///< No package manager.
    CARGO = 1 << 0, ///< Cargo, the Rust package manager.

  #if defined(__linux__) || defined(__APPLE__)
    NIX = 1 << 1, ///< Nix package manager (available on Linux and macOS).
  #endif

  #ifdef __linux__
    APK    = 1 << 2, ///< apk, the Alpine Linux package manager.
    DPKG   = 1 << 3, ///< dpkg, the Debian package system (used by APT).
    MOSS   = 1 << 4, ///< Moss, the package manager for MOOS.
    PACMAN = 1 << 5, ///< Pacman, the Arch Linux package manager.
    RPM    = 1 << 6, ///< RPM, package manager used by Fedora, RHEL, etc.
    XBPS   = 1 << 7, ///< XBPS, the X Binary Package System (used by Void Linux).
  #elifdef __APPLE__
    HOMEBREW = 1 << 2, ///< Homebrew, package manager for macOS.
    MACPORTS = 1 << 3, ///< MacPorts, package manager for macOS.
  #elifdef _WIN32
    WINGET     = 1 << 1, ///< Winget, the Windows Package Manager.
    CHOCOLATEY = 1 << 2, ///< Chocolatey, package manager for Windows.
    SCOOP      = 1 << 3, ///< Scoop, command-line installer for Windows.
  #elif defined(__FreeBSD__) || defined(__DragonFly__)
    PKGNG = 1 << 1, ///< pkg, package management system for FreeBSD and DragonFly BSD.
  #elifdef __NetBSD__
    PKGSRC = 1 << 1, ///< pkgsrc, package management system for NetBSD.
  #elifdef __HAIKU__
    HAIKUPKG = 1 << 1, ///< haikupkg, package manager for Haiku OS.
  #endif
  };

  /**
   * @brief Combines two PackageManager flags using a bitwise OR operation.
   *
   * @param pmA The first PackageManager flag.
   * @param pmB The second PackageManager flag.
   * @return A new PackageManager value representing the combination of pmA and pmB.
   */
  constexpr fn operator|(PackageManager pmA, PackageManager pmB)->PackageManager {
    return static_cast<PackageManager>(static_cast<unsigned int>(pmA) | static_cast<unsigned int>(pmB));
  }
#endif
} // namespace config

#if DRAC_ENABLE_WEATHER
template <>
struct std::formatter<config::WeatherUnit> {
  static constexpr auto parse(std::format_parse_context& ctx) {
    return ctx.begin();
  }

  static fn format(config::WeatherUnit unit, std::format_context& ctx) {
    using matchit::match, matchit::is, matchit::_;

    return std::format_to(ctx.out(), "{}", match(unit)(is | config::WeatherUnit::METRIC = "metric", is | config::WeatherUnit::IMPERIAL = "imperial"));
  }
};
#endif
