#pragma once

// probably stupid but it fixes the issue with windows.h defining ERROR
#undef ERROR

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
                                timestamp = terminal_color::bright_white, file_info = terminal_color::bright_white;
}

enum class LogLevel : u8 { DEBUG, INFO, WARN, ERROR };

template <typename... Args>
void LogImpl(LogLevel level, const std::source_location& loc, fmt::format_string<Args...> fmt, Args&&... args) {
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
  const struct tm   time     = *std::localtime(&now);

  // Timestamp and level
  fmt::print(fg(log_colors::timestamp), "[{:%H:%M:%S}] ", time);
  fmt::print(fmt::emphasis::bold | fg(color), "{} ", levelStr);

  // Message
  fmt::print(fmt, std::forward<Args>(args)...);

  // File info (debug builds only)
#ifndef NDEBUG
  fmt::print(fg(log_colors::file_info), "\n{:>14} ", "╰──");
  fmt::print(fmt::emphasis::italic | fg(log_colors::file_info), "{}:{}", filename, loc.line());
#endif

  fmt::print("\n");
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-macros"
#ifdef NDEBUG
#define DEBUG_LOG(...) (void)0
#else
#define DEBUG_LOG(...) LogImpl(LogLevel::DEBUG, std::source_location::current(), __VA_ARGS__)
#endif

#define INFO_LOG(...) LogImpl(LogLevel::INFO, std::source_location::current(), __VA_ARGS__)
#define WARN_LOG(...) LogImpl(LogLevel::WARN, std::source_location::current(), __VA_ARGS__)
#define ERROR_LOG(...) LogImpl(LogLevel::ERROR, std::source_location::current(), __VA_ARGS__)
#pragma clang diagnostic pop
