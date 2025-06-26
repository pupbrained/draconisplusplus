#pragma once

#include <Drac++/Core/System.hpp>

#include <Drac++/Utils/Error.hpp>
#include <Drac++/Utils/Types.hpp>

#include "Config/Config.hpp"

namespace draconis::core::system {
  namespace {
    using config::Config;

    using utils::types::CPUArch;
    using utils::types::Frequencies;
    using utils::types::MediaInfo;
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
    Result<String>        osVersion;
    Result<ResourceUsage> memInfo;
    Result<String>        desktopEnv;
    Result<String>        windowMgr;
    Result<ResourceUsage> diskUsage;
    Result<String>        shell;
    Result<String>        cpuModel;
    Result<CPUCores>      cpuCores;
    Result<String>        gpuModel;
    Result<seconds>       uptime;
    Result<Display>       primaryDisplay;
#if DRAC_ENABLE_PACKAGECOUNT
    Result<u64> packageCount;
#endif
#if DRAC_ENABLE_NOWPLAYING
    Result<MediaInfo> nowPlaying;
#endif

    explicit SystemInfo(utils::cache::CacheManager& cache, const Config& config);
  };
} // namespace draconis::core::system
