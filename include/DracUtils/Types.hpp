#pragma once

#include <array>       // std::array (Array)
#include <format>      // std::formatter
#include <future>      // std::future (Future)
#include <map>         // std::map (Map)
#include <memory>      // std::shared_ptr and std::unique_ptr (SharedPointer, UniquePointer)
#include <mutex>       // std::mutex and std::lock_guard (Mutex, LockGuard)
#include <optional>    // std::optional (Option)
#include <string>      // std::string (String, StringView)
#include <string_view> // std::string_view (StringView)
#include <stringzilla/stringzilla.hpp>
#include <utility> // std::pair (Pair)
#include <vector>  // std::vector (Vec)

namespace util::types {
  using u8  = std::uint8_t;  ///< 8-bit unsigned integer.
  using u16 = std::uint16_t; ///< 16-bit unsigned integer.
  using u32 = std::uint32_t; ///< 32-bit unsigned integer.
  using u64 = std::uint64_t; ///< 64-bit unsigned integer.

  using i8  = std::int8_t;  ///< 8-bit signed integer.
  using i16 = std::int16_t; ///< 16-bit signed integer.
  using i32 = std::int32_t; ///< 32-bit signed integer.
  using i64 = std::int64_t; ///< 64-bit signed integer.

  using f32 = float;  ///< 32-bit floating-point number.
  using f64 = double; ///< 64-bit floating-point number.

  using usize = std::size_t;    ///< Unsigned size type (result of sizeof).
  using isize = std::ptrdiff_t; ///< Signed size type (result of pointer subtraction).

  using String     = std::string;      ///< Owning, mutable string.
  using StringView = std::string_view; ///< Non-owning view of a string.

  using SZString     = ashvardanian::stringzilla::string;      ///< Owning, mutable string.
  using SZStringView = ashvardanian::stringzilla::string_view; ///< Non-owning view of a string.

  using CStr = const char*; ///< Pointer to a null-terminated C-style string.

  using AnyPtr = void*; ///< A type-erased pointer.

  using Exception = std::exception; ///< Standard exception type.

  using Mutex     = std::mutex;             ///< Mutex type for synchronization.
  using LockGuard = std::lock_guard<Mutex>; ///< RAII-style lock guard for mutexes.

  inline constexpr std::nullopt_t None = std::nullopt; ///< Represents an empty optional value.

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

  /**
   * @typedef Future
   * @brief Alias for std::future<Tp>. Represents a value that will be available in the future.
   * @tparam Tp The type of the value.
   */
  template <typename Tp>
  using Future = std::future<Tp>;

  /**
   * @struct ResourceUsage
   * @brief Represents usage information for a resource (disk space, RAM, etc.).
   *
   * Used to report usage statistics for various system resources.
   */
  struct ResourceUsage {
    u64 usedBytes;  ///< Currently used resource space in bytes.
    u64 totalBytes; ///< Total resource space in bytes.
  };

  /**
   * @struct MediaInfo
   * @brief Holds structured metadata about currently playing media.
   *
   * Used as the success type for os::GetNowPlaying.
   * Using Option<> for fields that might not always be available.
   */
  struct MediaInfo {
    Option<SZString> title;  ///< Track title.
    Option<SZString> artist; ///< Track artist(s).

    MediaInfo() = default;

    MediaInfo(Option<SZString> title, Option<SZString> artist)
      : title(std::move(title)), artist(std::move(artist)) {}
  };
} // namespace util::types

// Custom formatters for SZString and SZStringView
namespace std {
  /**
   * @brief Formatter specialization for SZString
   * @tparam CharT Character type (char)
   */
  template <typename CharT>
  struct formatter<util::types::SZString, CharT> : formatter<util::types::StringView, CharT> {
    template <typename FormatContext>
    auto format(const util::types::SZString& str, FormatContext& ctx) const {
      return formatter<util::types::StringView, CharT>::format(util::types::StringView(str.data(), str.size()), ctx);
    }
  };

  /**
   * @brief Formatter specialization for SZStringView
   * @tparam CharT Character type (char)
   */
  template <typename CharT>
  struct formatter<util::types::SZStringView, CharT> : formatter<util::types::StringView, CharT> {
    template <typename FormatContext>
    auto format(const util::types::SZStringView& str, FormatContext& ctx) const {
      return formatter<util::types::StringView, CharT>::format(util::types::StringView(str.data(), str.size()), ctx);
    }
  };
} // namespace std
