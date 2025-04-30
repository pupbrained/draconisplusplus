#pragma once

#include <format>
#include <source_location> // std::source_location
#include <system_error>    // std::error_code

#ifdef _WIN32
  #include <winerror.h>   // error values
  #include <winrt/base.h> // winrt::hresult_error
#endif

#include "src/core/util/types.hpp"

namespace util::error {
  using types::u8, types::i32, types::String, types::StringView, types::Exception;

  /**
   * @enum DraconisErrorCode
   * @brief Error codes for general OS-level operations.
   */
  enum class DraconisErrorCode : u8 {
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
   * @struct DraconisError
   * @brief Holds structured information about an OS-level error.
   *
   * Used as the error type in Result for many os:: functions.
   */
  struct DraconisError {
    // ReSharper disable CppDFANotInitializedField
    String               message;  ///< A descriptive error message, potentially including platform details.
    DraconisErrorCode    code;     ///< The general category of the error.
    std::source_location location; ///< The source location where the error occurred (file, line, function).
    // ReSharper restore CppDFANotInitializedField

    DraconisError(
      const DraconisErrorCode     errc,
      String                      msg,
      const std::source_location& loc = std::source_location::current()
    )
      : message(std::move(msg)), code(errc), location(loc) {}

    explicit DraconisError(const Exception& exc, const std::source_location& loc = std::source_location::current())
      : message(exc.what()), code(DraconisErrorCode::InternalError), location(loc) {}

    explicit DraconisError(
      const std::error_code&      errc,
      const std::source_location& loc = std::source_location::current()
    )
      : message(errc.message()), location(loc) {
      using enum DraconisErrorCode;
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
    explicit DraconisError(const winrt::hresult_error& e) : message(winrt::to_string(e.message())) {
      switch (e.code()) {
        case E_ACCESSDENIED:                              code = DraconisErrorCode::PermissionDenied; break;
        case HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND):
        case HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND):
        case HRESULT_FROM_WIN32(ERROR_SERVICE_NOT_FOUND): code = DraconisErrorCode::NotFound; break;
        case HRESULT_FROM_WIN32(ERROR_TIMEOUT):
        case HRESULT_FROM_WIN32(ERROR_SEM_TIMEOUT):       code = DraconisErrorCode::Timeout; break;
        case HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED):     code = DraconisErrorCode::NotSupported; break;
        default:                                          code = DraconisErrorCode::PlatformSpecific; break;
      }
    }
#else
    DraconisError(const DraconisErrorCode code_hint, const int errno_val)
      : message(std::system_category().message(errno_val)), code(code_hint) {
      using enum DraconisErrorCode;

      switch (errno_val) {
        case EACCES:    code = PermissionDenied; break;
        case ENOENT:    code = NotFound; break;
        case ETIMEDOUT: code = Timeout; break;
        case ENOTSUP:   code = NotSupported; break;
        default:        code = PlatformSpecific; break;
      }
    }

    static auto withErrno(const String& context, const std::source_location& loc = std::source_location::current())
      -> DraconisError {
      const i32    errNo   = errno;
      const String msg     = std::system_category().message(errNo);
      const String fullMsg = std::format("{}: {}", context, msg);

      DraconisErrorCode code = DraconisErrorCode::PlatformSpecific;
      switch (errNo) {
        case EACCES:
        case EPERM:        code = DraconisErrorCode::PermissionDenied; break;
        case ENOENT:       code = DraconisErrorCode::NotFound; break;
        case ETIMEDOUT:    code = DraconisErrorCode::Timeout; break;
        case ENOTSUP:      code = DraconisErrorCode::NotSupported; break;
        case EIO:          code = DraconisErrorCode::IoError; break;
        case ECONNREFUSED:
        case ENETDOWN:
        case ENETUNREACH:  code = DraconisErrorCode::NetworkError; break;
        default:           code = DraconisErrorCode::PlatformSpecific; break;
      }

      return DraconisError { code, fullMsg, loc };
    }
#endif
  };
} // namespace util::error
