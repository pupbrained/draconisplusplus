#include "SystemData.hpp"

#include <chrono>      // std::chrono::system_clock
#include <ctime>       // localtime_r/s, strftime, time_t, tm
#include <format>      // std::format
#include <future>      // std::{async, launch}
#include <matchit.hpp> // matchit::{match, is, in, _}

#include "Config/Config.hpp"

#ifdef DRAC_ENABLE_PACKAGECOUNT
  #include "Services/PackageCounting.hpp"
#endif
#ifdef DRAC_ENABLE_WEATHER
  #include "Services/Weather.hpp"
#endif

#include "Util/Definitions.hpp"
#include "Util/Error.hpp"
#include "Util/Types.hpp"

#include "OS/OperatingSystem.hpp"

using util::error::DracError, util::error::DracErrorCode;

namespace {
  using util::types::i32, util::types::CStr;

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

  fn getDate() -> Result<String> {
    using std::chrono::system_clock;
    using util::types::String, util::types::usize, util::types::Err;

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
} // namespace

namespace os {
  SystemData::SystemData([[maybe_unused]] const Config& config) {
#ifdef DRAC_ENABLE_PACKAGECOUNT
    using package::GetTotalCount;
#endif // DRAC_ENABLE_PACKAGECOUNT
#ifdef DRAC_ENABLE_WEATHER
    using weather::WeatherReport;
#endif // DRAC_ENABLE_WEATHER
    using util::types::Future, util::types::Err;
    using enum std::launch;
    using enum util::error::DracErrorCode;

    Future<Result<String>>        hostFut   = std::async(async, GetHost);
    Future<Result<String>>        kernelFut = std::async(async, GetKernelVersion);
    Future<Result<String>>        osFut     = std::async(async, GetOSVersion);
    Future<Result<ResourceUsage>> memFut    = std::async(async, GetMemInfo);
    Future<Result<String>>        deFut     = std::async(async, GetDesktopEnvironment);
    Future<Result<String>>        wmFut     = std::async(async, GetWindowManager);
    Future<Result<ResourceUsage>> diskFut   = std::async(async, GetDiskUsage);
    Future<Result<String>>        shellFut  = std::async(async, GetShell);
#if DRAC_ENABLE_PACKAGECOUNT
    Future<Result<u64>> pkgFut;

  #ifdef PRECOMPILED_CONFIG
    pkgFut = std::async(async, GetTotalCount);
  #else
    pkgFut = std::async(async, GetTotalCount);
  #endif
#endif // DRAC_ENABLE_PACKAGECOUNT

#if DRAC_ENABLE_NOWPLAYING
    Future<Result<MediaInfo>> npFut = std::async(config.nowPlaying.enabled ? async : deferred, GetNowPlaying);
#endif // DRAC_ENABLE_NOWPLAYING
#if DRAC_ENABLE_WEATHER
    Future<Result<WeatherReport>> wthrFut = std::async(config.weather.enabled ? async : deferred, [&config]() -> Result<WeatherReport> {
      return config.weather.enabled && config.weather.service
        ? config.weather.service->getWeatherInfo()
        : Err(DracError(ApiUnavailable, "Weather API disabled"));
    });
#endif // DRAC_ENABLE_WEATHER

    this->date          = getDate();
    this->host          = hostFut.get();
    this->kernelVersion = kernelFut.get();
    this->osVersion     = osFut.get();
    this->memInfo       = memFut.get();
    this->desktopEnv    = deFut.get();
    this->windowMgr     = wmFut.get();
    this->diskUsage     = diskFut.get();
    this->shell         = shellFut.get();
#if DRAC_ENABLE_PACKAGECOUNT
    this->packageCount = pkgFut.get();
#endif // DRAC_ENABLE_PACKAGECOUNT
#if DRAC_ENABLE_WEATHER
    this->weather = config.weather.enabled ? wthrFut.get() : Err(DracError(ApiUnavailable, "Weather API disabled"));
#endif // DRAC_ENABLE_WEATHER
#if DRAC_ENABLE_NOWPLAYING
    this->nowPlaying = config.nowPlaying.enabled ? npFut.get() : Err(DracError(ApiUnavailable, "Now Playing API disabled"));
#endif // DRAC_ENABLE_NOWPLAYING
  }
} // namespace os
