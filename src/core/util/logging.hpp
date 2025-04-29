#pragma once

#include <chrono>                 // std::chrono::{days, floor, seconds, system_clock}
#include <filesystem>             // std::filesystem::path
#include <format>                 // std::format
#include <ftxui/screen/color.hpp> // ftxui::Color
#include <mutex>                  // std::mutex
#include <print>                  // std::print
#include <source_location>        // std::source_location
#include <utility>                // std::forward

#include "src/core/util/defs.hpp"
#include "src/core/util/error.hpp"
#include "src/core/util/types.hpp"

namespace util::logging {
  using types::usize, types::u8, types::i32, types::i64, types::CStr, types::String, types::StringView, types::Array,
    types::Option, types::None;

  // Store all compile-time constants in a struct
  struct LogLevelConst {
    // ANSI color codes
    // clang-format off
    static constexpr Array<const char*, 16> COLOR_CODE_LITERALS = {
      "\033[38;5;0m",  "\033[38;5;1m",  "\033[38;5;2m",  "\033[38;5;3m",
      "\033[38;5;4m",  "\033[38;5;5m",  "\033[38;5;6m",  "\033[38;5;7m",
      "\033[38;5;8m",  "\033[38;5;9m",  "\033[38;5;10m", "\033[38;5;11m",
      "\033[38;5;12m", "\033[38;5;13m", "\033[38;5;14m", "\033[38;5;15m",
    };
    // clang-format on

    // ANSI formatting constants
    static constexpr const char* RESET_CODE   = "\033[0m";
    static constexpr const char* BOLD_START   = "\033[1m";
    static constexpr const char* BOLD_END     = "\033[22m";
    static constexpr const char* ITALIC_START = "\033[3m";
    static constexpr const char* ITALIC_END   = "\033[23m";

    // Log level text constants
    static constexpr StringView DEBUG_STR = "DEBUG";
    static constexpr StringView INFO_STR  = "INFO ";
    static constexpr StringView WARN_STR  = "WARN ";
    static constexpr StringView ERROR_STR = "ERROR";

    // Log level color constants
    static constexpr ftxui::Color::Palette16 DEBUG_COLOR      = ftxui::Color::Palette16::Cyan;
    static constexpr ftxui::Color::Palette16 INFO_COLOR       = ftxui::Color::Palette16::Green;
    static constexpr ftxui::Color::Palette16 WARN_COLOR       = ftxui::Color::Palette16::Yellow;
    static constexpr ftxui::Color::Palette16 ERROR_COLOR      = ftxui::Color::Palette16::Red;
    static constexpr ftxui::Color::Palette16 DEBUG_INFO_COLOR = ftxui::Color::Palette16::GrayLight;

    static constexpr CStr TIMESTAMP_FORMAT = "{:%X}";
    static constexpr CStr LOG_FORMAT       = "{} {} {}";

#ifndef NDEBUG
    static constexpr CStr DEBUG_INFO_FORMAT = "{}{}{}\n";
    static constexpr CStr FILE_LINE_FORMAT  = "{}:{}";
    static constexpr CStr DEBUG_LINE_PREFIX = "           ╰── ";
#endif
  };

  /**
   * @enum LogLevel
   * @brief Represents different log levels.
   */
  enum class LogLevel : u8 { Debug, Info, Warn, Error };

  /**
   * @brief Directly applies ANSI color codes to text
   * @param text The text to colorize
   * @param color The FTXUI color
   * @return Styled string with ANSI codes
   */
  inline fn Colorize(const String& text, const ftxui::Color::Palette16& color) -> String {
    return String(LogLevelConst::COLOR_CODE_LITERALS[static_cast<int>(color)]) + text + LogLevelConst::RESET_CODE;
  }

  /**
   * @brief Make text bold with ANSI codes
   * @param text The text to make bold
   * @return Bold text
   */
  inline fn Bold(const StringView text) -> String {
    return String(LogLevelConst::BOLD_START) + String(text) + String(LogLevelConst::BOLD_END);
  }

  /**
   * @brief Make text italic with ANSI codes
   * @param text The text to make italic
   * @return Italic text
   */
  inline fn Italic(const StringView text) -> String {
    return String(LogLevelConst::ITALIC_START) + String(text) + String(LogLevelConst::ITALIC_END);
  }

  // Initialize at runtime using the constexpr static values
  // This can't be constexpr itself due to the string operations
  inline const Array<String, 4> LEVEL_INFO = {
    Bold(Colorize(LogLevelConst::DEBUG_STR.data(), LogLevelConst::DEBUG_COLOR)),
    Bold(Colorize(LogLevelConst::INFO_STR.data(), LogLevelConst::INFO_COLOR)),
    Bold(Colorize(LogLevelConst::WARN_STR.data(), LogLevelConst::WARN_COLOR)),
    Bold(Colorize(LogLevelConst::ERROR_STR.data(), LogLevelConst::ERROR_COLOR)),
  };

  /**
   * @brief Returns FTXUI color representation for a log level
   * @param level The log level
   * @return FTXUI color code
   */
  constexpr fn GetLevelColor(const LogLevel level) -> ftxui::Color::Palette16 {
    switch (level) {
      case LogLevel::Debug: return LogLevelConst::DEBUG_COLOR;
      case LogLevel::Info:  return LogLevelConst::INFO_COLOR;
      case LogLevel::Warn:  return LogLevelConst::WARN_COLOR;
      case LogLevel::Error: return LogLevelConst::ERROR_COLOR;
      default:              std::unreachable();
    }
  }

  /**
   * @brief Returns string representation of a log level
   * @param level The log level
   * @return String representation
   */
  constexpr fn GetLevelString(const LogLevel level) -> String {
    switch (level) {
      case LogLevel::Debug: return LogLevelConst::DEBUG_STR.data();
      case LogLevel::Info:  return LogLevelConst::INFO_STR.data();
      case LogLevel::Warn:  return LogLevelConst::WARN_STR.data();
      case LogLevel::Error: return LogLevelConst::ERROR_STR.data();
      default:              std::unreachable();
    }
  }

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
    using std::filesystem::path;

    using Buffer = Array<char, 512>;

    // Dynamic parts (runtime)
    const time_point<system_clock, duration<i64>> now = floor<seconds>(system_clock::now());

    const String timestamp = std::format(LogLevelConst::TIMESTAMP_FORMAT, now);
    const String message   = std::format(fmt, std::forward<Args>(args)...);

    Buffer buffer {};

    Buffer::iterator iter = std::format_to(
      buffer.begin(),
      LogLevelConst::LOG_FORMAT,
      Colorize("[" + timestamp + "]", LogLevelConst::DEBUG_INFO_COLOR),
      LEVEL_INFO[static_cast<usize>(level)],
      message
    );

#ifndef NDEBUG
    iter = std::format_to(
      iter,
      "\n{}",
      Italic(Colorize(
        LogLevelConst::DEBUG_LINE_PREFIX +
          std::format("{}:{}", path(loc.file_name()).lexically_normal().string(), std::to_string(loc.line())),
        LogLevelConst::DEBUG_INFO_COLOR
      ))
    );
#endif

    const usize length = std::distance(buffer.begin(), iter);

    std::println("{}", std::string_view(buffer.data(), length));
  }

  template <typename ErrorType>
  fn LogError(const LogLevel level, const ErrorType& error_obj) {
    using DecayedErrorType = std::decay_t<ErrorType>;

    std::source_location logLocation;
    String               errorMessagePart;

    if constexpr (std::is_same_v<DecayedErrorType, error::DraconisError>) {
      logLocation      = error_obj.location;
      errorMessagePart = error_obj.message;
    } else {
      logLocation = std::source_location::current();
      if constexpr (std::is_base_of_v<std::exception, DecayedErrorType>)
        errorMessagePart = error_obj.what();
      else if constexpr (requires { error_obj.message; })
        errorMessagePart = error_obj.message;
      else
        errorMessagePart = "Unknown error type logged";
    }

    LogImpl(level, logLocation, "{}", errorMessagePart);
  }

#ifndef NDEBUG
  #define debug_log(fmt, ...)                                                                           \
    ::util::logging::LogImpl(                                                                           \
      ::util::logging::LogLevel::Debug, std::source_location::current(), fmt __VA_OPT__(, ) __VA_ARGS__ \
    )
  #define debug_at(error_obj) ::util::logging::LogError(::util::logging::LogLevel::Debug, error_obj);
#else
  #define debug_log(...) ((void)0)
  #define debug_at(...)  ((void)0)
#endif

#define info_log(fmt, ...)                                                                           \
  ::util::logging::LogImpl(                                                                          \
    ::util::logging::LogLevel::Info, std::source_location::current(), fmt __VA_OPT__(, ) __VA_ARGS__ \
  )
#define info_at(error_obj) ::util::logging::LogError(::util::logging::LogLevel::Info, error_obj);

#define warn_log(fmt, ...)                                                                           \
  ::util::logging::LogImpl(                                                                          \
    ::util::logging::LogLevel::Warn, std::source_location::current(), fmt __VA_OPT__(, ) __VA_ARGS__ \
  )
#define warn_at(error_obj) ::util::logging::LogError(::util::logging::LogLevel::Warn, error_obj);

#define error_log(fmt, ...)                                                                           \
  ::util::logging::LogImpl(                                                                           \
    ::util::logging::LogLevel::Error, std::source_location::current(), fmt __VA_OPT__(, ) __VA_ARGS__ \
  )
#define error_at(error_obj) ::util::logging::LogError(::util::logging::LogLevel::Error, error_obj);
} // namespace util::logging
