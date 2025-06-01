#pragma once

#if DRAC_ENABLE_PACKAGECOUNT
  #include "Util/Definitions.hpp"
#endif

#include "Util/Types.hpp"

namespace config {
  using util::types::String, util::types::u8;

#if DRAC_ENABLE_WEATHER
  enum class WeatherProvider : u8 {
    OPENWEATHERMAP,
    OPENMETEO,
    METNO,
  };

  enum class WeatherUnit : u8 {
    METRIC,
    IMPERIAL,
  };
#endif

#if DRAC_ENABLE_PACKAGECOUNT
  enum class PackageManager : u8 {
    NONE  = 0,
    CARGO = 1 << 0,

  #if defined(__linux__) || defined(__APPLE__)
    NIX = 1 << 1,
  #endif

  #ifdef __linux__
    APK    = 1 << 2,
    DPKG   = 1 << 3,
    MOSS   = 1 << 4,
    PACMAN = 1 << 5,
    RPM    = 1 << 6,
    XBPS   = 1 << 7,
  #elifdef __APPLE
    HOMEBREW = 1 << 2,
    MACPORTS = 1 << 3,
  #elifdef _WIN32
    WINGET     = 1 << 1,
    CHOCOLATEY = 1 << 2,
    SCOOP      = 1 << 3,
  #elif defined(__FreeBSD__) || defined(__DragonFly__)
    PKGNG = 1 << 1,
  #elifdef __NetBSD__
    PKGSRC = 1 << 1,
  #elifdef __HAIKU__
    HAIKUPKG = 1 << 1,
  #endif
  };

  constexpr fn operator|(PackageManager pmA, PackageManager pmB)->PackageManager {
    return static_cast<PackageManager>(static_cast<unsigned int>(pmA) | static_cast<unsigned int>(pmB));
  }

  constexpr fn HasPackageManager(PackageManager current_flags, PackageManager flag_to_check) -> bool {
    return (static_cast<unsigned int>(current_flags) & static_cast<unsigned int>(flag_to_check)) != 0;
  }
#endif
} // namespace config
