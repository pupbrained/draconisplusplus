#pragma once

#include <filesystem>
#include <fmt/chrono.h>
#include <fmt/color.h>
#include <fmt/format.h>
#include <source_location>

#include "types.h"

#define fn auto // Rust-style function shorthand

namespace log_colors {
  using fmt::terminal_color;
  constexpr fmt::terminal_color debug = terminal_color::cyan, info = terminal_color::green,
                                warn = terminal_color::yellow, error = terminal_color::red,
                                timestamp = terminal_color::bright_white,
                                file_info = terminal_color::bright_white;
}

enum class LogLevel : u8 { DEBUG, INFO, WARN, ERROR };

template <typename... Args>
fn LogImpl(
  LogLevel                    level,
  const std::source_location& loc,
  fmt::format_string<Args...> fmt,
  Args&&... args
) -> void {
  const time_t now             = std::time(nullptr);
  const auto [color, levelStr] = [&] {
    switch (level) {
      case LogLevel::DEBUG:
        return std::make_pair(log_colors::debug, "DEBUG");
      case LogLevel::INFO:
        return std::make_pair(log_colors::info, "INFO ");
      case LogLevel::WARN:
        return std::make_pair(log_colors::warn, "WARN ");
      case LogLevel::ERROR:
        return std::make_pair(log_colors::error, "ERROR");
    }
  }();

  const std::string filename = std::filesystem::path(loc.file_name()).lexically_normal().string();
  const u32         line     = loc.line();
  const struct tm   time     = *std::localtime(&now);

  // Timestamp section
  fmt::print(fg(log_colors::timestamp), "[{:%H:%M:%S}] ", time);

  // Level section
  fmt::print(fmt::emphasis::bold | fg(color), "{}", levelStr);

  // Message section
  fmt::print(" ");
  fmt::print(fmt, std::forward<Args>(args)...);

  // File info section
#ifndef NDEBUG
  fmt::print(fg(log_colors::file_info), "\n{:>14} ", "╰──");
  const std::string fileInfo = fmt::format("{}:{}", filename.c_str(), line);
  fmt::print(fmt::emphasis::italic | fg(log_colors::file_info), "{}", fileInfo);
#endif

  fmt::print("\n");
}

// Logging utility wrapper to replace macros
// Logging utility wrapper to replace macros
template <LogLevel level>
struct LogWrapper {
  std::source_location m_loc; // Changed to m_loc

  constexpr LogWrapper(const std::source_location& loc = std::source_location::current())
    : m_loc(loc) {} // Initialize member with parameter

  template <typename... Args>
  void operator()(fmt::format_string<Args...> fmt, Args&&... args) const {
    LogImpl(level, m_loc, fmt, std::forward<Args>(args)...); // Use m_loc
  }
};

// Debug logging is conditionally compiled
#ifdef NDEBUG
struct {
  template <typename... Args>
  void operator()(fmt::format_string<Args...>, Args&&...) const {}
} DEBUG_LOG;
#else
constexpr LogWrapper<LogLevel::DEBUG> DEBUG_LOG;
#endif

// Define loggers for other levels
constexpr LogWrapper<LogLevel::INFO>  INFO_LOG;
constexpr LogWrapper<LogLevel::WARN>  WARN_LOG;
constexpr LogWrapper<LogLevel::ERROR> ERROR_LOG;
