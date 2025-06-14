#pragma once

#include <format> // std::{formatter, format_to}

#if DRAC_ENABLE_WEATHER
  #include "Drac++/Services/Weather.hpp"
#endif

#include "DracUtils/Definitions.hpp"
#include "DracUtils/Error.hpp"
#include "DracUtils/Types.hpp"

/**
 * @struct BytesToGiB
 * @brief Helper struct to format a byte value to GiB (Gibibytes).
 *
 * Encapsulates a byte value and provides a custom formatter
 * to convert it to GiB for display purposes.
 */
struct BytesToGiB {
  util::types::u64 value; ///< The byte value to be converted.

  /**
   * @brief Constructor for BytesToGiB.
   * @param value The byte value to be converted.
   */
  explicit constexpr BytesToGiB(const util::types::u64 value) : value(value) {}
};

/// @brief Conversion factor from bytes to GiB
constexpr util::types::u64 GIB = 1'073'741'824;

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
    return std::format_to(ctx.out(), "{:.2f}GiB", static_cast<util::types::f64>(BTG.value) / GIB);
  }
};

namespace os {
  /**
   * @struct SystemInfo
   * @brief Groups related system information that is often fetched together
   */
  struct SystemInfo {
    util::types::Result<util::types::SZString>      date;          ///< Current date (e.g., "April 26th").
    util::types::Result<util::types::SZString>      host;          ///< Host/product family (e.g., "MacBook Air").
    util::types::Result<util::types::SZString>      kernelVersion; ///< OS kernel version (e.g., "6.14.4").
    util::types::Result<util::types::SZString>      osVersion;     ///< OS pretty name (e.g., "Ubuntu 24.04.2 LTS").
    util::types::Result<util::types::ResourceUsage> memInfo;       ///< Total physical RAM in bytes.
    util::types::Result<util::types::SZString>      desktopEnv;    ///< Desktop environment (e.g., "KDE").
    util::types::Result<util::types::SZString>      windowMgr;     ///< Window manager (e.g., "KWin").
    util::types::Result<util::types::ResourceUsage> diskUsage;     ///< Used/Total disk space for root filesystem.
    util::types::Result<util::types::SZString>      shell;         ///< Name of the current user shell (e.g., "zsh").
    util::types::Result<util::types::SZString>      cpuModel;      ///< CPU model name.
    util::types::Result<util::types::SZString>      gpuModel;      ///< GPU model name.
#if DRAC_ENABLE_PACKAGECOUNT
    util::types::Result<util::types::u64> packageCount; ///< Total number of packages installed.
#endif
#if DRAC_ENABLE_NOWPLAYING
    util::types::Result<util::types::MediaInfo> nowPlaying; ///< Result of fetching media info.
#endif
#if DRAC_ENABLE_WEATHER
    util::types::Result<weather::WeatherReport> weather; ///< Result of fetching weather info.
#endif
  };

  /**
   * @struct EnvironmentInfo
   * @brief Groups desktop environment related information
   */
  struct EnvironmentInfo {
    util::types::String desktopEnv; ///< Desktop environment
    util::types::String windowMgr;  ///< Window manager
    util::types::String shell;      ///< Current user shell
  };

  /**
   * @struct ResourceInfo
   * @brief Groups system resource usage information
   */
  struct ResourceInfo {
    util::types::ResourceUsage memInfo;   ///< Memory usage information
    util::types::ResourceUsage diskUsage; ///< Disk usage information
  };

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
    util::types::Result<util::types::SZString>      date;          ///< Current date (e.g., "April 26th").
    util::types::Result<util::types::SZString>      host;          ///< Host/product family (e.g., "MacBook Air").
    util::types::Result<util::types::SZString>      kernelVersion; ///< OS kernel version (e.g., "6.14.4").
    util::types::Result<util::types::SZString>      osVersion;     ///< OS pretty name (e.g., "Ubuntu 24.04.2 LTS").
    util::types::Result<util::types::ResourceUsage> memInfo;       ///< Total physical RAM in bytes.
    util::types::Result<util::types::SZString>      desktopEnv;    ///< Desktop environment (e.g., "KDE").
    util::types::Result<util::types::SZString>      windowMgr;     ///< Window manager (e.g., "KWin").
    util::types::Result<util::types::ResourceUsage> diskUsage;     ///< Used/Total disk space for root filesystem.
    util::types::Result<util::types::SZString>      shell;         ///< Name of the current user shell (e.g., "zsh").
    util::types::Result<util::types::SZString>      cpuModel;      ///< CPU model name.
    util::types::Result<util::types::SZString>      gpuModel;      ///< GPU model name.
#if DRAC_ENABLE_PACKAGECOUNT
    util::types::Result<util::types::u64> packageCount; ///< Total number of packages installed.
#endif
#if DRAC_ENABLE_NOWPLAYING
    util::types::Result<util::types::MediaInfo> nowPlaying; ///< Result of fetching media info.
#endif
#if DRAC_ENABLE_WEATHER
    util::types::Result<weather::WeatherReport> weather; ///< Result of fetching weather info.
#endif

    /**
     * @brief Fetches memory information.
     * @return Result containing memory usage information.
     */
    static fn getMemInfo() -> util::types::Result<util::types::ResourceUsage>;

#if DRAC_ENABLE_NOWPLAYING
    /**
     * @brief Fetches now playing media information.
     * @return Result containing the now playing media information.
     */
    static fn getNowPlaying() -> util::types::Result<util::types::MediaInfo>;
#endif

    /**
     * @brief Fetches the OS version.
     * @return Result containing the OS version.
     */
    static fn getOSVersion() -> util::types::Result<util::types::SZString>;

    /**
     * @brief Fetches the desktop environment.
     * @return Result containing the desktop environment.
     */
    static fn getDesktopEnvironment() -> util::types::Result<util::types::SZString>;

    /**
     * @brief Fetches the window manager.
     * @return Result containing the window manager.
     */
    static fn getWindowManager() -> util::types::Result<util::types::SZString>;

    /**
     * @brief Fetches the shell.
     * @return Result containing the shell.
     */
    static fn getShell() -> util::types::Result<util::types::SZString>;

    /**
     * @brief Fetches the host.
     * @return Result containing the host.
     */
    static fn getHost() -> util::types::Result<util::types::SZString>;

    /**
     * @brief Fetches the CPU model.
     * @return Result containing the CPU model.
     */
    static fn getCPUModel() -> util::types::Result<util::types::SZString>;

    /**
     * @brief Fetches the GPU model.
     * @return Result containing the GPU model.
     */
    static fn getGPUModel() -> util::types::Result<util::types::SZString>;

    /**
     * @brief Fetches the kernel version.
     * @return Result containing the kernel version.
     */
    static fn getKernelVersion() -> util::types::Result<util::types::SZString>;

    /**
     * @brief Fetches the disk usage.
     * @return Result containing the disk usage.
     */
    static fn getDiskUsage() -> util::types::Result<util::types::ResourceUsage>;

    /**
     * @brief Fetches the date.
     * @return Result containing the date.
     */
    static fn getDate() -> util::types::Result<util::types::SZString>;
  };
} // namespace os