#pragma once

#include <format> // std::{formatter, format_to}

#include "src/config/weather.hpp" // weather::Output

#include "util/defs.hpp"
#include "util/error.hpp"
#include "util/types.hpp"

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
 * #include "system_data.h" // Assuming BytesToGiB is defined here
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

/**
 * @struct SystemData
 * @brief Holds various pieces of system information collected from the OS.
 *
 * This structure aggregates information about the system,
 * in order to display it at all at once during runtime.
 */
struct SystemData {
  using NowPlayingResult = Option<Result<MediaInfo, util::error::DraconisError>>;

  // clang-format off
  String                           date;                ///< Current date (e.g., "April 26th"). Always expected to succeed.
  Result<String, util::error::DraconisError>    host;                ///< Host/product family (e.g., "MacBookPro18,3") or OS util::erroror.
  Result<String, util::error::DraconisError>    kernel_version;      ///< OS kernel version (e.g., "23.4.0") or OS error.
  Result<String, util::error::DraconisError>    os_version;          ///< OS pretty name (e.g., "macOS Sonoma 14.4.1") or OS error.
  Result<u64, util::error::DraconisError>       mem_info;            ///< Total physical RAM in bytes or OS error.
  Option<String>                   desktop_environment; ///< Detected desktop environment (e.g., "Aqua", "Plasma"). None if not detected/applicable.
  Option<String>                   window_manager;      ///< Detected window manager (e.g., "Quartz Compositor", "KWin"). None if not detected/applicable.
  Result<DiskSpace, util::error::DraconisError> disk_usage;          ///< Used/Total disk space for root filesystem or OS error.
  Option<String>                   shell;               ///< Name of the current user shell (e.g., "zsh"). None if not detected.
  NowPlayingResult                 now_playing;         ///< Optional: Result of fetching media info (MediaInfo on success, NowPlayingError on failure). None if disabled.
  Option<weather::Output>          weather_info;        ///< Optional: Weather information. None if disabled or util::erroror during fetch.
  // clang-format on

  /**
   * @brief Fetches all system data asynchronously.
   * @param config The application configuration.
   * @return A populated SystemData object.
   */
  static fn fetchSystemData(const Config& config) -> SystemData;
};
