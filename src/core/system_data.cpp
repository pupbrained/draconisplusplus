#include "system_data.hpp"

#include <chrono> // std::chrono::{year_month_day, days, floor, system_clock}
#include <format> // std::format
#include <future> // std::async

#include "src/config/config.hpp"
#include "src/config/weather.hpp"
#include "src/core/util/error.hpp"
#include "src/core/util/types.hpp"
#include "src/os/os.hpp"

namespace os {
  SystemData::SystemData(const Config& config) {
    // NOLINTNEXTLINE(misc-include-cleaner) - std::chrono::{days, floor} are inherited from <chrono>
    using std::chrono::year_month_day, std::chrono::system_clock, std::chrono::floor, std::chrono::days;
    using util::error::DracError, util::error::DracErrorCode;
    using util::types::Result, util::types::Err, util::types::Option, util::types::None, util::types::Exception,
      util::types::Future;
    using weather::Output;
    using enum std::launch;

    Future<Result<String, DracError>>    hostFut   = std::async(async, GetHost);
    Future<Result<String, DracError>>    kernelFut = std::async(async, GetKernelVersion);
    Future<Result<String, DracError>>    osFut     = std::async(async, GetOSVersion);
    Future<Result<u64, DracError>>       memFut    = std::async(async, GetMemInfo);
    Future<Result<String, DracError>>    deFut     = std::async(async, GetDesktopEnvironment);
    Future<Result<String, DracError>>    wmFut     = std::async(async, GetWindowManager);
    Future<Result<DiskSpace, DracError>> diskFut   = std::async(async, GetDiskUsage);
    Future<Result<String, DracError>>    shellFut  = std::async(async, GetShell);
    Future<Result<u64, DracError>>       pkgFut    = std::async(async, GetPackageCount);
    Future<Result<MediaInfo, DracError>> npFut =
      std::async(config.nowPlaying.enabled ? async : deferred, GetNowPlaying);
    Future<Result<Output, DracError>> wthrFut =
      std::async(config.weather.enabled ? async : deferred, [&config] -> Result<Output, DracError> {
        return config.weather.getWeatherInfo();
      });

    // TODO: make this use the user's timezone
    this->date          = std::format("{:%B %d}", year_month_day { floor<days>(system_clock::now()) });
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
