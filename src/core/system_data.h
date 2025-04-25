#pragma once

#include "src/config/config.h"
#include "src/util/types.h"

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
  using NowPlayingResult = Option<Result<String, NowPlayingError>>;

  // clang-format off
  String                 date;                ///< Current date (e.g., "April 24th").
  String                 host;                ///< Host or product family name (e.g., "MacBook Pro").
  String                 kernel_version;      ///< OS kernel version (e.g., "5.15.0-generic").
  Result<String, String> os_version;          ///< OS pretty name (e.g., "Ubuntu 22.04 LTS") or an error message.
  Result<u64, String>    mem_info;            ///< Total physical RAM in bytes or an error message.
  Option<String>         desktop_environment; ///< Detected desktop environment (e.g., "GNOME", "KDE", "Fluent (Windows 11)"). Might be None.
  String                 window_manager;      ///< Detected window manager (e.g., "Mutter", "KWin", "DWM").
  NowPlayingResult       now_playing;         ///< Currently playing media ("Artist - Title") or an error/None if disabled/unavailable.
  Option<WeatherOutput>  weather_info;        ///< Weather information or None if disabled/unavailable.
  u64                    disk_used;           ///< Used disk space in bytes for the root filesystem.
  u64                    disk_total;          ///< Total disk space in bytes for the root filesystem.
  String                 shell;               ///< Name of the current user shell (e.g., "Bash", "Zsh", "PowerShell").
  // clang-format on

  /**
   * @brief Fetches all system data asynchronously.
   * @param config The application configuration.
   * @return A populated SystemData object.
   */
  static fn fetchSystemData(const Config& config) -> SystemData;
};
