/**
 * @file Types.hpp
 * @brief Defines various type aliases for commonly used types.
 *
 * This header provides a collection of type aliases for commonly used types
 * in the Drac++ project. These aliases are defined using the standard library
 * types and are provided as convenient shorthand notations.
 */

#pragma once

#include <array> // std::array (Array)
#include <format>
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

namespace draconis::utils::types {
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
   * @brief Alias for std::string_view.
   *
   * Non-owning view of a string.
   */
  using StringView = std::string_view;

  /**
   * @brief Alias for const char*.
   *
   * Pointer to a null-terminated C-style string.
   */
  using CStr = const char*;

  /**
   * @brief Alias for void*.
   *
   * A type-erased pointer.
   */
  using AnyPtr = void*;

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
   * @brief Alias for std::nullopt_t.
   *
   * Represents an empty optional value.
   */
  inline constexpr std::nullopt_t None = std::nullopt;

  /**
   * @brief Alias for std::optional<Tp>.
   *
   * Represents a value that may or may not be present.
   * @tparam Tp The type of the potential value.
   */
  template <typename Tp>
  using Option = std::optional<Tp>;

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
   * @brief Alias for std::map<Key, Val>.
   *
   * Represents an ordered map (dictionary).
   * @tparam Key The key type.
   * @tparam Val The value type.
   */
  template <typename Key, typename Val>
  using Map = std::map<Key, Val>;

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
    Option<String> title;  ///< Track title.
    Option<String> artist; ///< Track artist(s).

    MediaInfo() = default;

    MediaInfo(Option<String> title, Option<String> artist)
      : title(std::move(title)), artist(std::move(artist)) {}
  };

  constexpr u64 GIB = 1'073'741'824;

  struct BytesToGiB {
    u64 value;

    explicit constexpr BytesToGiB(const u64 value)
      : value(value) {}
  };

  struct SecondsToFormattedDuration {
    std::chrono::seconds value;

    explicit constexpr SecondsToFormattedDuration(const std::chrono::seconds value)
      : value(value) {}
  };

  enum class CPUArch : u8 {
    I686,    ///< x86 32-bit architecture.
    X86_64,  ///< x86_64 64-bit architecture.
    ARM,     ///< 32-bit ARM architecture.
    AARCH64, ///< 64-bit ARM architecture (ARMv8-A).
    UNKNOWN  ///< Unknown or unsupported architecture.
  };

  /**
   * @struct CPUCores
   * @brief Represents the number of physical and logical cores on a CPU.
   *
   * Used to report the number of physical and logical cores on a CPU.
   */
  struct CPUCores {
    u16 physical; ///< Number of physical cores.
    u16 logical;  ///< Number of logical cores.

    CPUCores() = default;

    CPUCores(u16 physical, u16 logical)
      : physical(physical), logical(logical) {}
  };

  struct Frequencies {
    f64 base;    ///< Base (rated) frequency in MHz.
    f64 max;     ///< Maximum (turbo) frequency in MHz.
    f64 current; ///< Current operating frequency in MHz (can fluctuate).
  };

  /**
   * @struct Display
   * @brief Represents a display device.
   *
   * Used to report the display device.
   */
  struct Display {
    u32 id; ///< Display ID.

    struct Resolution {
      u16 width;  ///< Width in pixels.
      u16 height; ///< Height in pixels.
    } resolution; ///< Resolution in pixels.

    u16  refreshRate; ///< Refresh rate in Hz.
    bool isPrimary;   ///< Whether the display is the primary display.

    Display() = default;

    Display(u32 identifier, Resolution resolution, u16 refreshRate, bool isPrimary)
      : id(identifier), resolution(resolution), refreshRate(refreshRate), isPrimary(isPrimary) {}
  };

  struct NetworkInterface {
    String         name;        ///< Network interface name.
    Option<String> ipv4Address; ///< Network interface IPv4 address.
    Option<String> ipv6Address; ///< Network interface IPv6 address.
    Option<String> macAddress;  ///< Network interface MAC address.
    bool           isUp;        ///< Whether the network interface is up.
    bool           isLoopback;  ///< Whether the network interface is a loopback interface.

    NetworkInterface() = default;

    NetworkInterface(String name, Option<String> ipv4Address, Option<String> ipv6Address, Option<String> macAddress, bool isUp, bool isLoopback)
      : name(std::move(name)), ipv4Address(std::move(ipv4Address)), ipv6Address(std::move(ipv6Address)), macAddress(std::move(macAddress)), isUp(isUp), isLoopback(isLoopback) {}
  };
} // namespace draconis::utils::types

namespace std {
  template <>
  struct formatter<draconis::utils::types::BytesToGiB> : formatter<draconis::utils::types::f64> {
    auto format(const draconis::utils::types::BytesToGiB& BTG, auto& ctx) const {
      return format_to(ctx.out(), "{:.2f}GiB", static_cast<draconis::utils::types::f64>(BTG.value) / draconis::utils::types::GIB);
    }
  };

  template <>
  struct formatter<draconis::utils::types::SecondsToFormattedDuration> : formatter<draconis::utils::types::String> {
    auto format(const draconis::utils::types::SecondsToFormattedDuration& stfd, auto& ctx) const {
      using draconis::utils::types::Array;
      using draconis::utils::types::String;
      using draconis::utils::types::u64;
      using draconis::utils::types::usize;

      const u64 totalSeconds = stfd.value.count();
      const u64 days         = totalSeconds / 86400;
      const u64 hours        = (totalSeconds % 86400) / 3600;
      const u64 minutes      = (totalSeconds % 3600) / 60;
      const u64 seconds      = totalSeconds % 60;

      Array<String, 4> parts = {};

      usize partsCount = 0;

      if (days > 0)
        parts.at(partsCount++) = std::format("{}d", days);
      if (hours > 0)
        parts.at(partsCount++) = std::format("{}h", hours);
      if (minutes > 0)
        parts.at(partsCount++) = std::format("{}m", minutes);
      if (seconds > 0 || partsCount == 0)
        parts.at(partsCount++) = std::format("{}s", seconds);

      String formattedString;
      for (usize i = 0; i < partsCount; ++i) {
        formattedString += parts.at(i);
        if (i < partsCount - 1)
          formattedString += " ";
      }

      return std::formatter<String>::format(formattedString, ctx);
    }
  };
} // namespace std
