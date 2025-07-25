/**
 * @file DataTypes.hpp
 * @brief Defines various data structures for use in Drac++.
 *
 * This header provides a collection of data structures for use
 * in the Drac++ project.
 */

#pragma once

#include <chrono>

#include <Drac++/Utils/Types.hpp>

namespace draconis::utils::types {
  /**
   * @struct OSInfo
   * @brief Represents information about the operating system.
   */
  struct OSInfo {
    String name;
    String version;
    String id;

    OSInfo() = default;

    OSInfo(String& name, String& version, String& identifier)
      : name(std::move(name)), version(std::move(version)), id(std::move(identifier)) {}
  };

  /**
   * @struct ResourceUsage
   * @brief Represents usage information for a resource (disk space, RAM, etc.).
   *
   * Used to report usage statistics for various system resources.
   */
  struct ResourceUsage {
    u64 usedBytes;  ///< Currently used resource space in bytes.
    u64 totalBytes; ///< Total resource space in bytes.

    ResourceUsage() = default;

    ResourceUsage(const u64& usedBytes, const u64& totalBytes)
      : usedBytes(usedBytes), totalBytes(totalBytes) {}
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

  /**
   * @struct CPUCores
   * @brief Represents the number of physical and logical cores on a CPU.
   *
   * Used to report the number of physical and logical cores on a CPU.
   */
  struct CPUCores {
    usize physical; ///< Number of physical cores.
    usize logical;  ///< Number of logical cores.

    CPUCores() = default;

    CPUCores(const usize& physical, const usize& logical)
      : physical(physical), logical(logical) {}
  };

  /**
   * @struct DisplayInfo
   * @brief Represents a display or monitor device.
   *
   * Used to report the display or monitor device.
   */
  struct DisplayInfo {
    usize id; ///< Output ID.

    struct Resolution {
      usize width;  ///< Width in pixels.
      usize height; ///< Height in pixels.
    } resolution;   ///< Resolution in pixels.

    f64  refreshRate; ///< Refresh rate in Hz.
    bool isPrimary;   ///< Whether the display is the primary display.

    DisplayInfo() = default;

    DisplayInfo(const usize& identifier, const Resolution& resolution, const f64& refreshRate, const bool& isPrimary)
      : id(identifier), resolution(resolution), refreshRate(refreshRate), isPrimary(isPrimary) {}
  };

  /**
   * @struct NetworkInterface
   * @brief Represents a network interface.
   */
  struct NetworkInterface {
    String         name;        ///< Network interface name.
    Option<String> ipv4Address; ///< Network interface IPv4 address.
    Option<String> ipv6Address; ///< Network interface IPv6 address.
    Option<String> macAddress;  ///< Network interface MAC address.
    bool           isUp;        ///< Whether the network interface is up.
    bool           isLoopback;  ///< Whether the network interface is a loopback interface.

    NetworkInterface() = default;

    NetworkInterface(String& name, Option<String> ipv4Address, Option<String> ipv6Address, Option<String> macAddress, bool isUp, bool isLoopback)
      : name(std::move(name)), ipv4Address(std::move(ipv4Address)), ipv6Address(std::move(ipv6Address)), macAddress(std::move(macAddress)), isUp(isUp), isLoopback(isLoopback) {}
  };

  /**
   * @struct Battery
   * @brief Represents a battery.
   */
  struct Battery {
    enum class Status : u8 {
      Unknown,     ///< Battery status is unknown.
      Charging,    ///< Battery is charging.
      Discharging, ///< Battery is discharging.
      Full,        ///< Battery is fully charged.
      NotPresent,  ///< No battery present.
    } status;      ///< Current battery status.

    Option<u8>                   percentage;    ///< Battery charge percentage (0-100).
    Option<std::chrono::seconds> timeRemaining; ///< Estimated time remaining in seconds, if available.

    Battery() = default;

    Battery(const Status& status, const Option<u8> percentage, Option<std::chrono::seconds> timeRemaining)
      : status(status), percentage(percentage), timeRemaining(timeRemaining) {}
  };

  /**
   * @struct BytesToGiB
   * @brief Represents a value in bytes converted to gibibytes.
   */
  struct BytesToGiB {
    u64 value;

    explicit constexpr BytesToGiB(const u64& value)
      : value(value) {}
  };

  /**
   * @struct SecondsToFormattedDuration
   * @brief Represents a value in seconds converted to a formatted duration.
   */
  struct SecondsToFormattedDuration {
    std::chrono::seconds value;

    explicit constexpr SecondsToFormattedDuration(const std::chrono::seconds& value)
      : value(value) {}
  };
} // namespace draconis::utils::types

namespace std {
  template <>
  struct formatter<draconis::utils::types::BytesToGiB> : formatter<draconis::utils::types::f64> {
    fn format(const draconis::utils::types::BytesToGiB& BTG, auto& ctx) const {
      constexpr draconis::utils::types::u64 gib = 1'073'741'824;

      return format_to(ctx.out(), "{:.2f}GiB", static_cast<draconis::utils::types::f64>(BTG.value) / gib);
    }
  };

  template <>
  struct formatter<draconis::utils::types::SecondsToFormattedDuration> : formatter<draconis::utils::types::String> {
    fn format(const draconis::utils::types::SecondsToFormattedDuration& stfd, auto& ctx) const {
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