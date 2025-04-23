#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef _WIN32
// ReSharper disable once CppUnusedIncludeDirective
#include <guiddef.h>
#include <variant>
#include <winrt/base.h>
#else
#include <variant>
#endif

/**
 * @typedef u8
 * @brief Represents an 8-bit unsigned integer.
 *
 * This type alias is used for 8-bit unsigned integers, ranging from 0 to 255.
 * It is based on the std::uint8_t type.
 */
using u8 = std::uint8_t;

/**
 * @typedef u16
 * @brief Represents a 16-bit unsigned integer.
 *
 * This type alias is used for 16-bit unsigned integers, ranging from 0 to 65,535.
 * It is based on the std::uint16_t type.
 */
using u16 = std::uint16_t;

/**
 * @typedef u32
 * @brief Represents a 32-bit unsigned integer.
 *
 * This type alias is used for 32-bit unsigned integers, ranging from 0 to 4,294,967,295.
 * It is based on the std::uint32_t type.
 */
using u32 = std::uint32_t;

/**
 * @typedef u64
 * @brief Represents a 64-bit unsigned integer.
 *
 * This type alias is used for 64-bit unsigned integers, ranging from 0 to
 * 18,446,744,073,709,551,615. It is based on the std::uint64_t type.
 */
using u64 = std::uint64_t;

// Type Aliases for Signed Integers

/**
 * @typedef i8
 * @brief Represents an 8-bit signed integer.
 *
 * This type alias is used for 8-bit signed integers, ranging from -128 to 127.
 * It is based on the std::int8_t type.
 */
using i8 = std::int8_t;

/**
 * @typedef i16
 * @brief Represents a 16-bit signed integer.
 *
 * This type alias is used for 16-bit signed integers, ranging from -32,768 to 32,767.
 * It is based on the std::int16_t type.
 */
using i16 = std::int16_t;

/**
 * @typedef i32
 * @brief Represents a 32-bit signed integer.
 *
 * This type alias is used for 32-bit signed integers, ranging from -2,147,483,648 to 2,147,483,647.
 * It is based on the std::int32_t type.
 */
using i32 = std::int32_t;

/**
 * @typedef i64
 * @brief Represents a 64-bit signed integer.
 *
 * This type alias is used for 64-bit signed integers, ranging from -9,223,372,036,854,775,808 to
 * 9,223,372,036,854,775,807. It is based on the std::int64_t type.
 */
using i64 = std::int64_t;

// Type Aliases for Floating-Point Numbers

/**
 * @typedef f32
 * @brief Represents a 32-bit floating-point number.
 *
 * This type alias is used for 32-bit floating-point numbers, which follow the IEEE 754 standard.
 * It is based on the float type.
 */
using f32 = float;

/**
 * @typedef f64
 * @brief Represents a 64-bit floating-point number.
 *
 * This type alias is used for 64-bit floating-point numbers, which follow the IEEE 754 standard.
 * It is based on the double type.
 */
using f64 = double;

// Type Aliases for Size Types

/**
 * @typedef usize
 * @brief Represents an unsigned size type.
 *
 * This type alias is used for representing the size of objects in bytes.
 * It is based on the std::size_t type, which is the result type of the sizeof operator.
 */
using usize = std::size_t;

/**
 * @typedef isize
 * @brief Represents a signed size type.
 *
 * This type alias is used for representing pointer differences.
 * It is based on the std::ptrdiff_t type, which is the signed integer type returned when
 * subtracting two pointers.
 */
using isize = std::ptrdiff_t;

/**
 * @typedef String
 * @brief Represents a string.
 */
using String = std::string;

/**
 * @typedef StringView
 * @brief Represents a string view.
 *
 * This type alias is used for non-owning views of strings, allowing for efficient string manipulation
 * without copying the underlying data.
 */
using StringView = std::string_view;

/**
 * @typedef Exception
 * @brief Represents a generic exception type.
 */
using Exception = std::exception;

/**
 * @typedef Expected
 * @brief Represents an expected value or an error.
 */
template <typename Tp, typename Er>
using Result = std::expected<Tp, Er>;

/**
 * @typedef Unexpected
 * @brief Represents an unexpected error.
 */
template <typename Er>
using Err = std::unexpected<Er>;

/**
 * @typedef Optional
 * @brief Represents an optional value.
 */
template <typename Tp>
using Option = std::optional<Tp>;

/**
 * @typedef Array
 * @brief Represents a fixed-size array.
 */
template <typename Tp, std::size_t nm>
using Array = std::array<Tp, nm>;

/**
 * @typedef Vec
 * @brief Represents a dynamic array (vector).
 */
template <typename Tp>
using Vec = std::vector<Tp>;

/**
 * @typedef Pair
 * @brief Represents a pair of values.
 */
template <typename T1, typename T2>
using Pair = std::pair<T1, T2>;

/**
 * @typedef Map
 * @brief Represents a map (dictionary) of key-value pairs.
 */
template <typename Key, typename Val>
using Map = std::map<Key, Val>;

/**
 * @typedef SharedPointer
 * @brief Represents a shared pointer.
 *
 * This type alias is used for shared ownership of dynamically allocated objects.
 */
template <typename Tp>
using SharedPointer = std::shared_ptr<Tp>;

/**
 * @typedef UniquePointer
 * @brief Represents a unique pointer.
 *
 * This type alias is used for unique ownership of dynamically allocated objects.
 */
template <typename Tp, typename Dp>
using UniquePointer = std::unique_ptr<Tp, Dp>;

/**
 * @typedef CStr
 * @brief Represents a C string (const char*).
 *
 * This type alias is used for C-style strings, which are null-terminated arrays of characters.
 */
using CStr = const char*;

/**
 * @enum NowPlayingCode
 * @brief Represents error codes for Now Playing functionality.
 */
enum class NowPlayingCode : u8 {
  NoPlayers,
  NoActivePlayer,
};

#ifdef _WIN32
/**
 * @typedef WindowsError
 * @brief Represents a Windows-specific error.
 */
using WindowsError = winrt::hresult_error;
#endif

// Unified error type
using NowPlayingError = std::variant<
  NowPlayingCode,
#ifdef _WIN32
  WindowsError
#else
  String
#endif
  >;

enum class EnvError : u8 { NotFound, AccessError };

inline auto GetEnv(const String& name) -> Result<String, EnvError> {
#ifdef _WIN32
  char*  rawPtr     = nullptr;
  size_t bufferSize = 0;

  if (_dupenv_s(&rawPtr, &bufferSize, name.c_str()) != 0)
    return std::unexpected(EnvError::AccessError);

  if (!rawPtr)
    return std::unexpected(EnvError::NotFound);

  const String result(rawPtr);
  free(rawPtr);
  return result;
#else
  CStr value = std::getenv(name.c_str());

  if (!value)
    return Err(EnvError::NotFound);

  return String(value);
#endif
}
