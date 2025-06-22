#pragma once

#include <Drac++/Utils/Error.hpp>
#include <Drac++/Utils/Types.hpp>

#include "Config/Config.hpp"

namespace draconis::core::system {
  namespace {
    using config::Config;

    using utils::types::MediaInfo;
    using utils::types::ResourceUsage;
    using utils::types::Result;
    using utils::types::String;
    using utils::types::u64;
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
    Result<String>        gpuModel;
#if DRAC_ENABLE_PACKAGECOUNT
    Result<u64> packageCount;
#endif
#if DRAC_ENABLE_NOWPLAYING
    Result<MediaInfo> nowPlaying;
#endif

    explicit SystemInfo(const Config& config);
  };
} // namespace draconis::core::system
