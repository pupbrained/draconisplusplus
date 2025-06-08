#pragma once

#ifdef _WIN32
  #include <stdlib.h> // NOLINT(*-deprecated-headers)
#endif

#include "Util/Definitions.hpp"
#include "Util/Error.hpp"
#include "Util/Types.hpp"

namespace util::helpers {
  using types::Result, types::String, types::CStr;

  /**
   * @brief Safely retrieves an environment variable.
   * @param name  The name of the environment variable to retrieve.
   * @return A Result containing the value of the environment variable as a String,
   * or an EnvError if an error occurred.
   */
  [[nodiscard]] inline fn GetEnv(CStr name) -> Result<String> {
    using error::DracError, error::DracErrorCode;
    using types::Err;

#ifdef _WIN32
    using types::i32, types::usize, types::UniquePointer;

    char* rawPtr     = nullptr;
    usize bufferSize = 0;

    // Use _dupenv_s to safely retrieve environment variables on Windows
    const i32 err = _dupenv_s(&rawPtr, &bufferSize, name);

    const UniquePointer<char, decltype(&free)> ptrManager(rawPtr, free);

    if (err != 0)
      return Err(DracError(DracErrorCode::PermissionDenied, "Failed to retrieve environment variable"));

    if (!ptrManager)
      return Err(DracError(DracErrorCode::NotFound, "Environment variable not found"));

    return ptrManager.get();
#else
    // Use std::getenv to retrieve environment variables on POSIX systems
    const CStr value = std::getenv(name);

    if (!value)
      return Err(DracError(DracErrorCode::NotFound, "Environment variable not found"));

    return value;
#endif
  }
} // namespace util::helpers
