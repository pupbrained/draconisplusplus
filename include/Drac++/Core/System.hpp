#pragma once

#if DRAC_ENABLE_WEATHER
  #include "Drac++/Services/Weather.hpp"
#endif

#include "DracUtils/Definitions.hpp"
#include "DracUtils/Error.hpp"
#include "DracUtils/Formatting.hpp"
#include "DracUtils/Types.hpp"

/// @brief Conversion factor from bytes to GiB
constexpr util::types::u64 GIB = 1'073'741'824;

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

namespace util::formatting {
  template <>
  struct Formatter<BytesToGiB> {
    Formatter<double> doubleFormatter;

    constexpr fn parse(detail::FormatParseContext& parseCtx) {
      doubleFormatter.parse(parseCtx);
    }

    fn format(const BytesToGiB& btg, auto& ctx) const {
      Formatter<double> localDoubleFormatter;

      localDoubleFormatter.precision = 2;
      localDoubleFormatter.fmt       = std::chars_format::fixed;

      localDoubleFormatter.format(static_cast<double>(btg.value) / GIB, ctx);

      ctx.out().append("GiB");
    }
  };
} // namespace util::formatting

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