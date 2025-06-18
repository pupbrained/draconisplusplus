#pragma once

#include <expected>        // std::{unexpected, expected}
#include <format>          // std::formatter
#include <matchit.hpp>     // matchit::{match, is, or_, _}
#include <source_location> // std::source_location
#include <system_error>    // std::error_code

#ifdef _WIN32
  // ReSharper disable once CppUnusedIncludeDirective
  #include <guiddef.h>    // GUID
  #include <winerror.h>   // error values
  #include <winrt/base.h> // winrt::hresult_error
#endif

#include "Definitions.hpp"
#include "Types.hpp"

namespace draconis::utils {
  namespace error {
    namespace {
      using types::Exception;
      using types::String;
      using types::u32;
      using types::u8;
    } // namespace

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
      String               message;  ///< A descriptive error message, potentially including platform details.
      std::source_location location; ///< The source location where the error occurred (file, line, function).
      DracErrorCode        code;     ///< The general category of the error.

      DracError(const DracErrorCode errc, String msg, const std::source_location& loc = std::source_location::current())
        : message(std::move(msg)), location(loc), code(errc) {}

      explicit DracError(const Exception& exc, const std::source_location& loc = std::source_location::current())
        : message(exc.what()), location(loc), code(DracErrorCode::InternalError) {}

      explicit DracError(const std::error_code& errc, const std::source_location& loc = std::source_location::current())
        : message(errc.message()), location(loc) {
        using matchit::match, matchit::is, matchit::or_, matchit::_;
        using enum DracErrorCode;
        using enum std::errc;

        code = match(errc)(
          is | or_(file_too_large, io_error)                                                = IoError,
          is | invalid_argument                                                             = InvalidArgument,
          is | not_enough_memory                                                            = OutOfMemory,
          is | or_(address_family_not_supported, operation_not_supported, not_supported)    = NotSupported,
          is | or_(network_unreachable, network_down, connection_refused)                   = NetworkError,
          is | or_(no_such_file_or_directory, not_a_directory, is_a_directory, file_exists) = NotFound,
          is | permission_denied                                                            = PermissionDenied,
          is | timed_out                                                                    = Timeout,
          is | _                                                                            = errc.category() == std::generic_category() ? InternalError : PlatformSpecific
        );
      }

#ifdef _WIN32
      explicit DracError(const winrt::hresult_error& err)
        : message(winrt::to_string(err.message())) {
        using matchit::match, matchit::is, matchit::or_, matchit::_;
        using enum DracErrorCode;

        fn fromWin32 = [](const u32 win32) -> HRESULT { return HRESULT_FROM_WIN32(win32); };

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
#else
      DracError(const String& context, const std::source_location& loc = std::source_location::current())
        : message(std::format("{}: {}", context, std::system_category().message(errno))), location(loc) {
        using matchit::match, matchit::is, matchit::or_, matchit::_;
        using enum DracErrorCode;

        code = match(errno)(
          is | EACCES                                   = PermissionDenied,
          is | ENOENT                                   = NotFound,
          is | ETIMEDOUT                                = Timeout,
          is | ENOTSUP                                  = NotSupported,
          is | EIO                                      = IoError,
          is | or_(ECONNREFUSED, ENETDOWN, ENETUNREACH) = NetworkError,
          is | _                                        = PlatformSpecific
        );
      }
#endif
    };
  } // namespace error

  namespace types {
    /**
     * @typedef Result
     * @brief Alias for std::expected<Tp, Er>. Represents a value that can either be
     * a success value of type Tp or an error value of type Er.
     * @tparam Tp The type of the success value.
     * @tparam Er The type of the error value.
     */
    template <typename Tp = void, typename Er = error::DracError>
    using Result = std::expected<Tp, Er>;

    /**
     * @typedef Err
     * @brief Alias for std::unexpected<Er>. Used to construct a Result in an error state.
     * @tparam Er The type of the error value.
     */
    template <typename Er = error::DracError>
    using Err = std::unexpected<Er>;
  } // namespace types
} // namespace draconis::utils

namespace std {
  template <>
  struct formatter<::draconis::utils::error::DracErrorCode> : formatter<::draconis::utils::types::StringView> {
    template <typename FormatContext>
    fn format(draconis::utils::error::DracErrorCode code, FormatContext& ctx) const {
      using enum draconis::utils::error::DracErrorCode;
      using matchit::match, matchit::is, matchit::or_, matchit::_;

      draconis::utils::types::StringView name = match(code)(
        is | ApiUnavailable   = "ApiUnavailable",
        is | InternalError    = "InternalError",
        is | InvalidArgument  = "InvalidArgument",
        is | IoError          = "IoError",
        is | NetworkError     = "NetworkError",
        is | NotFound         = "NotFound",
        is | NotSupported     = "NotSupported",
        is | Other            = "Other",
        is | OutOfMemory      = "OutOfMemory",
        is | ParseError       = "ParseError",
        is | PermissionDenied = "PermissionDenied",
        is | PlatformSpecific = "PlatformSpecific",
        is | Timeout          = "Timeout",
        is | _                = "Unknown"
      );

      return formatter<draconis::utils::types::StringView>::format(name, ctx);
    }
  };
} // namespace std