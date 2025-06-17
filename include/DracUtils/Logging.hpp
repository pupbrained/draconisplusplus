#pragma once

#include <chrono>                 // std::chrono::{days, floor, seconds, system_clock}
#include <ctime>                  // localtime_r/s, strftime, time_t, tm
#include <filesystem>             // std::filesystem::path
#include <format>                 // std::format
#include <ftxui/screen/color.hpp> // ftxui::Color
#include <utility>                // std::forward

#ifdef __cpp_lib_print
  #include <print> // std::print
#else
  #include <iostream> // std::cout
#endif

#ifndef NDEBUG
  #include <source_location> // std::source_location
#endif

#include "Definitions.hpp"
#include "Error.hpp"
#include "Types.hpp"

namespace util::logging {

  inline fn GetLogMutex() -> types::Mutex& {
    static types::Mutex LogMutexInstance;
    return LogMutexInstance;
  }

  struct LogLevelConst {
    // clang-format off
    static constexpr types::Array<types::StringView, 16> COLOR_CODE_LITERALS = {
      "\033[38;5;0m",  "\033[38;5;1m",  "\033[38;5;2m",  "\033[38;5;3m",
      "\033[38;5;4m",  "\033[38;5;5m",  "\033[38;5;6m",  "\033[38;5;7m",
      "\033[38;5;8m",  "\033[38;5;9m",  "\033[38;5;10m", "\033[38;5;11m",
      "\033[38;5;12m", "\033[38;5;13m", "\033[38;5;14m", "\033[38;5;15m",
    };
    // clang-format on

    static constexpr const char* RESET_CODE   = "\033[0m";
    static constexpr const char* BOLD_START   = "\033[1m";
    static constexpr const char* BOLD_END     = "\033[22m";
    static constexpr const char* ITALIC_START = "\033[3m";
    static constexpr const char* ITALIC_END   = "\033[23m";

    static constexpr types::StringView DEBUG_STR = "DEBUG";
    static constexpr types::StringView INFO_STR  = "INFO ";
    static constexpr types::StringView WARN_STR  = "WARN ";
    static constexpr types::StringView ERROR_STR = "ERROR";

    static constexpr ftxui::Color::Palette16 DEBUG_COLOR      = ftxui::Color::Palette16::Cyan;
    static constexpr ftxui::Color::Palette16 INFO_COLOR       = ftxui::Color::Palette16::Green;
    static constexpr ftxui::Color::Palette16 WARN_COLOR       = ftxui::Color::Palette16::Yellow;
    static constexpr ftxui::Color::Palette16 ERROR_COLOR      = ftxui::Color::Palette16::Red;
    static constexpr ftxui::Color::Palette16 DEBUG_INFO_COLOR = ftxui::Color::Palette16::GrayLight;

    static constexpr types::CStr TIMESTAMP_FORMAT = "%X";
    static constexpr types::CStr LOG_FORMAT       = "{} {} {}";

#ifndef NDEBUG
    static constexpr types::CStr DEBUG_INFO_FORMAT = "{}{}{}\n";
    static constexpr types::CStr FILE_LINE_FORMAT  = "{}:{}";
    static constexpr types::CStr DEBUG_LINE_PREFIX = "           ╰──── ";
#endif
  };

  /**
   * @enum LogLevel
   * @brief Represents different log levels.
   */
  enum class LogLevel : types::u8 {
    Debug,
    Info,
    Warn,
    Error,
  };

  inline fn GetRuntimeLogLevel() -> LogLevel& {
    static LogLevel RuntimeLogLevel = LogLevel::Info;
    return RuntimeLogLevel;
  }

  inline fn SetRuntimeLogLevel(const LogLevel level) {
    GetRuntimeLogLevel() = level;
  }

  /**
   * @brief Directly applies ANSI color codes to text
   * @param text The text to colorize
   * @param color The FTXUI color
   * @return Styled string with ANSI codes
   */
  inline fn Colorize(const types::StringView text, const ftxui::Color::Palette16& color) -> types::String {
    return std::format("{}{}{}", LogLevelConst::COLOR_CODE_LITERALS.at(color), text, LogLevelConst::RESET_CODE);
  }

  /**
   * @brief Make text bold with ANSI codes
   * @param text The text to make bold
   * @return Bold text
   */
  inline fn Bold(const types::StringView text) -> types::String {
    return std::format("{}{}{}", LogLevelConst::BOLD_START, text, LogLevelConst::BOLD_END);
  }

  /**
   * @brief Make text italic with ANSI codes
   * @param text The text to make italic
   * @return Italic text
   */
  inline fn Italic(const types::StringView text) -> types::String {
    return std::format("{}{}{}", LogLevelConst::ITALIC_START, text, LogLevelConst::ITALIC_END);
  }

  /**
   * @brief Returns the pre-formatted and styled log level strings.
   * @note Uses function-local static for lazy initialization to avoid
   * static initialization order issues and CERT-ERR58-CPP warnings.
   */
  inline fn GetLevelInfo() -> const types::Array<types::String, 4>& {
    static const types::Array<types::String, 4> LEVEL_INFO_INSTANCE = {
      Bold(Colorize(LogLevelConst::DEBUG_STR, LogLevelConst::DEBUG_COLOR)),
      Bold(Colorize(LogLevelConst::INFO_STR, LogLevelConst::INFO_COLOR)),
      Bold(Colorize(LogLevelConst::WARN_STR, LogLevelConst::WARN_COLOR)),
      Bold(Colorize(LogLevelConst::ERROR_STR, LogLevelConst::ERROR_COLOR)),
    };
    return LEVEL_INFO_INSTANCE;
  }

  /**
   * @brief Returns FTXUI color representation for a log level
   * @param level The log level
   * @return FTXUI color code
   */
  constexpr fn GetLevelColor(const LogLevel level) -> ftxui::Color::Palette16 {
    using namespace matchit;
    using enum LogLevel;

    return match(level)(
      is | Debug = LogLevelConst::DEBUG_COLOR,
      is | Info  = LogLevelConst::INFO_COLOR,
      is | Warn  = LogLevelConst::WARN_COLOR,
      is | Error = LogLevelConst::ERROR_COLOR
    );
  }

  /**
   * @brief Returns string representation of a log level
   * @param level The log level
   * @return String representation
   */
  constexpr fn GetLevelString(const LogLevel level) -> types::StringView {
    using namespace matchit;
    using enum LogLevel;

    return match(level)(
      is | Debug = LogLevelConst::DEBUG_STR,
      is | Info  = LogLevelConst::INFO_STR,
      is | Warn  = LogLevelConst::WARN_STR,
      is | Error = LogLevelConst::ERROR_STR
    );
  }

  // ReSharper disable once CppDoxygenUnresolvedReference
  /**
   * @brief Logs a message with the specified log level, source location, and format string.
   * @tparam Args Parameter pack for format arguments.
   * @param level The log level (DEBUG, INFO, WARN, ERROR).
   * \ifnot NDEBUG
   * @param loc The source location of the log message (only in Debug builds).
   * \endif
   * @param fmt The format string.
   * @param args The arguments for the format string.
   */
  template <typename... Args>
  fn LogImpl(
    const LogLevel level,
#ifndef NDEBUG
    const std::source_location& loc,
#endif
    std::format_string<Args...> fmt,
    Args&&... args
  ) {
    using namespace std::chrono;
    using std::filesystem::path;

    if (level < GetRuntimeLogLevel())
      return;

    const types::LockGuard lock(GetLogMutex());

    const auto        nowTp = system_clock::now();
    const std::time_t nowTt = system_clock::to_time_t(nowTp);
    std::tm           localTm {};

    types::String timestamp;

#ifdef _WIN32
    if (localtime_s(&localTm, &nowTt) == 0) {
#else
    if (localtime_r(&nowTt, &localTm) != nullptr) {
#endif
      types::Array<char, 64> timeBuffer {};

      const types::usize formattedTime =
        std::strftime(timeBuffer.data(), sizeof(timeBuffer), LogLevelConst::TIMESTAMP_FORMAT, &localTm);

      if (formattedTime > 0) {
        timestamp = timeBuffer.data();
      } else {
        try {
          timestamp = std::format("{:%X}", nowTp);
        } catch ([[maybe_unused]] const std::format_error& fmtErr) { timestamp = "??:??:??"; }
      }
    } else
      timestamp = "??:??:??";

    const types::String message = std::format(fmt, std::forward<Args>(args)...);

    const types::String mainLogLine = std::format(
      LogLevelConst::LOG_FORMAT,
      Colorize(types::String("[") + timestamp + "]", LogLevelConst::DEBUG_INFO_COLOR),
      GetLevelInfo().at(static_cast<types::usize>(level)),
      message
    );

#ifdef __cpp_lib_print
    std::print("{}", mainLogLine);
#else
    std::cout << mainLogLine;
#endif

#ifndef NDEBUG
    const types::String fileLine      = std::format(LogLevelConst::FILE_LINE_FORMAT, path(loc.file_name()).lexically_normal().string(), loc.line());
    const types::String fullDebugLine = std::format("{}{}", LogLevelConst::DEBUG_LINE_PREFIX, fileLine);
  #ifdef __cpp_lib_print
    std::print("\n{}", Italic(Colorize(fullDebugLine, LogLevelConst::DEBUG_INFO_COLOR)));
  #else
    std::cout << '\n'
              << Italic(Colorize(fullDebugLine, LogLevelConst::DEBUG_INFO_COLOR));
  #endif
#endif

#ifdef __cpp_lib_print
    std::println("{}", LogLevelConst::RESET_CODE);
#else
    std::cout << LogLevelConst::RESET_CODE << '\n';
#endif
  }

  template <typename ErrorType>
  fn LogError(const LogLevel level, const ErrorType& error_obj) {
    using DecayedErrorType = std::decay_t<ErrorType>;

#ifndef NDEBUG
    std::source_location logLocation;
#endif

    types::String errorMessagePart;

    if constexpr (std::is_same_v<DecayedErrorType, error::DracError>) {
#ifndef NDEBUG
      logLocation = error_obj.location;
#endif
      errorMessagePart = error_obj.message;
    } else {
#ifndef NDEBUG
      logLocation = std::source_location::current();
#endif
      if constexpr (std::is_base_of_v<std::exception, DecayedErrorType>)
        errorMessagePart = error_obj.what();
      else if constexpr (requires { error_obj.message; })
        errorMessagePart = error_obj.message;
      else
        errorMessagePart = "Unknown error type logged";
    }

#ifndef NDEBUG
    LogImpl(level, logLocation, "{}", errorMessagePart);
#else
    LogImpl(level, "{}", errorMessagePart);
#endif
  }

#define debug_at(error_obj) ::util::logging::LogError(::util::logging::LogLevel::Debug, error_obj)
#define info_at(error_obj)  ::util::logging::LogError(::util::logging::LogLevel::Info, error_obj)
#define warn_at(error_obj)  ::util::logging::LogError(::util::logging::LogLevel::Warn, error_obj)
#define error_at(error_obj) ::util::logging::LogError(::util::logging::LogLevel::Error, error_obj)

#ifdef NDEBUG
  #define debug_log(fmt, ...) ::util::logging::LogImpl(::util::logging::LogLevel::Debug, fmt __VA_OPT__(, ) __VA_ARGS__)
  #define info_log(fmt, ...)  ::util::logging::LogImpl(::util::logging::LogLevel::Info, fmt __VA_OPT__(, ) __VA_ARGS__)
  #define warn_log(fmt, ...)  ::util::logging::LogImpl(::util::logging::LogLevel::Warn, fmt __VA_OPT__(, ) __VA_ARGS__)
  #define error_log(fmt, ...) ::util::logging::LogImpl(::util::logging::LogLevel::Error, fmt __VA_OPT__(, ) __VA_ARGS__)
#else
  #define debug_log(fmt, ...) \
    ::util::logging::LogImpl(::util::logging::LogLevel::Debug, std::source_location::current(), fmt __VA_OPT__(, ) __VA_ARGS__)
  #define info_log(fmt, ...) \
    ::util::logging::LogImpl(::util::logging::LogLevel::Info, std::source_location::current(), fmt __VA_OPT__(, ) __VA_ARGS__)
  #define warn_log(fmt, ...) \
    ::util::logging::LogImpl(::util::logging::LogLevel::Warn, std::source_location::current(), fmt __VA_OPT__(, ) __VA_ARGS__)
  #define error_log(fmt, ...) \
    ::util::logging::LogImpl(::util::logging::LogLevel::Error, std::source_location::current(), fmt __VA_OPT__(, ) __VA_ARGS__)
#endif
} // namespace util::logging
