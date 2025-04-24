#include <future>

#include "system_data.h"

#include "src/config/config.h"
#include "src/os/os.h"

namespace {
  fn GetDate() -> String {
    using namespace std::chrono;

    const year_month_day ymd = year_month_day { floor<days>(system_clock::now()) };

    String month = std::format("{:%B}", ymd);

    u32 day = static_cast<u32>(ymd.day());

    CStr suffix = day >= 11 && day <= 13 ? "th"
                  : day % 10 == 1        ? "st"
                  : day % 10 == 2        ? "nd"
                  : day % 10 == 3        ? "rd"
                                         : "th";

    return std::format("{} {}{}", month, day, suffix);
  }
}

fn SystemData::fetchSystemData(const Config& config) -> SystemData {
  SystemData data {
    .date                = GetDate(),
    .host                = os::GetHost(),
    .kernel_version      = os::GetKernelVersion(),
    .os_version          = os::GetOSVersion(),
    .mem_info            = os::GetMemInfo(),
    .desktop_environment = os::GetDesktopEnvironment(),
    .window_manager      = os::GetWindowManager(),
    .now_playing         = {},
    .weather_info        = {},
    .disk_used           = {},
    .disk_total          = {},
    .shell               = {},
  };

  auto diskShell = std::async(std::launch::async, [] {
    auto [used, total] = os::GetDiskUsage();
    return std::make_tuple(used, total, os::GetShell());
  });

  std::future<WeatherOutput>                   weather;
  std::future<Result<String, NowPlayingError>> nowPlaying;

  if (config.weather.enabled)
    weather = std::async(std::launch::async, [&config] { return config.weather.getWeatherInfo(); });

  if (config.now_playing.enabled)
    nowPlaying = std::async(std::launch::async, os::GetNowPlaying);

  auto [used, total, shell] = diskShell.get();
  data.disk_used            = used;
  data.disk_total           = total;
  data.shell                = shell;

  if (weather.valid())
    data.weather_info = weather.get();
  if (nowPlaying.valid())
    data.now_playing = nowPlaying.get();

  return data;
}
