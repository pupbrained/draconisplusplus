// ReSharper disable CppDFAConstantParameter
#pragma once

// Fixes conflict in Windows with <windows.h>
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

/// Macro alias for trailing return type functions.
#define fn auto

/// Macro alias for std::nullopt, represents an empty optional value.
#define None std::nullopt

/**
 * @namespace term
 * @brief Provides terminal-related utilities, including color and style formatting.
 */
namespace term {
  /**
   * @enum Emphasis
   * @brief Represents text emphasis styles.
   *
   * Enum values can be combined using bitwise OR to apply multiple styles at once.
   */
  enum class Emphasis : u8 {
    Bold,  ///< Bold text.
    Italic ///< Italic text.
  };

  /**
   * @enum Color
   * @brief Represents ANSI color codes for terminal output.
   *
   * Color codes can be used to format terminal output.
   */
  enum class Color : u8 {
    Black         = 30, ///< Black color.
    Red           = 31, ///< Red color.
    Green         = 32, ///< Green color.
    Yellow        = 33, ///< Yellow color.
    Blue          = 34, ///< Blue color.
    Magenta       = 35, ///< Magenta color.
    Cyan          = 36, ///< Cyan color.
    White         = 37, ///< White color.
    BrightBlack   = 90, ///< Bright black (gray) color.
    BrightRed     = 91, ///< Bright red color.
    BrightGreen   = 92, ///< Bright green color.
    BrightYellow  = 93, ///< Bright yellow color.
    BrightBlue    = 94, ///< Bright blue color.
    BrightMagenta = 95, ///< Bright magenta color.
    BrightCyan    = 96, ///< Bright cyan color.
    BrightWhite   = 97, ///< Bright white color.
  };

  /**
   * @brief Combines two emphasis styles using bitwise OR.
   * @param emphA The first emphasis style.
   * @param emphB The second emphasis style.
   * @return The combined emphasis style.
   */
  constexpr fn operator|(Emphasis emphA, Emphasis emphB)->Emphasis {
    return static_cast<Emphasis>(static_cast<u8>(emphA) | static_cast<u8>(emphB));
  }

  /**
   * @brief Checks if two emphasis styles are equal using bitwise AND.
   * @param emphA The first emphasis style.
   * @param emphB The second emphasis style.
   * @return The result of the bitwise AND operation.
   */
  constexpr fn operator&(Emphasis emphA, Emphasis emphB)->u8 { return static_cast<u8>(emphA) & static_cast<u8>(emphB); }

  /**
   * @struct Style
   * @brief Represents a combination of text styles.
   *
   * Emphasis and color are both optional, allowing for flexible styling.
   */
  struct Style {
    Option<Emphasis> emph;   ///< Optional emphasis style.
    Option<Color>    fg_col; ///< Optional foreground color style.

    /**
     * @brief Generates the ANSI escape code for the combined styles.
     * @return The ANSI escape code for the combined styles.
     */
    [[nodiscard]] fn ansiCode() const -> String {
      String result;

      if (emph) {
        if ((*emph & Emphasis::Bold) != 0)
          result += "\033[1m";
        if ((*emph & Emphasis::Italic) != 0)
          result += "\033[3m";
      }

      if (fg_col)
        result += std::format("\033[{}m", static_cast<u8>(*fg_col));

      return result;
    }
  };

  /**
   * @brief Combines an emphasis style and a foreground color into a Style.
   * @param emph The emphasis style to apply.
   * @param fgColor The foreground color to apply.
   * @return The combined style.
   */
  constexpr fn operator|(const Emphasis emph, const Color fgColor)->Style {
    return { .emph = emph, .fg_col = fgColor };
  }

  /**
   * @brief Combines a foreground color and an emphasis style into a Style.
   * @param fgColor The foreground color to apply.
   * @param emph The emphasis style to apply.
   * @return The combined style.
   */
  constexpr fn operator|(const Color fgColor, const Emphasis emph)->Style {
    return { .emph = emph, .fg_col = fgColor };
  }

  /**
   * @brief Prints formatted text with the specified style.
   * @tparam Args Parameter pack for format arguments.
   * @param style The Style object containing emphasis and/or color.
   * @param fmt The format string.
   * @param args The arguments for the format string.
   */
  template <typename... Args>
  fn Print(const Style& style, std::format_string<Args...> fmt, Args&&... args) -> void {
    if (const String styleCode = style.ansiCode(); styleCode.empty())
      std::print(fmt, std::forward<Args>(args)...);
    else
      std::print("{}{}{}", styleCode, std::format(fmt, std::forward<Args>(args)...), "\033[0m");
  }

  /**
   * @brief Prints formatted text with the specified foreground color.
   * @tparam Args Parameter pack for format arguments.
   * @param fgColor The foreground color to apply.
   * @param fmt The format string.
   * @param args The arguments for the format string.
   */
  template <typename... Args>
  fn Print(const Color& fgColor, std::format_string<Args...> fmt, Args&&... args) -> void {
    Print({ .emph = None, .fg_col = fgColor }, fmt, std::forward<Args>(args)...);
  }

  /**
   * @brief Prints formatted text with the specified emphasis style.
   * @tparam Args Parameter pack for format arguments.
   * @param emph The emphasis style to apply.
   * @param fmt The format string.
   * @param args The arguments for the format string.
   */
  template <typename... Args>
  fn Print(const Emphasis emph, std::format_string<Args...> fmt, Args&&... args) -> void {
    Print({ .emph = emph, .fg_col = None }, fmt, std::forward<Args>(args)...);
  }

  /**
   * @brief Prints formatted text with no specific style (default terminal style).
   * @tparam Args Parameter pack for format arguments.
   * @param fmt The format string.
   * @param args The arguments for the format string.
   */
  template <typename... Args>
  fn Print(std::format_string<Args...> fmt, Args&&... args) -> void {
    // Directly use std::print for unstyled output
    std::print(fmt, std::forward<Args>(args)...);
  }
}

/**
 * @enum LogLevel
 * @brief Represents different log levels.
 */
enum class LogLevel : u8 { DEBUG, INFO, WARN, ERROR };

/**
 * @brief Logs a message with the specified log level, source location, and format string.
 * @tparam Args Parameter pack for format arguments.
 * @param level The log level (DEBUG, INFO, WARN, ERROR).
 * @param loc The source location of the log message.
 * @param fmt The format string.
 * @param args The arguments for the format string.
 */
template <typename... Args>
fn LogImpl(const LogLevel level, const std::source_location& loc, std::format_string<Args...> fmt, Args&&... args) {
  using namespace std::chrono;
  using namespace term;
#ifdef _WIN32
  using enum term::Color;
#else
  using enum Color;
#endif

  const auto [color, levelStr] = [&] {
    switch (level) {
      case LogLevel::DEBUG: return std::make_pair(Cyan, "DEBUG");
      case LogLevel::INFO:  return std::make_pair(Green, "INFO ");
      case LogLevel::WARN:  return std::make_pair(Yellow, "WARN ");
      case LogLevel::ERROR: return std::make_pair(Red, "ERROR");
      default:              std::unreachable();
    }
  }();

  Print(BrightWhite, "[{:%X}] ", std::chrono::floor<seconds>(system_clock::now()));
  Print(Emphasis::Bold | color, "{} ", levelStr);
  Print(fmt, std::forward<Args>(args)...);

#ifndef NDEBUG
  Print(BrightWhite, "\n{:>14} ", "╰──");
  Print(
    Emphasis::Italic | BrightWhite,
    "{}:{}",
    std::filesystem::path(loc.file_name()).lexically_normal().string(),
    loc.line()
  );
#endif

  Print("\n");
}

// Suppress unused macro warnings in Clang
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-macros"
#endif

#ifdef NDEBUG
#define DEBUG_LOG(...) static_cast<void>(0)
#else
/**
 * @def DEBUG_LOG
 * @brief Logs a message at the DEBUG level.
 * @details Only active in non-release builds (when NDEBUG is not defined).
 * Includes timestamp, level, message, and source location.
 * @param ... Format string and arguments for the log message.
 */
#define DEBUG_LOG(...) LogImpl(LogLevel::DEBUG, std::source_location::current(), __VA_ARGS__)
#endif

/**
 * @def INFO_LOG(...)
 * @brief Logs a message at the INFO level.
 * @details Includes timestamp, level, message, and source location (in debug builds).
 * @param ... Format string and arguments for the log message.
 */
#define INFO_LOG(...) LogImpl(LogLevel::INFO, std::source_location::current(), __VA_ARGS__)

/**
 * @def WARN_LOG(...)
 * @brief Logs a message at the WARN level.
 * @details Includes timestamp, level, message, and source location (in debug builds).
 * @param ... Format string and arguments for the log message.
 */
#define WARN_LOG(...) LogImpl(LogLevel::WARN, std::source_location::current(), __VA_ARGS__)

/**
 * @def ERROR_LOG(...)
 * @brief Logs a message at the ERROR level.
 * @details Includes timestamp, level, message, and source location (in debug builds).
 * @param ... Format string and arguments for the log message.
 */
#define ERROR_LOG(...) LogImpl(LogLevel::ERROR, std::source_location::current(), __VA_ARGS__)

/**
 * @def RETURN_ERR(...)
 * @brief Logs an error message and returns a value.
 * @details Logs the error message with the ERROR log level and returns the specified value.
 * @param ... Format string and arguments for the error message.
 */
#define RETURN_ERR(...)     \
  do {                      \
    ERROR_LOG(__VA_ARGS__); \
    return None;            \
  } while (0)

#ifdef __clang__
#pragma clang diagnostic pop
#endif
