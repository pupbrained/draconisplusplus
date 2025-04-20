#pragma once

// probably stupid but it fixes the issue with windows.h defining ERROR
#undef ERROR
#include <chrono>
#include <filesystem>
#include <format>
#include <print>
#include <source_location>
#include <utility>

#include "types.h"

#define fn auto // Rust-style function shorthand

// Terminal color implementation to replace fmt::color
namespace term {
  // Text styles
  enum class Emphasis : u8 { none = 0, bold = 1, italic = 2 };

  constexpr fn operator|(Emphasis emphA, Emphasis emphB)->Emphasis {
    return static_cast<Emphasis>(static_cast<int>(emphA) | static_cast<int>(emphB));
  }

  // Terminal colors
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

  // Style wrapper for foreground color
  struct FgColor {
    Color col;

    constexpr explicit FgColor(Color color) : col(color) {}

    [[nodiscard]] fn ansiCode() const -> std::string { return std::format("\033[{}m", static_cast<int>(col)); }
  };

  // Create a foreground color modifier
  constexpr fn Fg(Color color) -> FgColor { return FgColor(color); }

  // Combined style (emphasis + color)
  struct Style {
    Emphasis emph   = Emphasis::none;
    FgColor  fg_col = FgColor(static_cast<Color>(-1)); // Invalid color

    [[nodiscard]] fn ansiCode() const -> std::string {
      std::string result;

      if (emph != Emphasis::none) {
        if ((static_cast<int>(emph) & static_cast<int>(Emphasis::bold)) != 0) {
          result += "\033[1m";
        }
        if ((static_cast<int>(emph) & static_cast<int>(Emphasis::italic)) != 0) {
          result += "\033[3m";
        }
      }

      if (static_cast<int>(fg_col.col) != -1) {
        result += fg_col.ansiCode();
      }

      return result;
    }
  };

  // Combine emphasis and color
  constexpr fn operator|(Emphasis emph, FgColor fgColor)->Style { return { .emph = emph, .fg_col = fgColor }; }

  constexpr fn operator|(FgColor fgColor, Emphasis emph)->Style { return emph | fgColor; }

  // Reset all styles
  constexpr const char* reset = "\033[0m";

  // Print with style
  template <typename... Args>
  fn Print(const Style& style, std::format_string<Args...> fmt, Args&&... args) -> void {
    std::print("{}{}{}", style.ansiCode(), std::format(fmt, std::forward<Args>(args)...), reset);
  }

  // Print with foreground color only
  template <typename... Args>
  fn Print(const FgColor& fgColor, std::format_string<Args...> fmt, Args&&... args) -> void {
    std::print("{}{}{}", fgColor.ansiCode(), std::format(fmt, std::forward<Args>(args)...), reset);
  }

  // Print with emphasis only
  template <typename... Args>
  fn Print(Emphasis emph, std::format_string<Args...> fmt, Args&&... args) -> void {
    Print({ .emph = emph }, fmt, std::forward<Args>(args)...);
  }

  // Print without styling (plain text)
  template <typename... Args>
  fn Print(std::format_string<Args...> fmt, Args&&... args) -> void {
    std::print(fmt, std::forward<Args>(args)...);
  }
}

namespace log_colors {
  using term::Color;
  constexpr auto debug = Color::cyan, info = Color::green, warn = Color::yellow, error = Color::red,
                 timestamp = Color::bright_white, file_info = Color::bright_white;
}

enum class LogLevel : u8 { DEBUG, INFO, WARN, ERROR };

template <typename... Args>
void LogImpl(const LogLevel level, const std::source_location& loc, std::format_string<Args...> fmt, Args&&... args) {
  const auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());

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
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcovered-switch-default"
      default:
        std::unreachable();
#pragma clang diagnostic pop
    }
  }();
  const string filename = std::filesystem::path(loc.file_name()).lexically_normal().string();

  // Timestamp and level - using std::chrono with std::format
  term::Print(term::Fg(log_colors::timestamp), "[{:%H:%M:%S}] ", now);
  term::Print(term::Emphasis::bold | term::Fg(color), "{} ", levelStr);
  // Message
  term::Print(fmt, std::forward<Args>(args)...);
  // File info (debug builds only)
#ifndef NDEBUG
  term::Print(term::Fg(log_colors::file_info), "\n{:>14} ", "╰──");
  term::Print(term::Emphasis::italic | term::Fg(log_colors::file_info), "{}:{}", filename, loc.line());
#endif
  term::Print("\n");
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-macros"
#ifdef NDEBUG
#define DEBUG_LOG(...) static_cast<void>(0)
#else
#define DEBUG_LOG(...) LogImpl(LogLevel::DEBUG, std::source_location::current(), __VA_ARGS__)
#endif
#define INFO_LOG(...) LogImpl(LogLevel::INFO, std::source_location::current(), __VA_ARGS__)
#define WARN_LOG(...) LogImpl(LogLevel::WARN, std::source_location::current(), __VA_ARGS__)
#define ERROR_LOG(...) LogImpl(LogLevel::ERROR, std::source_location::current(), __VA_ARGS__)
#pragma clang diagnostic pop
