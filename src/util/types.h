#pragma once

#include <array>    // std::array alias (Array)
#include <cstdlib>  // std::getenv, std::free
#include <expected> // std::expected alias (Result)
#include <map>      // std::map alias (Map)
#include <memory>   // std::shared_ptr and std::unique_ptr aliases (SharedPointer, UniquePointer)
#include <optional> // std::optional alias (Option)
#include <string>   // std::string and std::string_view aliases (String, StringView)
#include <utility>  // std::pair alias (Pair)
#include <variant>  // std::variant alias (NowPlayingError)
#include <vector>   // std::vector alias (Vec)

#ifdef _WIN32
#include <winrt/base.h> // winrt::hresult_error (WindowsError)
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

using f32 = std::float_t;  ///< 32-bit floating-point number.
using f64 = std::double_t; ///< 64-bit floating-point number.

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

#ifdef _WIN32
using WindowsError = winrt::hresult_error; ///< Alias for WinRT HRESULT error type.
#endif

/**
 * @typedef NowPlayingError
 * @brief Represents the possible errors returned by "Now Playing" functions.
 * It's a variant that can hold either a generic NowPlayingCode,
 * a platform-specific error (WindowsError on Windows, String on others),
 * or potentially other error types if extended.
 */
using NowPlayingError = std::variant<
  NowPlayingCode,
#ifdef _WIN32
  WindowsError
#else
  String
#endif
  >;

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
inline auto GetEnv(CStr name) -> Result<String, EnvError> {
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
