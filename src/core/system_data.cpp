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

  fn log_timing(const std::string& name, const std::chrono::steady_clock::duration& duration) -> void {
    const auto millis = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(duration);
    debug_log("{} took: {} ms", name, millis.count());
  };

  template <typename Func>
  fn time_execution(const std::string& name, Func&& func) {
    const auto start = std::chrono::steady_clock::now();
    if constexpr (std::is_void_v<decltype(func())>) {
      func();

      const auto end = std::chrono::steady_clock::now();

      log_timing(name, end - start);
    } else {
      auto result = func();

      const auto end = std::chrono::steady_clock::now();
      log_timing(name, end - start);

      return result;
    }
  }
} // namespace

fn SystemData::fetchSystemData(const Config& config) -> SystemData {
  using util::types::None, util::types::Exception;
  using namespace os;

  SystemData data {
    .date                = time_execution("GetDate", GetDate),
    .host                = time_execution("GetHost", GetHost),
    .kernel_version      = time_execution("GetKernelVersion", GetKernelVersion),
    .os_version          = time_execution("GetOSVersion", GetOSVersion),
    .mem_info            = time_execution("GetMemInfo", GetMemInfo),
    .desktop_environment = time_execution("GetDesktopEnvironment", GetDesktopEnvironment),
    .window_manager      = time_execution("GetWindowManager", GetWindowManager),
    .disk_usage          = time_execution("GetDiskUsage", GetDiskUsage),
    .shell               = time_execution("GetShell", GetShell),
  };

  if (const Result<MediaInfo, DraconisError>& nowPlayingResult = time_execution("GetNowPlaying", os::GetNowPlaying)) {
    data.now_playing = nowPlayingResult;
  } else {
    data.now_playing = None;
  }

  const auto start  = std::chrono::steady_clock::now();
  data.weather_info = config.weather.getWeatherInfo();
  const auto end    = std::chrono::steady_clock::now();

  log_timing("config.weather.getWeatherInfo", end - start);

  return data;
}
