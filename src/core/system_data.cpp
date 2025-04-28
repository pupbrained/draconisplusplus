#include "system_data.hpp"

#include <chrono>    // std::chrono::{year_month_day, floor, days, system_clock}
#include <exception> // std::exception (Exception)
#include <future>    // std::{future, async, launch}
#include <locale>    // std::locale
#include <stdexcept> // std::runtime_error
#include <tuple>     // std::{tuple, get, make_tuple}
#include <utility>   // std::move

#include "src/config/config.hpp"
#include "src/os/os.hpp"

#include "util/logging.hpp"

namespace {
  fn GetDate() -> String {
    using namespace std::chrono;

    const year_month_day ymd = year_month_day { floor<days>(system_clock::now()) };

    try {
      return std::format(std::locale(""), "{:%B %d}", ymd);
    } catch (const std::runtime_error& e) {
      warn_log("Could not retrieve or use system locale ({}). Falling back to default C locale.", e.what());
      return std::format(std::locale::classic(), "{:%B %d}", ymd);
    }
  }
} // namespace

fn SystemData::fetchSystemData(const Config& config) -> SystemData {
  using util::types::None;

  SystemData data {
    .date                = GetDate(),
    .host                = os::GetHost(),
    .kernel_version      = os::GetKernelVersion(),
    .os_version          = os::GetOSVersion(),
    .mem_info            = os::GetMemInfo(),
    .desktop_environment = os::GetDesktopEnvironment(),
    .window_manager      = os::GetWindowManager(),
    .disk_usage          = {},
    .shell               = None,
    .now_playing         = None,
    .weather_info        = None,
  };

  auto diskShellFuture = std::async(std::launch::async, [] {
    Result<DiskSpace, DraconisError> diskResult  = os::GetDiskUsage();
    Option<String>                   shellOption = os::GetShell();
    return std::make_tuple(std::move(diskResult), std::move(shellOption));
  });

  std::future<weather::Output>                  weatherFuture;
  std::future<Result<MediaInfo, DraconisError>> nowPlayingFuture;

  if (config.weather.enabled)
    weatherFuture = std::async(std::launch::async, [&config] { return config.weather.getWeatherInfo(); });

  if (config.now_playing.enabled)
    nowPlayingFuture = std::async(std::launch::async, os::GetNowPlaying);

  auto [diskResult, shellOption] = diskShellFuture.get();

  data.disk_usage = std::move(diskResult);
  data.shell      = std::move(shellOption);

  if (weatherFuture.valid())
    try {
      data.weather_info = weatherFuture.get();
    } catch (const std::exception& e) { data.weather_info = None; }

  if (nowPlayingFuture.valid())
    data.now_playing = nowPlayingFuture.get();

  return data;
}
