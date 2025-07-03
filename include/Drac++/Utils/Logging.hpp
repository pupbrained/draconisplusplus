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

#include "Error.hpp"
#include "Types.hpp"

namespace draconis::utils::logging {
  namespace {
    using types::Array;
    using types::LockGuard;
    using types::Mutex;
    using types::PCStr;
    using types::String;
    using types::StringView;
    using types::u64;
    using types::u8;
    using types::usize;
  } // namespace

  inline fn GetLogMutex() -> Mutex& {
    static Mutex LogMutexInstance;
    return LogMutexInstance;
  }

  struct LogLevelConst {
    // clang-format off
    static constexpr Array<StringView, 16> COLOR_CODE_LITERALS = {
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

    static constexpr StringView DEBUG_STR = "DEBUG";
    static constexpr StringView INFO_STR  = "INFO ";
    static constexpr StringView WARN_STR  = "WARN ";
    static constexpr StringView ERROR_STR = "ERROR";

    static constexpr ftxui::Color::Palette16 DEBUG_COLOR      = ftxui::Color::Palette16::Cyan;
    static constexpr ftxui::Color::Palette16 INFO_COLOR       = ftxui::Color::Palette16::Green;
    static constexpr ftxui::Color::Palette16 WARN_COLOR       = ftxui::Color::Palette16::Yellow;
    static constexpr ftxui::Color::Palette16 ERROR_COLOR      = ftxui::Color::Palette16::Red;
    static constexpr ftxui::Color::Palette16 DEBUG_INFO_COLOR = ftxui::Color::Palette16::GrayLight;

    static constexpr PCStr TIMESTAMP_FORMAT = "%X";
    static constexpr PCStr LOG_FORMAT       = "{} {} {}";

#ifndef NDEBUG
    static constexpr PCStr DEBUG_INFO_FORMAT = "{}{}{}\n";
    static constexpr PCStr FILE_LINE_FORMAT  = "{}:{}";
    static constexpr PCStr DEBUG_LINE_PREFIX = "           ╰──── ";
#endif
  };

  /**
   * @enum LogLevel
   * @brief Represents different log levels.
   */
  enum class LogLevel : u8 {
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
  inline fn Colorize(const StringView text, const ftxui::Color::Palette16& color) -> String {
    return std::format("{}{}{}", LogLevelConst::COLOR_CODE_LITERALS.at(static_cast<usize>(color)), text, LogLevelConst::RESET_CODE);
  }

  /**
   * @brief Make text bold with ANSI codes
   * @param text The text to make bold
   * @return Bold text
   */
  inline fn Bold(const StringView text) -> String {
    return std::format("{}{}{}", LogLevelConst::BOLD_START, text, LogLevelConst::BOLD_END);
  }

  /**
   * @brief Make text italic with ANSI codes
   * @param text The text to make italic
   * @return Italic text
   */
  inline fn Italic(const StringView text) -> String {
    return std::format("{}{}{}", LogLevelConst::ITALIC_START, text, LogLevelConst::ITALIC_END);
  }

  /**
   * @brief Returns the pre-formatted and styled log level strings.
   * @note Uses function-local static for lazy initialization to avoid
   * static initialization order issues and CERT-ERR58-CPP warnings.
   */
  inline fn GetLevelInfo() -> const Array<String, 4>& {
    static const Array<String, 4> LEVEL_INFO_INSTANCE = {
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
  constexpr fn GetLevelString(const LogLevel level) -> StringView {
    using namespace matchit;
    using enum LogLevel;

    return match(level)(
      is | Debug = LogLevelConst::DEBUG_STR,
      is | Info  = LogLevelConst::INFO_STR,
      is | Warn  = LogLevelConst::WARN_STR,
      is | Error = LogLevelConst::ERROR_STR
    );
  }

  /**
   * @brief Helper function to print formatted text with automatic std::print/std::cout selection
   * @tparam Args Parameter pack for format arguments
   * @param fmt The format string
   * @param args The arguments for the format string
   */
  template <typename... Args>
  inline fn Print(std::format_string<Args...> fmt, Args&&... args) {
#ifdef __cpp_lib_print
    std::print(fmt, std::forward<Args>(args)...);
#else
    std::cout << std::format(fmt, std::forward<Args>(args)...);
#endif
  }

  /**
   * @brief Helper function to print pre-formatted text with automatic std::print/std::cout selection
   * @param text The pre-formatted text to print
   */
  inline fn Print(const StringView text) {
#ifdef __cpp_lib_print
    std::print("{}", text);
#else
    std::cout << text;
#endif
  }

  /**
   * @brief Helper function to print formatted text with newline with automatic std::print/std::cout selection
   * @tparam Args Parameter pack for format arguments
   * @param fmt The format string
   * @param args The arguments for the format string
   */
  template <typename... Args>
  inline fn Println(std::format_string<Args...> fmt, Args&&... args) {
#ifdef __cpp_lib_print
    std::println(fmt, std::forward<Args>(args)...);
#else
    std::cout << std::format(fmt, std::forward<Args>(args)...) << '\n';
#endif
  }

  /**
   * @brief Helper function to print pre-formatted text with newline with automatic std::print/std::cout selection
   * @param text The pre-formatted text to print
   */
  inline fn Println(const StringView text) {
#ifdef __cpp_lib_print
    std::println("{}", text);
#else
    std::cout << text << '\n';
#endif
  }

  /**
   * @brief Helper function to print just a newline with automatic std::print/std::cout selection
   */
  inline fn Println() {
#ifdef __cpp_lib_print
    std::println();
#else
    std::cout << '\n';
#endif
  }

  /**
   * @brief Logs a message with the specified log level, source location, and format string.
   * @tparam Args Parameter pack for format arguments.
   * @param level The log level (DEBUG, INFO, WARN, ERROR).
   * @param loc The source location of the log message (only in Debug builds).
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

    const LockGuard lock(GetLogMutex());

    const auto        nowTp = system_clock::now();
    const std::time_t nowTt = system_clock::to_time_t(nowTp);
    std::tm           localTm {};

    String timestamp;

#ifdef _WIN32
    if (localtime_s(&localTm, &nowTt) == 0) {
#else
    if (localtime_r(&nowTt, &localTm) != nullptr) {
#endif
      Array<char, 64> timeBuffer {};

      const usize formattedTime =
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

    const String message = std::format(fmt, std::forward<Args>(args)...);

    const String mainLogLine = std::format(
      LogLevelConst::LOG_FORMAT,
      Colorize(String("[") + timestamp + "]", LogLevelConst::DEBUG_INFO_COLOR),
      GetLevelInfo().at(static_cast<usize>(level)),
      message
    );

    Println(mainLogLine);

#ifndef NDEBUG
    const String fileLine      = std::format(LogLevelConst::FILE_LINE_FORMAT, path(loc.file_name()).lexically_normal().string(), loc.line());
    const String fullDebugLine = std::format("{}{}", LogLevelConst::DEBUG_LINE_PREFIX, fileLine);
    Print(Italic(Colorize(fullDebugLine, LogLevelConst::DEBUG_INFO_COLOR)));
    Println(LogLevelConst::RESET_CODE);
#else
    Print(LogLevelConst::RESET_CODE);
#endif
  }

  template <typename ErrorType>
  fn LogError(const LogLevel level, const ErrorType& error_obj) {
    using DecayedErrorType = std::decay_t<ErrorType>;

#ifndef NDEBUG
    std::source_location logLocation;
#endif

    String errorMessagePart;

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

#define debug_at(error_obj) ::draconis::utils::logging::LogError(::draconis::utils::logging::LogLevel::Debug, error_obj)
#define info_at(error_obj)  ::draconis::utils::logging::LogError(::draconis::utils::logging::LogLevel::Info, error_obj)
#define warn_at(error_obj)  ::draconis::utils::logging::LogError(::draconis::utils::logging::LogLevel::Warn, error_obj)
#define error_at(error_obj) ::draconis::utils::logging::LogError(::draconis::utils::logging::LogLevel::Error, error_obj)

#ifdef NDEBUG
  #define debug_log(fmt, ...) ::draconis::utils::logging::LogImpl(::draconis::utils::logging::LogLevel::Debug, fmt __VA_OPT__(, ) __VA_ARGS__)
  #define info_log(fmt, ...)  ::draconis::utils::logging::LogImpl(::draconis::utils::logging::LogLevel::Info, fmt __VA_OPT__(, ) __VA_ARGS__)
  #define warn_log(fmt, ...)  ::draconis::utils::logging::LogImpl(::draconis::utils::logging::LogLevel::Warn, fmt __VA_OPT__(, ) __VA_ARGS__)
  #define error_log(fmt, ...) ::draconis::utils::logging::LogImpl(::draconis::utils::logging::LogLevel::Error, fmt __VA_OPT__(, ) __VA_ARGS__)
#else
  #define debug_log(fmt, ...) \
    ::draconis::utils::logging::LogImpl(::draconis::utils::logging::LogLevel::Debug, std::source_location::current(), fmt __VA_OPT__(, ) __VA_ARGS__)
  #define info_log(fmt, ...) \
    ::draconis::utils::logging::LogImpl(::draconis::utils::logging::LogLevel::Info, std::source_location::current(), fmt __VA_OPT__(, ) __VA_ARGS__)
  #define warn_log(fmt, ...) \
    ::draconis::utils::logging::LogImpl(::draconis::utils::logging::LogLevel::Warn, std::source_location::current(), fmt __VA_OPT__(, ) __VA_ARGS__)
  #define error_log(fmt, ...) \
    ::draconis::utils::logging::LogImpl(::draconis::utils::logging::LogLevel::Error, std::source_location::current(), fmt __VA_OPT__(, ) __VA_ARGS__)
#endif
} // namespace draconis::utils::logging
