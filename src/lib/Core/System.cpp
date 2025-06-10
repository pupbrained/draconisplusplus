#include "System.hpp"

#include <chrono>      // std::chrono::system_clock
#include <ctime>       // localtime_r/s, strftime, time_t, tm
#include <format>      // std::format
#include <functional>  // std::cref
#include <future>      // std::{async, launch}
#include <matchit.hpp> // matchit::{match, is, in, _}

#include "Config/Config.hpp"

#if DRAC_ENABLE_PACKAGECOUNT
  #include "Services/PackageCounting.hpp"
#endif

#if DRAC_ENABLE_WEATHER
  #include "Services/Weather.hpp"
#endif

#include "Util/Definitions.hpp"
#include "Util/Error.hpp"
#include "Util/Types.hpp"

using util::error::DracErrorCode;
using util::types::Err, util::types::i32, util::types::CStr, util::types::usize;

namespace {
  fn getOrdinalSuffix(const i32 day) -> CStr {
    using matchit::match, matchit::is, matchit::_, matchit::in;

    return match(day)(
      is | in(11, 13)    = "th",
      is | (_ % 10 == 1) = "st",
      is | (_ % 10 == 2) = "nd",
      is | (_ % 10 == 3) = "rd",
      is | _             = "th"
    );
  }
} // namespace

namespace os {
  fn System::getDate() -> Result<String> {
    using std::chrono::system_clock;

    const system_clock::time_point nowTp = system_clock::now();
    const std::time_t              nowTt = system_clock::to_time_t(nowTp);

    std::tm nowTm;

#ifdef _WIN32
    if (localtime_s(&nowTm, &nowTt) == 0) {
#else
    if (localtime_r(&nowTt, &nowTm) != nullptr) {
#endif
      i32 day = nowTm.tm_mday;

      String monthBuffer(32, '\0');

      if (const usize monthLen = std::strftime(monthBuffer.data(), monthBuffer.size(), "%B", &nowTm); monthLen > 0) {
        monthBuffer.resize(monthLen);

        CStr suffix = getOrdinalSuffix(day);

        try {
          return std::format("{} {}{}", monthBuffer, day, suffix);
        } catch (const std::format_error& e) { return Err(DracError(DracErrorCode::ParseError, e.what())); }
      }

      return Err(DracError(DracErrorCode::ParseError, "Failed to format date"));
    }

    return Err(DracError(DracErrorCode::ParseError, "Failed to get local time"));
  }

#if DRAC_ENABLE_WEATHER
  fn System::getWeatherInfo(const Config& config) -> Result<weather::WeatherReport> {
    if (config.weather.enabled && config.weather.service)
      return config.weather.service->getWeatherInfo();

    return Err(DracError(DracErrorCode::ApiUnavailable, "Weather API disabled or service not configured"));
  }
#endif

  System::System(const Config& config) {
    using util::types::Future;
    using enum std::launch;
    using enum util::error::DracErrorCode;

    // Use batch operations for related information
    Future<Result<SystemInfo>>      sysFut  = std::async(async, &System::getSystemInfo);
    Future<Result<EnvironmentInfo>> envFut  = std::async(async, &System::getEnvironmentInfo);
    Future<Result<ResourceInfo>>    resFut  = std::async(async, &System::getResourceInfo);
    Future<Result<String>>          dateFut = std::async(async, &System::getDate);

#if DRAC_ENABLE_PACKAGECOUNT
    Future<Result<u64>> pkgFut = std::async(async, package::GetTotalCount);
#endif

#if DRAC_ENABLE_NOWPLAYING
    Future<Result<MediaInfo>> npFut = std::async(config.nowPlaying.enabled ? async : deferred, &System::getNowPlaying);
#endif

#if DRAC_ENABLE_WEATHER
    Future<Result<weather::WeatherReport>> wthrFut = std::async(config.weather.enabled ? async : deferred, &System::getWeatherInfo, std::cref(config));
#endif

    // Get results from batch operations
    auto sysResult  = sysFut.get();
    auto envResult  = envFut.get();
    auto resResult  = resFut.get();
    auto dateResult = dateFut.get();

    // Assign results to member variables
    if (sysResult) {
      this->osVersion     = std::move(sysResult->osVersion);
      this->kernelVersion = std::move(sysResult->kernelVersion);
      this->host          = std::move(sysResult->host);
    } else {
      this->osVersion     = Err(DracError(PlatformSpecific, "Failed to fetch system information"));
      this->kernelVersion = Err(DracError(PlatformSpecific, "Failed to fetch system information"));
      this->host          = Err(DracError(PlatformSpecific, "Failed to fetch system information"));
    }

    if (envResult) {
      this->desktopEnv = std::move(envResult->desktopEnv);
      this->windowMgr  = std::move(envResult->windowMgr);
      this->shell      = std::move(envResult->shell);
    } else {
      this->desktopEnv = Err(DracError(PlatformSpecific, "Failed to fetch environment information"));
      this->windowMgr  = Err(DracError(PlatformSpecific, "Failed to fetch environment information"));
      this->shell      = Err(DracError(PlatformSpecific, "Failed to fetch environment information"));
    }

    if (resResult) {
      this->memInfo   = resResult->memInfo;
      this->diskUsage = resResult->diskUsage;
    } else {
      this->memInfo   = Err(DracError(PlatformSpecific, "Failed to fetch resource information"));
      this->diskUsage = Err(DracError(PlatformSpecific, "Failed to fetch resource information"));
    }

    this->date = dateResult;

#if DRAC_ENABLE_PACKAGECOUNT
    this->packageCount = pkgFut.get();
#endif

#if DRAC_ENABLE_WEATHER
    this->weather = config.weather.enabled ? wthrFut.get() : Err(DracError(ApiUnavailable, "Weather API disabled"));
#endif

#if DRAC_ENABLE_NOWPLAYING
    this->nowPlaying = config.nowPlaying.enabled ? npFut.get() : Err(DracError(ApiUnavailable, "Now Playing API disabled"));
#endif
  }

  fn System::getSystemInfo() -> Result<SystemInfo> {
    using util::types::Future;
    using enum std::launch;

    Future<Result<String>> osFut     = std::async(async, &System::getOSVersion);
    Future<Result<String>> kernelFut = std::async(async, &System::getKernelVersion);
    Future<Result<String>> hostFut   = std::async(async, &System::getHost);
    Future<Result<String>> cpuFut    = std::async(async, &System::getCPUModel);
    Future<Result<String>> gpuFut    = std::async(async, &System::getGPUModel);

    auto osResult     = osFut.get();
    auto kernelResult = kernelFut.get();
    auto hostResult   = hostFut.get();
    auto cpuResult    = cpuFut.get();
    auto gpuResult    = gpuFut.get();

    if (!osResult || !kernelResult || !hostResult || !cpuResult || !gpuResult) {
      return Err(DracError(DracErrorCode::PlatformSpecific, "Failed to fetch system information"));
    }

    return SystemInfo {
      .osVersion     = std::move(*osResult),
      .kernelVersion = std::move(*kernelResult),
      .host          = std::move(*hostResult),
      .cpuModel      = std::move(*cpuResult),
      .gpuModel      = std::move(*gpuResult)
    };
  }

  fn System::getEnvironmentInfo() -> Result<EnvironmentInfo> {
    using util::types::Future;
    using enum std::launch;

    Future<Result<String>> deFut    = std::async(async, &System::getDesktopEnvironment);
    Future<Result<String>> wmFut    = std::async(async, &System::getWindowManager);
    Future<Result<String>> shellFut = std::async(async, &System::getShell);

    auto deResult    = deFut.get();
    auto wmResult    = wmFut.get();
    auto shellResult = shellFut.get();

    if (!deResult || !wmResult || !shellResult) {
      return Err(DracError(DracErrorCode::PlatformSpecific, "Failed to fetch environment information"));
    }

    return EnvironmentInfo {
      .desktopEnv = std::move(*deResult),
      .windowMgr  = std::move(*wmResult),
      .shell      = std::move(*shellResult)
    };
  }

  fn System::getResourceInfo() -> Result<ResourceInfo> {
    using util::types::Future;
    using enum std::launch;

    Future<Result<ResourceUsage>> memFut  = std::async(async, &System::getMemInfo);
    Future<Result<ResourceUsage>> diskFut = std::async(async, &System::getDiskUsage);

    auto memResult  = memFut.get();
    auto diskResult = diskFut.get();

    if (!memResult || !diskResult) {
      return Err(DracError(DracErrorCode::PlatformSpecific, "Failed to fetch resource information"));
    }

    return ResourceInfo {
      .memInfo   = *memResult,
      .diskUsage = *diskResult
    };
  }
} // namespace os