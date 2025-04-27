#include "system_data.h"

#include <chrono>    // for year_month_day, floor, days...
#include <exception> // for exception
#include <future>    // for future, async, launch
#include <locale>    // for locale
#include <stdexcept> // for runtime_error
#include <tuple>     // for tuple, get, make_tuple
#include <utility>   // for move

#include "src/config/config.h" // for Config, Weather, NowPlaying
#include "src/os/os.h"         // for GetDesktopEnvironment, GetHost...

namespace {
  fn GetDate() -> String {
    using namespace std::chrono;

    const year_month_day ymd = year_month_day { floor<days>(system_clock::now()) };

    try {
      return std::format(std::locale(""), "{:%B %d}", ymd);
    } catch (const std::runtime_error& e) {
      WARN_LOG("Could not retrieve or use system locale ({}). Falling back to default C locale.", e.what());
      return std::format(std::locale::classic(), "{:%B %d}", ymd);
    }
  }
} // namespace

SystemData SystemData::fetchSystemData(const Config& config) {
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
    Result<DiskSpace, OsError> diskResult  = os::GetDiskUsage();
    Option<String>             shellOption = os::GetShell();
    return std::make_tuple(std::move(diskResult), std::move(shellOption));
  });

  std::future<WeatherOutput>                      weatherFuture;
  std::future<Result<MediaInfo, NowPlayingError>> nowPlayingFuture;

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
    } catch (const std::exception& e) {
      ERROR_LOG("Failed to get weather info: {}", e.what());
      data.weather_info = None;
    }

  if (nowPlayingFuture.valid())
    data.now_playing = nowPlayingFuture.get();

  return data;
}
