#pragma once

#ifdef _WIN32
  #include <stdlib.h> // NOLINT(*-deprecated-headers)
#endif

#include <cstdlib>

#include "Error.hpp"
#include "Types.hpp"

namespace draconis::utils::env {
  namespace {
    using types::i32;
    using types::Result;
    using types::UniquePointer;
    using types::Unit;
    using types::usize;

    using enum error::DracErrorCode;
  } // namespace

#ifdef _WIN32
  /**
   * @brief Safely retrieves an environment variable.
   * @tparam CharT Character type (char or wchar_t)
   * @param name The name of the environment variable to retrieve.
   * @return A Result containing the value of the environment variable.
   */
  template <typename CharT>
  [[nodiscard]] inline fn GetEnv(const CharT* name) -> Result<const CharT*> {
    CharT* rawPtr     = nullptr;
    usize  bufferSize = 0;

    i32 err = 0;

    if constexpr (std::is_same_v<CharT, char>)
      err = _dupenv_s(&rawPtr, &bufferSize, name);
    else
      err = _wdupenv_s(&rawPtr, &bufferSize, name);

    const UniquePointer<CharT, decltype(&free)> ptrManager(rawPtr, free);

    if (err != 0)
      ERR(PermissionDenied, "Failed to retrieve environment variable");

    if (!ptrManager)
      ERR(NotFound, "Environment variable not found");

    return ptrManager.get();
  }

  /**
   * @brief Safely sets an environment variable.
   * @tparam CharT Character type (char or wchar_t)
   * @param name The name of the environment variable to set.
   * @param value The value to set the environment variable to.
   */
  template <typename CharT>
  inline fn SetEnv(const CharT* name, const CharT* value) -> Unit {
    if constexpr (std::is_same_v<CharT, char>)
      _putenv_s(name, value);
    else
      _wputenv_s(name, value);
  }

  /**
   * @brief Safely unsets an environment variable.
   * @tparam CharT Character type (char or wchar_t)
   * @param name The name of the environment variable to unset.
   */
  template <typename CharT>
  inline fn UnsetEnv(const CharT* name) -> Unit {
    if constexpr (std::is_same_v<CharT, char>)
      _putenv_s(name, "");
    else
      _wputenv_s(name, L"");
  }
#else
  /**
   * @brief Safely retrieves an environment variable.
   * @tparam CharT Character type (char only for POSIX)
   * @param name The name of the environment variable to retrieve.
   * @return A Result containing the value of the environment variable.
   */
  template <typename CharT>
  [[nodiscard]] inline fn GetEnv(const CharT* name) -> Result<const CharT*> {
    static_assert(std::is_same_v<CharT, char>, "Only char is supported on POSIX systems");

    const CharT* value = std::getenv(name);

    if (!value)
      ERR(NotFound, "Environment variable not found");

    return value;
  }

  /**
   * @brief Safely sets an environment variable.
   * @tparam CharT Character type (char only for POSIX)
   * @param name The name of the environment variable to set.
   * @param value The value to set the environment variable to.
   */
  template <typename CharT>
  inline fn SetEnv(const CharT* name, const CharT* value) -> Unit {
    static_assert(std::is_same_v<CharT, char>, "Only char is supported on POSIX systems");
    setenv(name, value, 1);
  }

  /**
   * @brief Safely unsets an environment variable.
   * @tparam CharT Character type (char only for POSIX)
   * @param name The name of the environment variable to unset.
   */
  template <typename CharT>
  inline fn UnsetEnv(const CharT* name) -> Unit {
    static_assert(std::is_same_v<CharT, char>, "Only char is supported on POSIX systems");
    unsetenv(name);
  }
#endif
} // namespace draconis::utils::env
