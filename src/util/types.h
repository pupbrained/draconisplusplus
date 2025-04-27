#pragma once

#include <array>           // std::array alias (Array)
#include <cstdlib>         // std::getenv, std::free
#include <expected>        // std::expected alias (Result)
#include <format>          // std::format
#include <map>             // std::map alias (Map)
#include <memory>          // std::shared_ptr and std::unique_ptr aliases (SharedPointer, UniquePointer)
#include <optional>        // std::optional alias (Option)
#include <source_location> // std::source_location
#include <string>          // std::string and std::string_view aliases (String, StringView)
#include <system_error>    // std::error_code and std::system_error
#include <utility>         // std::pair alias (Pair)
#include <variant>         // std::variant alias (NowPlayingError)
#include <vector>          // std::vector alias (Vec)

#ifdef _WIN32
  #include <winrt/base.h> // winrt::hresult_error
#elifdef __linux__
  #include <dbus-cxx.h> // DBus::Error
#endif

//----------------------------------------------------------------//
// Integer Type Aliases                                           //
// Provides concise names for standard fixed-width integer types. //
//----------------------------------------------------------------//

using u8  = std::uint8_t;  ///< 8-bit unsigned integer.
using u16 = std::uint16_t; ///< 16-bit unsigned integer.
using u32 = std::uint32_t; ///< 32-bit unsigned integer.
using u64 = std::uint64_t; ///< 64-bit unsigned integer.

using i8  = std::int8_t;  ///< 8-bit signed integer.
using i16 = std::int16_t; ///< 16-bit signed integer.
using i32 = std::int32_t; ///< 32-bit signed integer.
using i64 = std::int64_t; ///< 64-bit signed integer.

//-----------------------------------------------------------//
// Floating-Point Type Aliases                               //
// Provides concise names for standard floating-point types. //
//-----------------------------------------------------------//

using f32 = float;  ///< 32-bit floating-point number.
using f64 = double; ///< 64-bit floating-point number.

//-------------------------------------------------//
// Size Type Aliases                               //
// Provides concise names for standard size types. //
//-------------------------------------------------//

using usize = std::size_t;    ///< Unsigned size type (result of sizeof).
using isize = std::ptrdiff_t; ///< Signed size type (result of pointer subtraction).

//---------------------------------------------------//
// String Type Aliases                               //
// Provides concise names for standard string types. //
//---------------------------------------------------//

using String     = std::string;      ///< Owning, mutable string.
using StringView = std::string_view; ///< Non-owning view of a string.
using CStr       = const char*;      ///< Pointer to a null-terminated C-style string.

//----------------------------------------------------//
// Standard Library Type Aliases                      //
// Provides concise names for standard library types. //
//----------------------------------------------------//

using Exception = std::exception; ///< Standard exception type.

/**
 * @typedef Result
 * @brief Alias for std::expected<Tp, Er>. Represents a value that can either be
 * a success value of type Tp or an error value of type Er.
 * @tparam Tp The type of the success value.
 * @tparam Er The type of the error value.
 */
template <typename Tp, typename Er>
using Result = std::expected<Tp, Er>;

/**
 * @typedef Err
 * @brief Alias for std::unexpected<Er>. Used to construct a Result in an error state.
 * @tparam Er The type of the error value.
 */
template <typename Er>
using Err = std::unexpected<Er>;

/**
 * @typedef Option
 * @brief Alias for std::optional<Tp>. Represents a value that may or may not be present.
 * @tparam Tp The type of the potential value.
 */
template <typename Tp>
using Option = std::optional<Tp>;

/**
 * @typedef Array
 * @brief Alias for std::array<Tp, sz>. Represents a fixed-size array.
 * @tparam Tp The element type.
 * @tparam sz The size of the array.
 */
template <typename Tp, usize sz>
using Array = std::array<Tp, sz>;

/**
 * @typedef Vec
 * @brief Alias for std::vector<Tp>. Represents a dynamic-size array (vector).
 * @tparam Tp The element type.
 */
template <typename Tp>
using Vec = std::vector<Tp>;

/**
 * @typedef Pair
 * @brief Alias for std::pair<T1, T2>. Represents a pair of values.
 * @tparam T1 The type of the first element.
 * @tparam T2 The type of the second element.
 */
template <typename T1, typename T2>
using Pair = std::pair<T1, T2>;

/**
 * @typedef Map
 * @brief Alias for std::map<Key, Val>. Represents an ordered map (dictionary).
 * @tparam Key The key type.
 * @tparam Val The value type.
 */
template <typename Key, typename Val>
using Map = std::map<Key, Val>;

/**
 * @typedef SharedPointer
 * @brief Alias for std::shared_ptr<Tp>. Manages shared ownership of a dynamically allocated object.
 * @tparam Tp The type of the managed object.
 */
template <typename Tp>
using SharedPointer = std::shared_ptr<Tp>;

/**
 * @typedef UniquePointer
 * @brief Alias for std::unique_ptr<Tp, Dp>. Manages unique ownership of a dynamically allocated object.
 * @tparam Tp The type of the managed object.
 * @tparam Dp The deleter type (defaults to std::default_delete<Tp>).
 */
template <typename Tp, typename Dp = std::default_delete<Tp>>
using UniquePointer = std::unique_ptr<Tp, Dp>;

//--------------------------------------------------------//
// Application-Specific Type Aliases                      //
// Provides concise names for application-specific types. //
//--------------------------------------------------------//

/**
 * @enum NowPlayingCode
 * @brief Error codes specific to the Now Playing feature.
 */
enum class NowPlayingCode : u8 {
  NoPlayers,      ///< No media players were found (e.g., no MPRIS services on Linux).
  NoActivePlayer, ///< Players were found, but none are currently active or playing.
};

/**
 * @enum OsErrorCode
 * @brief Error codes for general OS-level operations.
 */
enum class OsErrorCode : u8 {
  IoError,          ///< General I/O error (filesystem, pipes, etc.).
  PermissionDenied, ///< Insufficient permissions to perform the operation.
  NotFound,         ///< A required resource (file, registry key, device, API endpoint) was not found.
  ParseError,       ///< Failed to parse data obtained from the OS (e.g., file content, API output).
  ApiUnavailable,   ///< A required OS service/API is unavailable or failed unexpectedly at runtime.
  NotSupported,     ///< The requested operation is not supported on this platform, version, or configuration.
  Timeout,          ///< An operation timed out (e.g., waiting for IPC reply).
  BufferTooSmall,   ///< Optional: Keep if using fixed C-style buffers, otherwise remove.
  InternalError,    ///< An error occurred within the application's OS abstraction code logic.
  NetworkError,     ///< A network-related error occurred (e.g., DNS resolution, connection failure).
  PlatformSpecific, ///< An unmapped error specific to the underlying OS platform occurred (check message).
  Other,            ///< A generic or unclassified error originating from the OS or an external library.
};

/**
 * @struct OsError
 * @brief Holds structured information about an OS-level error.
 *
 * Used as the error type in Result for many os:: functions.
 */
struct OsError {
  // ReSharper disable CppDFANotInitializedField
  String               message;  ///< A descriptive error message, potentially including platform details.
  OsErrorCode          code;     ///< The general category of the error.
  std::source_location location; ///< The source location where the error occurred (file, line, function).
  // ReSharper restore CppDFANotInitializedField

  OsError(const OsErrorCode errc, String msg, const std::source_location& loc = std::source_location::current())
    : message(std::move(msg)), code(errc), location(loc) {}

  explicit OsError(const Exception& exc, const std::source_location& loc = std::source_location::current())
    : message(exc.what()), code(OsErrorCode::InternalError), location(loc) {}

  explicit OsError(const std::error_code& errc, const std::source_location& loc = std::source_location::current())
    : message(errc.message()), location(loc) {
    using enum OsErrorCode;
    using enum std::errc;

    switch (static_cast<std::errc>(errc.value())) {
      case permission_denied:         code = PermissionDenied; break;
      case no_such_file_or_directory: code = NotFound; break;
      case timed_out:                 code = Timeout; break;
      case io_error:                  code = IoError; break;
      case network_unreachable:
      case network_down:
      case connection_refused:        code = NetworkError; break;
      case not_supported:             code = NotSupported; break;
      default:                        code = errc.category() == std::generic_category() ? InternalError : PlatformSpecific; break;
    }
  }
#ifdef _WIN32
  explicit OsError(const winrt::hresult_error& e) : message(winrt::to_string(e.message())) {
    switch (e.code()) {
      case E_ACCESSDENIED:                              code = OsErrorCode::PermissionDenied; break;
      case HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND):
      case HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND):
      case HRESULT_FROM_WIN32(ERROR_SERVICE_NOT_FOUND): code = OsErrorCode::NotFound; break;
      case HRESULT_FROM_WIN32(ERROR_TIMEOUT):
      case HRESULT_FROM_WIN32(ERROR_SEM_TIMEOUT):       code = OsErrorCode::Timeout; break;
      case HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED):     code = OsErrorCode::NotSupported; break;
      default:                                          code = OsErrorCode::PlatformSpecific; break;
    }
  }
#else
  OsError(const OsErrorCode code_hint, const int errno_val)
    : message(std::system_category().message(errno_val)), code(code_hint) {
    using enum OsErrorCode;

    switch (errno_val) {
      case EACCES:    code = PermissionDenied; break;
      case ENOENT:    code = NotFound; break;
      case ETIMEDOUT: code = Timeout; break;
      case ENOTSUP:   code = NotSupported; break;
      default:        code = PlatformSpecific; break;
    }
  }

  static auto withErrno(const String& context, const std::source_location& loc = std::source_location::current())
    -> OsError {
    const i32    errNo   = errno;
    const String msg     = std::system_category().message(errNo);
    const String fullMsg = std::format("{}: {}", context, msg);

    OsErrorCode code;
    switch (errNo) {
      case EACCES:
      case EPERM:        code = OsErrorCode::PermissionDenied; break;
      case ENOENT:       code = OsErrorCode::NotFound; break;
      case ETIMEDOUT:    code = OsErrorCode::Timeout; break;
      case ENOTSUP:      code = OsErrorCode::NotSupported; break;
      case EIO:          code = OsErrorCode::IoError; break;
      case ECONNREFUSED:
      case ENETDOWN:
      case ENETUNREACH:  code = OsErrorCode::NetworkError; break;
      default:           code = OsErrorCode::PlatformSpecific; break;
    }

    return OsError { code, fullMsg, loc };
  }

  #ifdef __linux__
  static auto fromDBus(const DBus::Error& err, const std::source_location& loc = std::source_location::current())
    -> OsError {
    String      name     = err.name();
    OsErrorCode codeHint = OsErrorCode::PlatformSpecific;
    String      message;

    using namespace std::string_view_literals;

    if (name == "org.freedesktop.DBus.Error.ServiceUnknown"sv ||
        name == "org.freedesktop.DBus.Error.NameHasNoOwner"sv) {
      codeHint = OsErrorCode::NotFound;
      message  = std::format("DBus service/name not found: {}", err.message());
    } else if (name == "org.freedesktop.DBus.Error.NoReply"sv || name == "org.freedesktop.DBus.Error.Timeout"sv) {
      codeHint = OsErrorCode::Timeout;
      message  = std::format("DBus timeout/no reply: {}", err.message());
    } else if (name == "org.freedesktop.DBus.Error.AccessDenied"sv) {
      codeHint = OsErrorCode::PermissionDenied;
      message  = std::format("DBus access denied: {}", err.message());
    } else {
      message = std::format("DBus error: {} - {}", name, err.message());
    }

    return OsError { codeHint, message, loc };
  }
  #endif
#endif
};

/**
 * @struct DiskSpace
 * @brief Represents disk usage information.
 *
 * Used as the success type for os::GetDiskUsage.
 */
struct DiskSpace {
  u64 used_bytes;  ///< Currently used disk space in bytes.
  u64 total_bytes; ///< Total disk space in bytes.
};

/**
 * @struct MediaInfo
 * @brief Holds structured metadata about currently playing media.
 *
 * Used as the success type for os::GetNowPlaying.
 * Using Option<> for fields that might not always be available.
 */
struct MediaInfo {
  Option<String> title;    ///< Track title.
  Option<String> artist;   ///< Track artist(s).
  Option<String> album;    ///< Album name.
  Option<String> app_name; ///< Name of the media player application (e.g., "Spotify", "Firefox").

  MediaInfo() = default;

  MediaInfo(Option<String> title, Option<String> artist) : title(std::move(title)), artist(std::move(artist)) {}

  MediaInfo(Option<String> title, Option<String> artist, Option<String> album, Option<String> app)
    : title(std::move(title)), artist(std::move(artist)), album(std::move(album)), app_name(std::move(app)) {}
};

//--------------------------------------------------------//
// Potentially Update Existing Application-Specific Types //
//--------------------------------------------------------//

/**
 * @typedef NowPlayingError (Updated Recommendation)
 * @brief Represents the possible errors returned by os::GetNowPlaying.
 *
 * It's a variant that can hold either a specific NowPlayingCode
 * (indicating player state like 'no active player') or a general OsError
 * (indicating an underlying system/API failure).
 */
using NowPlayingError = std::variant<NowPlayingCode, OsError>;

/**
 * @enum EnvError
 * @brief Error codes for environment variable retrieval.
 */
enum class EnvError : u8 {
  NotFound,    ///< Environment variable not found.
  AccessError, ///< Access error when trying to retrieve the variable.
};

/**
 * @brief Safely retrieves an environment variable.
 * @param name  The name of the environment variable to retrieve.
 * @return A Result containing the value of the environment variable as a String,
 * or an EnvError if an error occurred.
 */
[[nodiscard]] inline auto GetEnv(CStr name) -> Result<String, EnvError> {
#ifdef _WIN32
  char* rawPtr     = nullptr;
  usize bufferSize = 0;

  // Use _dupenv_s to safely retrieve environment variables on Windows
  const i32 err = _dupenv_s(&rawPtr, &bufferSize, name);

  const UniquePointer<char, decltype(&free)> ptrManager(rawPtr, free);

  if (err != 0)
    return Err(EnvError::AccessError); // Error retrieving environment variable

  if (!ptrManager)
    return Err(EnvError::NotFound); // Environment variable not found

  return ptrManager.get();
#else
  // Use std::getenv to retrieve environment variables on POSIX systems
  const CStr value = std::getenv(name);

  if (!value)
    return Err(EnvError::NotFound); // Environment variable not found

  return value;
#endif
}
