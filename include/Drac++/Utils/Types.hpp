/**
 * @file Types.hpp
 * @brief Defines various type aliases for commonly used types.
 *
 * This header provides a collection of type aliases for commonly used types
 * in the Drac++ project. These aliases are defined using the standard library
 * types and are provided as convenient shorthand notations.
 */

#pragma once

#include <array>         // std::array (Array)
#include <expected>      // std::expected
#include <functional>    // std::function (Fn)
#include <future>        // std::future (Future)
#include <map>           // std::map (Map)
#include <memory>        // std::shared_ptr and std::unique_ptr (SharedPointer, UniquePointer)
#include <mutex>         // std::mutex and std::lock_guard (Mutex, LockGuard)
#include <optional>      // std::optional (Option)
#include <span>          // std::span (Span)
#include <string>        // std::string (String, StringView)
#include <string_view>   // std::string_view (StringView)
#include <unordered_map> // std::unordered_map (UnorderedMap)
#include <utility>       // std::pair (Pair)
#include <vector>        // std::vector (Vec)

namespace draconis::utils {
  // Forward decl for Result and Err
  namespace error {
    struct DracError;
  } // namespace error

  namespace types {
    /**
     * @brief Alias for std::uint8_t.
     *
     * 8-bit unsigned integer.
     */
    using u8 = std::uint8_t;

    /**
     * @brief Alias for std::uint16_t.
     *
     * 16-bit unsigned integer.
     */
    using u16 = std::uint16_t;

    /**
     * @brief Alias for std::uint32_t.
     *
     * 32-bit unsigned integer.
     */
    using u32 = std::uint32_t;

    /**
     * @brief Alias for std::uint64_t.
     *
     * 64-bit unsigned integer.
     */
    using u64 = std::uint64_t;

    /**
     * @brief Alias for std::int8_t.
     *
     * 8-bit signed integer.
     */
    using i8 = std::int8_t;

    /**
     * @brief Alias for std::int16_t.
     *
     * 16-bit signed integer.
     */
    using i16 = std::int16_t;

    /**
     * @brief Alias for std::int32_t.
     *
     * 32-bit signed integer.
     */
    using i32 = std::int32_t;

    /**
     * @brief Alias for std::int64_t.
     *
     * 64-bit signed integer.
     */
    using i64 = std::int64_t;

    /**
     * @brief Alias for float.
     *
     * 32-bit floating-point number.
     */
    using f32 = float;

    /**
     * @brief Alias for double.
     *
     * 64-bit floating-point number.
     */
    using f64 = double;

    /**
     * @brief Alias for std::size_t.
     *
     * Unsigned size type (result of sizeof).
     */
    using usize = std::size_t;

    /**
     * @brief Alias for std::ptrdiff_t.
     *
     * Signed size type (result of pointer subtraction).
     */
    using isize = std::ptrdiff_t;

    /**
     * @brief Alias for std::string.
     *
     * Owning, mutable string.
     */
    using String = std::string;

    /**
     * @brief Alias for std::wstring.
     *
     * Owning, mutable wide string.
     */
    using WString = std::wstring;

    /**
     * @brief Alias for std::string_view.
     *
     * Non-owning view of a string.
     */
    using StringView = std::string_view;

    /**
     * @brief Alias for std::wstring_view.
     *
     * Non-owning view of a wide string.
     */
    using WStringView = std::wstring_view;

    /**
     * @brief Alias for char.
     *
     * Single character type.
     */
    using CStr = char;

    /**
     * @brief Alias for const char*.
     *
     * Pointer to a null-terminated C-style string.
     */
    using PCStr = const char*;

    /**
     * @brief Alias for wchar_t.
     *
     * Single wide character type.
     */
    using WCStr = wchar_t;

    /**
     * @brief Alias for const wchar_t*.
     *
     * Pointer to a null-terminated C-style wide string.
     */
    using PWCStr = const wchar_t*;

    /**
     * @brief Alias for std::exception.
     *
     * Standard exception type.
     */
    using Exception = std::exception;

    /**
     * @brief Alias for std::mutex.
     *
     * Mutex type for synchronization.
     */
    using Mutex = std::mutex;

    /**
     * @brief Alias for std::lock_guard<Mutex>.
     *
     * RAII-style lock guard for mutexes.
     */
    using LockGuard = std::lock_guard<Mutex>;

    /**
     * @brief Alias for void.
     *
     * Represents a unit type.
     */
    using Unit = void;

    /**
     * @brief Alias for void*.
     *
     * A type-erased pointer.
     */
    using RawPointer = void*;

    /**
     * @brief Alias for std::optional<Tp>.
     *
     * Represents a value that may or may not be present.
     * @tparam Tp The type of the potential value.
     */
    template <typename Tp>
    using Option = std::optional<Tp>;

    /**
     * @brief Alias for std::nullopt_t.
     *
     * Represents an empty optional value.
     */
    inline constexpr std::nullopt_t None = std::nullopt;

    /**
     * @brief Helper function to create an Option with a value.
     *
     * Creates an Option containing the given value.
     * @tparam Tp The type of the value.
     * @param value The value to wrap in an Option.
     * @return An Option containing the value.
     */
    template <typename Tp>
    constexpr fn Some(Tp&& value) -> Option<std::remove_reference_t<Tp>> {
      return std::make_optional<std::remove_reference_t<Tp>>(std::forward<Tp>(value));
    }

    /**
     * @brief Alias for std::array<Tp, sz>.
     *
     * Represents a fixed-size array.
     * @tparam Tp The element type.
     * @tparam sz The size of the array.
     */
    template <typename Tp, usize sz>
    using Array = std::array<Tp, sz>;

    /**
     * @brief Alias for std::vector<Tp>.
     *
     * Represents a dynamic-size array (vector).
     * @tparam Tp The element type.
     */
    template <typename Tp>
    using Vec = std::vector<Tp>;

    /**
     * @brief Alias for std::span<Tp, sz>.
     *
     * Represents a non-owning view of a contiguous sequence of elements.
     * @tparam Tp The element type.
     * @tparam sz (Optional) The size of the span.
     */
    template <typename Tp, usize sz = std::dynamic_extent>
    using Span = std::span<Tp, sz>;

    /**
     * @brief Alias for std::pair<T1, T2>.
     *
     * Represents a pair of values.
     * @tparam T1 The type of the first element.
     * @tparam T2 The type of the second element.
     */
    template <typename T1, typename T2>
    using Pair = std::pair<T1, T2>;

    /**
     * @brief Alias for std::tuple<Ts...>.
     *
     * Represents a tuple of values.
     * @tparam Ts The types of the elements.
     */
    template <typename... Ts>
    using Tuple = std::tuple<Ts...>;

    /**
     * @brief Alias for std::map<Key, Val>.
     *
     * Represents an ordered map (dictionary).
     * @tparam Key The key type.
     * @tparam Val The value type.
     */
    template <typename Key, typename Val>
    using Map = std::map<Key, Val, std::less<>>;

    /**
     * @brief Alias for std::unordered_map<Key, Val>.
     *
     * Represents an unordered map (dictionary).
     * @tparam Key The key type.
     * @tparam Val The value type.
     */
    template <typename Key, typename Val>
    using UnorderedMap = std::unordered_map<Key, Val>;

    /**
     * @brief Alias for std::shared_ptr<Tp>.
     *
     * Manages shared ownership of a dynamically allocated object.
     * @tparam Tp The type of the managed object.
     */
    template <typename Tp>
    using SharedPointer = std::shared_ptr<Tp>;

    /**
     * @brief Alias for std::unique_ptr<Tp, Dp>.
     *
     * Manages unique ownership of a dynamically allocated object.
     * @tparam Tp The type of the managed object.
     * @tparam Dp The deleter type (defaults to std::default_delete<Tp>).
     */
    template <typename Tp, typename Dp = std::default_delete<Tp>>
    using UniquePointer = std::unique_ptr<Tp, Dp>;

    /**
     * @brief Alias for std::future<Tp>.
     *
     * @tparam Tp The type of the value.
     */
    template <typename Tp>
    using Future = std::future<Tp>;

    /**
     * @brief Alias for std::move_only_function<Tp>.
     *
     * Represents a callable object.
     * @tparam Tp The return type of the callable.
     */
    template <typename Tp>
    using Fn = std::function<Tp>;

    /**
     * @typedef Result
     * @brief Alias for std::expected<Tp, Er>. Represents a value that can either be
     * a success value of type Tp or an error value of type Er.
     * @tparam Tp The type of the success value.
     * @tparam Er The type of the error value.
     */
    template <typename Tp = Unit, typename Er = error::DracError>
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
