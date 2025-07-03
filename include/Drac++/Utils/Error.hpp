#pragma once

#include <matchit.hpp>     // matchit::{match, is, or_, _}
#include <source_location> // std::source_location

#ifdef _WIN32
  // ReSharper disable once CppUnusedIncludeDirective
  #include <guiddef.h>    // GUID
  #include <winerror.h>   // error values
  #include <winrt/base.h> // winrt::hresult_error
#endif

#include "Types.hpp"

namespace draconis::utils::error {
  namespace {
    using types::String;
    using types::u32;
    using types::u8;
  } // namespace

  /**
   * @enum DracErrorCode
   * @brief Error codes for general OS-level operations.
   */
  enum class DracErrorCode : u8 {
    ApiUnavailable,     ///< A required OS service/API is unavailable or failed unexpectedly at runtime.
    InternalError,      ///< An error occurred within the application's OS abstraction code logic.
    InvalidArgument,    ///< An invalid argument was passed to a function or method.
    IoError,            ///< General I/O error (filesystem, pipes, etc.).
    NetworkError,       ///< A network-related error occurred (e.g., DNS resolution, connection failure).
    NotFound,           ///< A required resource (file, registry key, device, API endpoint) was not found.
    NotSupported,       ///< The requested operation is not supported on this platform, version, or configuration.
    Other,              ///< A generic or unclassified error originating from the OS or an external library.
    OutOfMemory,        ///< The system ran out of memory or resources to complete the operation.
    ParseError,         ///< Failed to parse data obtained from the OS (e.g., file content, API output).
    PermissionDenied,   ///< Insufficient permissions to perform the operation.
    PlatformSpecific,   ///< An unmapped error specific to the underlying OS platform occurred (check message).
    Timeout,            ///< An operation timed out (e.g., waiting for IPC reply).
    ResourceExhausted,  ///< System resource limit reached (not memory).
    CorruptedData,      ///< Data present but corrupt or inconsistent.
    UnavailableFeature, ///< Feature not present on this hardware/OS.
    ConfigurationError, ///< Configuration or environment issue.
    PermissionRequired, ///< Operation requires elevated privileges.
  };

  /**
   * @struct DracError
   * @brief Holds structured information about an OS-level error.
   *
   * Used as the error type in Result for many os:: functions.
   */
  struct DracError {
    String               message;  ///< A descriptive error message, potentially including platform details.
    std::source_location location; ///< The source location where the error occurred (file, line, function).
    DracErrorCode        code;     ///< The general category of the error.

    DracError(const DracErrorCode errc, String msg, const std::source_location& loc = std::source_location::current())
      : message(std::move(msg)), location(loc), code(errc) {}

#ifdef _WIN32
    explicit DracError(const winrt::hresult_error& err)
      : message(winrt::to_string(err.message())) {
      using matchit::match, matchit::is, matchit::or_, matchit::_;
      using enum DracErrorCode;

      auto fromWin32 = [](const u32 win32) -> HRESULT { return HRESULT_FROM_WIN32(win32); };

      code = match(err.code())(
        is | or_(E_ACCESSDENIED, fromWin32(ERROR_ACCESS_DENIED)) = PermissionDenied,
        is | fromWin32(ERROR_FILE_NOT_FOUND)                     = NotFound,
        is | fromWin32(ERROR_PATH_NOT_FOUND)                     = NotFound,
        is | fromWin32(ERROR_SERVICE_NOT_FOUND)                  = NotFound,
        is | fromWin32(ERROR_TIMEOUT)                            = Timeout,
        is | fromWin32(ERROR_SEM_TIMEOUT)                        = Timeout,
        is | fromWin32(ERROR_NOT_SUPPORTED)                      = NotSupported,
        is | _                                                   = PlatformSpecific
      );
    }
#endif
  };
} // namespace draconis::utils::error

#define ERR(errc, msg)          return ::draconis::utils::types::Err(::draconis::utils::error::DracError(errc, msg))
#define ERR_FMT(errc, fmt, ...) return ::draconis::utils::types::Err(::draconis::utils::error::DracError(errc, std::format(fmt, __VA_ARGS__)))