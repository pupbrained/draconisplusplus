#pragma once

#include <glaze/glaze.hpp>

#include <Drac++/Core/System.hpp>

#include <Drac++/Utils/Error.hpp>
#include <Drac++/Utils/Types.hpp>

#include "Config/Config.hpp"

namespace draconis::core::system {
  namespace {
    using config::Config;

    using utils::types::MediaInfo;
    using utils::types::OSInfo;
    using utils::types::ResourceUsage;
    using utils::types::Result;
    using utils::types::String;
    using utils::types::u64;
    using utils::types::usize;

    using std::chrono::seconds;
  } // namespace

  /**
   * @brief Utility struct for storing system information.
   */
  struct SystemInfo {
    Result<String>        date;
    Result<String>        host;
    Result<String>        kernelVersion;
    Result<OSInfo>        operatingSystem;
    Result<ResourceUsage> memInfo;
    Result<String>        desktopEnv;
    Result<String>        windowMgr;
    Result<ResourceUsage> diskUsage;
    Result<String>        shell;
    Result<String>        cpuModel;
    Result<CPUCores>      cpuCores;
    Result<String>        gpuModel;
    Result<seconds>       uptime;
#if DRAC_ENABLE_PACKAGECOUNT
    Result<u64> packageCount;
#endif
#if DRAC_ENABLE_NOWPLAYING
    Result<MediaInfo> nowPlaying;
#endif

    explicit SystemInfo(utils::cache::CacheManager& cache, const Config& config);
  };

  struct JsonInfo {
    std::optional<String>        date;
    std::optional<String>        host;
    std::optional<String>        kernelVersion;
    std::optional<OSInfo>        operatingSystem;
    std::optional<ResourceUsage> memInfo;
    std::optional<String>        desktopEnv;
    std::optional<String>        windowMgr;
    std::optional<ResourceUsage> diskUsage;
    std::optional<String>        shell;
    std::optional<String>        cpuModel;
    std::optional<CPUCores>      cpuCores;
    std::optional<String>        gpuModel;
    std::optional<i64>           uptimeSeconds;
#if DRAC_ENABLE_PACKAGECOUNT
    std::optional<u64> packageCount;
#endif
#if DRAC_ENABLE_NOWPLAYING
    std::optional<MediaInfo> nowPlaying;
#endif
#if DRAC_ENABLE_WEATHER
    std::optional<services::weather::Report> weather;
#endif
  };

} // namespace draconis::core::system

namespace glz {
  template <>
  struct meta<draconis::utils::types::ResourceUsage> {
    using T = draconis::utils::types::ResourceUsage;

    static constexpr detail::Object value = object("usedBytes", &T::usedBytes, "totalBytes", &T::totalBytes);
  };

#if DRAC_ENABLE_NOWPLAYING
  template <>
  struct meta<draconis::utils::types::MediaInfo> {
    using T = draconis::utils::types::MediaInfo;

    static constexpr detail::Object value = object("title", &T::title, "artist", &T::artist);
  };
#endif

  template <>
  struct meta<draconis::core::system::JsonInfo> {
    using T = draconis::core::system::JsonInfo;

    // clang-format off
  static constexpr detail::Object value = object(
#if DRAC_ENABLE_PACKAGECOUNT
    "packageCount",    &T::packageCount,
#endif
#if DRAC_ENABLE_NOWPLAYING
    "nowPlaying",      &T::nowPlaying,
#endif
#if DRAC_ENABLE_WEATHER
    "weather",         &T::weather,
#endif
    "date",            &T::date,
    "host",            &T::host,
    "kernelVersion",   &T::kernelVersion,
    "operatingSystem", &T::operatingSystem,
    "memInfo",         &T::memInfo,
    "desktopEnv",      &T::desktopEnv,
    "windowMgr",       &T::windowMgr,
    "diskUsage",       &T::diskUsage,
    "shell",           &T::shell,
    "cpuModel",        &T::cpuModel,
    "cpuCores",        &T::cpuCores,
    "gpuModel",        &T::gpuModel,
    "uptimeSeconds",   &T::uptimeSeconds
  );
    // clang-format on
  };
} // namespace glz