#pragma once

#include <format>
#include <source_location> // std::source_location
#include <system_error>    // std::error_code

#ifdef _WIN32
  #include <winerror.h>   // error values
  #include <winrt/base.h> // winrt::hresult_error
#endif

#include "src/util/types.hpp"

namespace util::error {
  using types::u8, types::i32, types::String, types::StringView, types::Exception;

  /**
   * @enum DracErrorCode
   * @brief Error codes for general OS-level operations.
   */
  enum class DracErrorCode : u8 {
    ApiUnavailable,   ///< A required OS service/API is unavailable or failed unexpectedly at runtime.
    InternalError,    ///< An error occurred within the application's OS abstraction code logic.
    InvalidArgument,  ///< An invalid argument was passed to a function or method.
    IoError,          ///< General I/O error (filesystem, pipes, etc.).
    NetworkError,     ///< A network-related error occurred (e.g., DNS resolution, connection failure).
    NotFound,         ///< A required resource (file, registry key, device, API endpoint) was not found.
    NotSupported,     ///< The requested operation is not supported on this platform, version, or configuration.
    Other,            ///< A generic or unclassified error originating from the OS or an external library.
    OutOfMemory,      ///< The system ran out of memory or resources to complete the operation.
    ParseError,       ///< Failed to parse data obtained from the OS (e.g., file content, API output).
    PermissionDenied, ///< Insufficient permissions to perform the operation.
    PlatformSpecific, ///< An unmapped error specific to the underlying OS platform occurred (check message).
    Timeout,          ///< An operation timed out (e.g., waiting for IPC reply).
  };

  /**
   * @struct DracError
   * @brief Holds structured information about an OS-level error.
   *
   * Used as the error type in Result for many os:: functions.
   */
  struct DracError {
    // ReSharper disable CppDFANotInitializedField
    String               message;  ///< A descriptive error message, potentially including platform details.
    DracErrorCode        code;     ///< The general category of the error.
    std::source_location location; ///< The source location where the error occurred (file, line, function).
    // ReSharper restore CppDFANotInitializedField

    DracError(const DracErrorCode errc, String msg, const std::source_location& loc = std::source_location::current())
      : message(std::move(msg)), code(errc), location(loc) {}

    explicit DracError(const Exception& exc, const std::source_location& loc = std::source_location::current())
      : message(exc.what()), code(DracErrorCode::InternalError), location(loc) {}

    explicit DracError(const std::error_code& errc, const std::source_location& loc = std::source_location::current())
      : message(errc.message()), location(loc) {
      using enum DracErrorCode;
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
    explicit DracError(const winrt::hresult_error& e) : message(winrt::to_string(e.message())) {
      switch (e.code()) {
        case E_ACCESSDENIED:                              code = DracErrorCode::PermissionDenied; break;
        case HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND):
        case HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND):
        case HRESULT_FROM_WIN32(ERROR_SERVICE_NOT_FOUND): code = DracErrorCode::NotFound; break;
        case HRESULT_FROM_WIN32(ERROR_TIMEOUT):
        case HRESULT_FROM_WIN32(ERROR_SEM_TIMEOUT):       code = DracErrorCode::Timeout; break;
        case HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED):     code = DracErrorCode::NotSupported; break;
        default:                                          code = DracErrorCode::PlatformSpecific; break;
      }
    }
#else
    DracError(const DracErrorCode code_hint, const int errno_val)
      : message(std::system_category().message(errno_val)), code(code_hint) {
      using enum DracErrorCode;

      switch (errno_val) {
        case EACCES:    code = PermissionDenied; break;
        case ENOENT:    code = NotFound; break;
        case ETIMEDOUT: code = Timeout; break;
        case ENOTSUP:   code = NotSupported; break;
        default:        code = PlatformSpecific; break;
      }
    }

    static auto withErrno(const String& context, const std::source_location& loc = std::source_location::current())
      -> DracError {
      const i32    errNo   = errno;
      const String msg     = std::system_category().message(errNo);
      const String fullMsg = std::format("{}: {}", context, msg);

      const DracErrorCode code = [&errNo] {
        switch (errNo) {
          case EACCES:
          case EPERM:        return DracErrorCode::PermissionDenied;
          case ENOENT:       return DracErrorCode::NotFound;
          case ETIMEDOUT:    return DracErrorCode::Timeout;
          case ENOTSUP:      return DracErrorCode::NotSupported;
          case EIO:          return DracErrorCode::IoError;
          case ECONNREFUSED:
          case ENETDOWN:
          case ENETUNREACH:  return DracErrorCode::NetworkError;
          default:           return DracErrorCode::PlatformSpecific;
        }
      }();

      return DracError { code, fullMsg, loc };
    }
#endif
  };
} // namespace util::error
