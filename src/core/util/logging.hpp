#pragma once

#include <chrono>          // std::chrono::{days, floor, seconds, system_clock}
#include <filesystem>      // std::filesystem::path
#include <format>          // std::format
#include <print>           // std::print
#include <source_location> // std::source_location
#include <utility>         // std::{forward, make_pair}

#include "src/core/util/defs.hpp"
#include "src/core/util/error.hpp"
#include "src/core/util/types.hpp"

namespace util::logging {
  using types::u8, types::i32, types::String, types::StringView, types::Option;

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
    constexpr fn operator&(Emphasis emphA, Emphasis emphB)->u8 {
      return static_cast<u8>(emphA) & static_cast<u8>(emphB);
    }

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
    // ReSharper disable CppDFAConstantParameter
    constexpr fn operator|(const Emphasis emph, const Color fgColor)->Style {
      return { .emph = emph, .fg_col = fgColor };
    }
    // ReSharper restore CppDFAConstantParameter

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
  } // namespace term

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

#ifdef _MSC_VER
    using enum term::Color;
#else
    using enum Color;
#endif // _MSC_VER

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
#endif // !NDEBUG

    Print("\n");
  }

  template <typename ErrorType>
  fn LogAppError(const LogLevel level, const ErrorType& error_obj) {
    using DecayedErrorType = std::decay_t<ErrorType>;

    std::source_location log_location;
    String               error_message_part;

    if constexpr (std::is_same_v<DecayedErrorType, error::DraconisError>) {
      log_location       = error_obj.location;
      error_message_part = error_obj.message;
    } else {
      log_location = std::source_location::current();
      if constexpr (std::is_base_of_v<std::exception, DecayedErrorType>)
        error_message_part = error_obj.what();
      else if constexpr (requires { error_obj.message; })
        error_message_part = error_obj.message;
      else
        error_message_part = "Unknown error type logged";
    }

    LogImpl(level, log_location, "{}", error_message_part);
  }

#ifndef NDEBUG
  #define debug_log(fmt, ...)                                                                           \
    ::util::logging::LogImpl(                                                                           \
      ::util::logging::LogLevel::DEBUG, std::source_location::current(), fmt __VA_OPT__(, ) __VA_ARGS__ \
    )
  #define debug_at(error_obj) ::util::logging::LogAppError(::util::logging::LogLevel::DEBUG, error_obj);
#else
  #define debug_log(...) ((void)0)
  #define debug_at(...)  ((void)0)
#endif

#define info_log(fmt, ...)                                                                           \
  ::util::logging::LogImpl(                                                                          \
    ::util::logging::LogLevel::INFO, std::source_location::current(), fmt __VA_OPT__(, ) __VA_ARGS__ \
  )
#define info_at(error_obj) ::util::logging::LogAppError(::util::logging::LogLevel::INFO, error_obj);

#define warn_log(fmt, ...)                                                                           \
  ::util::logging::LogImpl(                                                                          \
    ::util::logging::LogLevel::WARN, std::source_location::current(), fmt __VA_OPT__(, ) __VA_ARGS__ \
  )
#define warn_at(error_obj) ::util::logging::LogAppError(::util::logging::LogLevel::WARN, error_obj);

#define error_log(fmt, ...)                                                                           \
  ::util::logging::LogImpl(                                                                           \
    ::util::logging::LogLevel::ERROR, std::source_location::current(), fmt __VA_OPT__(, ) __VA_ARGS__ \
  )
#define error_at(error_obj) ::util::logging::LogAppError(::util::logging::LogLevel::ERROR, error_obj);
} // namespace util::logging
