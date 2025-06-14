#pragma once

#ifdef _WIN32
  #include <stdlib.h> // NOLINT(*-deprecated-headers)
#endif

#include "Definitions.hpp"
#include "Error.hpp"
#include "Types.hpp"

namespace util::helpers {
  /**
   * @brief Safely retrieves an environment variable.
   * @param name  The name of the environment variable to retrieve.
   * @return A Result containing the value of the environment variable as a String,
   * or an EnvError if an error occurred.
   */
  [[nodiscard]] inline fn GetEnv(types::CStr name) -> types::Result<types::String> {
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
} // namespace util::helpers
