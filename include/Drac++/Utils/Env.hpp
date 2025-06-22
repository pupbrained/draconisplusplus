#pragma once

#ifdef _WIN32
  #include <stdlib.h> // NOLINT(*-deprecated-headers)
#endif

#include "Definitions.hpp"
#include "Error.hpp"
#include "Types.hpp"

namespace draconis::utils::env {
  namespace {
    using types::CStr;
    using types::Err;
    using types::i32;
    using types::Result;
    using types::UniquePointer;
    using types::usize;

    using error::DracError;
    using enum error::DracErrorCode;
  } // namespace

  /**
   * @brief Safely retrieves an environment variable.
   * @param name The name of the environment variable to retrieve.
   * @return A Result containing the value of the environment variable as a CStr.
   */
  [[nodiscard]] inline fn GetEnv(const CStr name) -> Result<CStr> {
#ifdef _WIN32
    char* rawPtr     = nullptr;
    usize bufferSize = 0;

    // Use _dupenv_s to safely retrieve environment variables on Windows
    const i32 err = _dupenv_s(&rawPtr, &bufferSize, name);

    const UniquePointer<char, decltype(&free)> ptrManager(rawPtr, free);

    if (err != 0)
      return Err(DracError(PermissionDenied, "Failed to retrieve environment variable"));

    if (!ptrManager)
      return Err(DracError(NotFound, "Environment variable not found"));

    return ptrManager.get();
#else
    // Use std::getenv to retrieve environment variables on POSIX systems
    const CStr value = std::getenv(name);

    if (!value)
      return Err(DracError(NotFound, "Environment variable not found"));

    return value;
#endif
  }

#ifdef _WIN32
  /**
   * @brief Safely retrieves an environment variable as a wstring.
   * @param name The name of the environment variable to retrieve.
   * @return A Result containing the value of the environment variable as a wstring.
   */
  [[nodiscard]] inline fn GetEnvW(const wchar_t* name) -> Result<wchar_t*> {
    wchar_t* rawPtr     = nullptr;
    usize    bufferSize = 0;

    const i32 err = _wdupenv_s(&rawPtr, &bufferSize, name);

    const UniquePointer<wchar_t, decltype(&free)> ptrManager(rawPtr, free);

    if (err != 0)
      return Err(DracError(PermissionDenied, "Failed to retrieve environment variable"));

    if (!ptrManager)
      return Err(DracError(NotFound, "Environment variable not found"));

    return ptrManager.get();
  }
#endif
} // namespace draconis::utils::env
