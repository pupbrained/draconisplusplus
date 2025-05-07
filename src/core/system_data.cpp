#include "system_data.hpp"

#include <chrono> // std::chrono::system_clock
#include <ctime>  // localtime_r/s, strftime, time_t, tm
#include <format> // std::format
#include <future> // std::{async, launch}

#include "src/config/config.hpp"
#include "src/config/weather.hpp"
#include "src/core/package.hpp"
#include "src/os/os.hpp"
#include "src/util/defs.hpp"
#include "src/util/error.hpp"
#include "src/util/types.hpp"

#include "include/matchit.hpp"

using util::error::DracError, util::error::DracErrorCode;

namespace {
  using util::types::i32, util::types::CStr;

  fn getOrdinalSuffix(const i32 day) -> CStr {
    using matchit::match, matchit::is, matchit::_, matchit::within;

    if (day >= 11 && day <= 13)
      return "th";

    return match(day % 10)(
      is | within(11, 13) = "th",
      is | 1              = "st",
      is | 2              = "nd",
      is | 3              = "rd",
      is | _              = "th"
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
  SystemData::SystemData(const Config& config) {
    using enum std::launch;
    using package::GetTotalCount;
    using util::types::Future, util::types::Err;

    Future<Result<String>>          hostFut   = std::async(async, GetHost);
    Future<Result<String>>          kernelFut = std::async(async, GetKernelVersion);
    Future<Result<String>>          osFut     = std::async(async, GetOSVersion);
    Future<Result<u64>>             memFut    = std::async(async, GetMemInfo);
    Future<Result<String>>          deFut     = std::async(async, GetDesktopEnvironment);
    Future<Result<String>>          wmFut     = std::async(async, GetWindowManager);
    Future<Result<DiskSpace>>       diskFut   = std::async(async, GetDiskUsage);
    Future<Result<String>>          shellFut  = std::async(async, GetShell);
    Future<Result<u64>>             pkgFut    = std::async(async, GetTotalCount);
    Future<Result<MediaInfo>>       npFut     = std::async(config.nowPlaying.enabled ? async : deferred, GetNowPlaying);
    Future<Result<weather::Output>> wthrFut =
      std::async(config.weather.enabled ? async : deferred, [&config] { return config.weather.getWeatherInfo(); });

    this->date          = getDate();
    this->host          = hostFut.get();
    this->kernelVersion = kernelFut.get();
    this->osVersion     = osFut.get();
    this->memInfo       = memFut.get();
    this->desktopEnv    = deFut.get();
    this->windowMgr     = wmFut.get();
    this->diskUsage     = diskFut.get();
    this->shell         = shellFut.get();
    this->packageCount  = pkgFut.get();
    this->weather =
      config.weather.enabled ? wthrFut.get() : Err(DracError(DracErrorCode::ApiUnavailable, "Weather API disabled"));
    this->nowPlaying = config.nowPlaying.enabled
      ? npFut.get()
      : Err(DracError(DracErrorCode::ApiUnavailable, "Now Playing API disabled"));
  }
} // namespace os
