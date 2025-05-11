#pragma once

#include <format> // std::{formatter, format_to}

#include "Services/Weather.hpp"
#include "Util/Definitions.hpp"
#include "Util/Error.hpp"
#include "Util/Types.hpp"

struct Config;

using util::types::u64, util::types::f64, util::types::String, util::types::Option, util::types::Result,
  util::types::MediaInfo, util::types::DiskSpace;

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
  explicit constexpr BytesToGiB(u64 value)
    : value(value) {}
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
 * i32 main() {
 *   BytesToGiB data_size{2'147'483'648}; // 2 GiB
 *   String formatted = std::format("Size: {}", data_size);
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
   * @struct SystemData
   * @brief Holds various pieces of system information collected from the OS.
   *
   * This structure aggregates information about the system,
   * in order to display it at all at once during runtime.
   */
  struct SystemData {
    Result<String>                 date;          ///< Current date (e.g., "April 26th").
    Result<String>                 host;          ///< Host/product family (e.g., "MacBook Air").
    Result<String>                 kernelVersion; ///< OS kernel version (e.g., "6.14.4").
    Result<String>                 osVersion;     ///< OS pretty name (e.g., "Ubuntu 24.04.2 LTS").
    Result<u64>                    memInfo;       ///< Total physical RAM in bytes.
    Result<String>                 desktopEnv;    ///< Desktop environment (e.g., "KDE").
    Result<String>                 windowMgr;     ///< Window manager (e.g., "KWin").
    Result<DiskSpace>              diskUsage;     ///< Used/Total disk space for root filesystem.
    Result<String>                 shell;         ///< Name of the current user shell (e.g., "zsh").
    Result<u64>                    packageCount;  ///< Total number of packages installed.
    Result<MediaInfo>              nowPlaying;    ///< Result of fetching media info.
    Result<weather::WeatherReport> weather;       ///< Result of fetching weather info.

    /**
     * @brief Constructs a SystemData object and initializes its members.
     * @param config The configuration object containing settings for the system data.
     */
    explicit SystemData(const Config& config);
  };
} // namespace os
