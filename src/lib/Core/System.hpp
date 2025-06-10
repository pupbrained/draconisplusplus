#pragma once

#include <format> // std::{formatter, format_to}

#if DRAC_ENABLE_WEATHER
  #include "Services/Weather.hpp"
#endif

#include "Util/Definitions.hpp"
#include "Util/Error.hpp"
#include "Util/Types.hpp"

struct Config;

using util::types::u64, util::types::f64, util::types::String, util::types::Option, util::types::Result,
  util::types::MediaInfo, util::types::ResourceUsage;

/**
 * @struct BytesToGiB
 * @brief Helper struct to format a byte value to GiB (Gibibytes).
 *
 * Encapsulates a byte value and provides a custom formatter
 * to convert it to GiB for display purposes.
 */
struct BytesToGiB {
  u64 value; ///< The byte value to be converted.

  /**
   * @brief Constructor for BytesToGiB.
   * @param value The byte value to be converted.
   */
  explicit constexpr BytesToGiB(const u64 value) : value(value) {}
};

/// @brief Conversion factor from bytes to GiB
constexpr u64 GIB = 1'073'741'824;

/**
 * @brief Custom formatter for BytesToGiB.
 *
 * Allows formatting BytesToGiB values using std::format.
 * Outputs the value in GiB with two decimal places.
 *
 * @code{.cpp}
 * #include <format>
 * #include "system_data.h"
 *
 * int main() {
 *   BytesToGiB data_size{2'147'483'648}; // 2 GiB
 *   std::string formatted = std::format("Size: {}", data_size);
 *   std::println("{}", formatted); // formatted will be "Size: 2.00GiB"
 *   return 0;
 * }
 * @endcode
 */
template <>
struct std::formatter<BytesToGiB> : std::formatter<double> {
  /**
   * @brief Formats the BytesToGiB value.
   * @param BTG The BytesToGiB instance to format.
   * @param ctx The formatting context.
   * @return An iterator to the end of the formatted output.
   */
  fn format(const BytesToGiB& BTG, auto& ctx) const {
    return std::format_to(ctx.out(), "{:.2f}GiB", static_cast<f64>(BTG.value) / GIB);
  }
};

namespace os {
  /**
   * @class System
   * @brief Holds various pieces of system information collected from the OS,
   *        and provides methods to fetch this information.
   *
   * This class aggregates information about the system,
   * in order to display it at all at once during runtime.
   * The actual implementation for each fetch function is platform-specific.
   */
  class System {
   public:
    Result<String>        date;          ///< Current date (e.g., "April 26th").
    Result<String>        host;          ///< Host/product family (e.g., "MacBook Air").
    Result<String>        kernelVersion; ///< OS kernel version (e.g., "6.14.4").
    Result<String>        osVersion;     ///< OS pretty name (e.g., "Ubuntu 24.04.2 LTS").
    Result<ResourceUsage> memInfo;       ///< Total physical RAM in bytes.
    Result<String>        desktopEnv;    ///< Desktop environment (e.g., "KDE").
    Result<String>        windowMgr;     ///< Window manager (e.g., "KWin").
    Result<ResourceUsage> diskUsage;     ///< Used/Total disk space for root filesystem.
    Result<String>        shell;         ///< Name of the current user shell (e.g., "zsh").
#if DRAC_ENABLE_PACKAGECOUNT
    Result<u64> packageCount; ///< Total number of packages installed.
#endif
#if DRAC_ENABLE_NOWPLAYING
    Result<MediaInfo> nowPlaying; ///< Result of fetching media info.
#endif
#if DRAC_ENABLE_WEATHER
    Result<weather::WeatherReport> weather; ///< Result of fetching weather info.
#endif

    /**
     * @brief Constructs a System object and initializes its members by fetching data.
     * @param config The configuration object containing settings for the system data.
     */
    explicit System(const Config& config);

    /**
     * @brief Fetches memory information.
     * @return Result containing memory usage information.
     */
    static fn getMemInfo() -> Result<ResourceUsage>;

#if DRAC_ENABLE_NOWPLAYING
    /**
     * @brief Fetches now playing media information.
     * @return Result containing the now playing media information.
     */
    static fn getNowPlaying() -> Result<MediaInfo>;
#endif

    /**
     * @brief Fetches the OS version.
     * @return Result containing the OS version.
     */
    static fn getOSVersion() -> Result<String>;

    /**
     * @brief Fetches the desktop environment.
     * @return Result containing the desktop environment.
     */
    static fn getDesktopEnvironment() -> Result<String>;

    /**
     * @brief Fetches the window manager.
     * @return Result containing the window manager.
     */
    static fn getWindowManager() -> Result<String>;

    /**
     * @brief Fetches the shell.
     * @return Result containing the shell.
     */
    static fn getShell() -> Result<String>;

    /**
     * @brief Fetches the host.
     * @return Result containing the host.
     */
    static fn getHost() -> Result<String>;

    /**
     * @brief Fetches the kernel version.
     * @return Result containing the kernel version.
     */
    static fn getKernelVersion() -> Result<String>;

    /**
     * @brief Fetches the disk usage.
     * @return Result containing the disk usage.
     */
    static fn getDiskUsage() -> Result<ResourceUsage>;

   private:
    /**
     * @brief Fetches the date.
     * @return Result containing the date.
     */
    static fn getDate() -> Result<String>;

#if DRAC_ENABLE_WEATHER
    /**
     * @brief Fetches the weather information.
     * @param config The configuration object containing settings for the weather.
     * @return Result containing the weather information.
     */
    static fn getWeatherInfo(const Config& config) -> Result<weather::WeatherReport>;
#endif
  };
} // namespace os