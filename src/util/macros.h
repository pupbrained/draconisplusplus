#pragma once

#ifdef _WIN32
#undef ERROR
#endif

#include <chrono>
#include <filesystem>
#include <format>
#include <print>
#include <source_location>
#include <utility>

#include "types.h"

#define fn auto

#ifdef None
#undef None
#endif

#define None std::nullopt

namespace term {
  enum class Emphasis : u8 { none = 0, bold = 1, italic = 2 };

  constexpr fn operator|(Emphasis emphA, Emphasis emphB)->Emphasis {
    return static_cast<Emphasis>(static_cast<int>(emphA) | static_cast<int>(emphB));
  }

  enum class Color : u8 {
    black          = 30,
    red            = 31,
    green          = 32,
    yellow         = 33,
    blue           = 34,
    magenta        = 35,
    cyan           = 36,
    white          = 37,
    bright_black   = 90,
    bright_red     = 91,
    bright_green   = 92,
    bright_yellow  = 93,
    bright_blue    = 94,
    bright_magenta = 95,
    bright_cyan    = 96,
    bright_white   = 97
  };

  struct FgColor {
    Color col;

    constexpr explicit FgColor(const Color color) : col(color) {}

    [[nodiscard]] fn ansiCode() const -> String { return std::format("\033[{}m", static_cast<int>(col)); }
  };

  struct Style {
    Emphasis emph   = Emphasis::none;
    FgColor  fg_col = FgColor(static_cast<Color>(-1));

    [[nodiscard]] fn ansiCode() const -> String {
      String result;

      if (emph != Emphasis::none) {
        if ((static_cast<int>(emph) & static_cast<int>(Emphasis::bold)) != 0)
          result += "\033[1m";
        if ((static_cast<int>(emph) & static_cast<int>(Emphasis::italic)) != 0)
          result += "\033[3m";
      }

      if (static_cast<int>(fg_col.col) != -1)
        result += fg_col.ansiCode();

      return result;
    }
  };

  constexpr fn operator|(const Emphasis emph, const FgColor fgColor)->Style {
    return { .emph = emph, .fg_col = fgColor };
  }

  constexpr fn operator|(const FgColor fgColor, const Emphasis emph)->Style {
    return { .emph = emph, .fg_col = fgColor };
  }

  constexpr CStr reset = "\033[0m";

  template <typename... Args>
  fn Print(const Style& style, std::format_string<Args...> fmt, Args&&... args) -> void {
    std::print("{}{}{}", style.ansiCode(), std::format(fmt, std::forward<Args>(args)...), reset);
  }

  template <typename... Args>
  fn Print(const FgColor& fgColor, std::format_string<Args...> fmt, Args&&... args) -> void {
    std::print("{}{}{}", fgColor.ansiCode(), std::format(fmt, std::forward<Args>(args)...), reset);
  }

  template <typename... Args>
  fn Print(Emphasis emph, std::format_string<Args...> fmt, Args&&... args) -> void {
    Print({ .emph = emph }, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  fn Print(std::format_string<Args...> fmt, Args&&... args) -> void {
    std::print(fmt, std::forward<Args>(args)...);
  }
}

namespace log_colors {
  using term::Color;

  constexpr Color debug = Color::cyan, info = Color::green, warn = Color::yellow, error = Color::red,
                  timestamp = Color::bright_white, file_info = Color::bright_white;
}

enum class LogLevel : u8 { DEBUG, INFO, WARN, ERROR };

template <typename... Args>
fn LogImpl(const LogLevel level, const std::source_location& loc, std::format_string<Args...> fmt, Args&&... args)
  -> void {
  using namespace std::chrono;

  const time_point<system_clock, duration<long long, std::ratio<1, 1>>> now =
    std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());

  const auto [color, levelStr] = [&] {
    switch (level) {
      case LogLevel::DEBUG: return std::make_pair(log_colors::debug, "DEBUG");
      case LogLevel::INFO:  return std::make_pair(log_colors::info, "INFO ");
      case LogLevel::WARN:  return std::make_pair(log_colors::warn, "WARN ");
      case LogLevel::ERROR: return std::make_pair(log_colors::error, "ERROR");
      default:              std::unreachable();
    }
  }();

  const String filename = std::filesystem::path(loc.file_name()).lexically_normal().string();

  using namespace term;

  Print(FgColor(log_colors::timestamp), "[{:%H:%M:%S}] ", now);
  Print(Emphasis::bold | FgColor(color), "{} ", levelStr);
  Print(fmt, std::forward<Args>(args)...);

#ifndef NDEBUG
  Print(FgColor(log_colors::file_info), "\n{:>14} ", "╰──");
  Print(Emphasis::italic | FgColor(log_colors::file_info), "{}:{}", filename, loc.line());
#endif

  Print("\n");
}

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-macros"
#endif
#ifdef NDEBUG
#define DEBUG_LOG(...) static_cast<void>(0)
#else
#define DEBUG_LOG(...) LogImpl(LogLevel::DEBUG, std::source_location::current(), __VA_ARGS__)
#endif
#define INFO_LOG(...) LogImpl(LogLevel::INFO, std::source_location::current(), __VA_ARGS__)
#define WARN_LOG(...) LogImpl(LogLevel::WARN, std::source_location::current(), __VA_ARGS__)
#define ERROR_LOG(...) LogImpl(LogLevel::ERROR, std::source_location::current(), __VA_ARGS__)
#ifdef __clang__
#pragma clang diagnostic pop
#endif
