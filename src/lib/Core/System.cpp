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

    Future<Result<String>>        hostFut   = std::async(async, &System::getHost);
    Future<Result<String>>        kernelFut = std::async(async, &System::getKernelVersion);
    Future<Result<String>>        osFut     = std::async(async, &System::getOSVersion);
    Future<Result<ResourceUsage>> memFut    = std::async(async, &System::getMemInfo);
    Future<Result<String>>        deFut     = std::async(async, &System::getDesktopEnvironment);
    Future<Result<String>>        wmFut     = std::async(async, &System::getWindowManager);
    Future<Result<ResourceUsage>> diskFut   = std::async(async, &System::getDiskUsage);
    Future<Result<String>>        shellFut  = std::async(async, &System::getShell);

#if DRAC_ENABLE_PACKAGECOUNT
    Future<Result<u64>> pkgFut = std::async(async, package::GetTotalCount);
#endif

#if DRAC_ENABLE_NOWPLAYING
    Future<Result<MediaInfo>> npFut = std::async(config.nowPlaying.enabled ? async : deferred, &System::getNowPlaying);
#endif

#if DRAC_ENABLE_WEATHER
    Future<Result<weather::WeatherReport>> wthrFut = std::async(config.weather.enabled ? async : deferred, &System::getWeatherInfo, std::cref(config));
#endif

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
#endif

#if DRAC_ENABLE_WEATHER
    this->weather = config.weather.enabled ? wthrFut.get() : Err(DracError(ApiUnavailable, "Weather API disabled"));
#endif

#if DRAC_ENABLE_NOWPLAYING
    this->nowPlaying = config.nowPlaying.enabled ? npFut.get() : Err(DracError(ApiUnavailable, "Now Playing API disabled"));
#endif
  }
} // namespace os