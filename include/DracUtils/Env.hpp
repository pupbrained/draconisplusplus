#pragma once

#ifdef _WIN32
  #include <stdlib.h> // NOLINT(*-deprecated-headers)
#endif

#include "Definitions.hpp"
#include "Error.hpp"
#include "Types.hpp"

namespace draconis::utils::env {
  /**
   * @brief Safely retrieves an environment variable.
   * @param name  The name of the environment variable to retrieve.
   * @return A Result containing the value of the environment variable as a CStr.
   */
  [[nodiscard]] inline fn GetEnv(types::CStr name) -> types::Result<types::CStr> {
#ifdef _WIN32
    char*        rawPtr     = nullptr;
    types::usize bufferSize = 0;

    // Use _dupenv_s to safely retrieve environment variables on Windows
    const types::i32 err = _dupenv_s(&rawPtr, &bufferSize, name);

    const types::UniquePointer<char, decltype(&free)> ptrManager(rawPtr, free);

    if (err != 0)
      return types::Err(error::DracError(error::DracErrorCode::PermissionDenied, "Failed to retrieve environment variable"));

    if (!ptrManager)
      return types::Err(error::DracError(error::DracErrorCode::NotFound, "Environment variable not found"));

    return ptrManager.get();
#else
    // Use std::getenv to retrieve environment variables on POSIX systems
    const types::CStr value = std::getenv(name);

    if (!value)
      return types::Err(error::DracError(error::DracErrorCode::NotFound, "Environment variable not found"));

    return value;
#endif
  }

#ifdef _WIN32
  /**
   * @brief Safely retrieves an environment variable as a wstring.
   * @param name  The name of the environment variable to retrieve.
   * @return A Result containing the value of the environment variable as a wstring.
   */
  [[nodiscard]] inline fn GetEnvW(const wchar_t* name) -> types::Result<wchar_t*> {
    wchar_t*     rawPtr     = nullptr;
    types::usize bufferSize = 0;

    const types::i32 err = _wdupenv_s(&rawPtr, &bufferSize, name);

    const types::UniquePointer<wchar_t, decltype(&free)> ptrManager(rawPtr, free);

    if (err != 0)
      return types::Err(error::DracError(error::DracErrorCode::PermissionDenied, "Failed to retrieve environment variable"));

    if (!ptrManager)
      return types::Err(error::DracError(error::DracErrorCode::NotFound, "Environment variable not found"));

    return ptrManager.get();
  }
#endif
} // namespace draconis::utils::env
