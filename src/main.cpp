#include <ctime>
#include <fmt/chrono.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <string>

#include "config/config.h"
#include "os/os.h"

struct BytesToGiB {
  u64 value;
};

// 1024^3 (size of 1 GiB)
constexpr u64 GIB = 1'073'741'824;

template <>
struct fmt::formatter<BytesToGiB> : fmt::formatter<double> {
  template <typename FmtCtx>
  constexpr auto format(const BytesToGiB& BTG, FmtCtx& ctx) const -> typename FmtCtx::iterator {
    auto out = fmt::formatter<double>::format(static_cast<double>(BTG.value) / GIB, ctx);
    *out++   = 'G';
    *out++   = 'i';
    *out++   = 'B';
    return out;
  }
};

fn GetDate() -> std::string {
  // Get current local time
  std::time_t now       = std::time(nullptr);
  std::tm     localTime = *std::localtime(&now);

  // Format the date using fmt::format
  std::string date = fmt::format("{:%e}", localTime);

  // Remove leading whitespace
  if (!date.empty() && std::isspace(date.front()))
    date.erase(date.begin());

  // Append appropriate suffix for the date
  if (date == "1" || date == "21" || date == "31")
    date += "st";
  else if (date == "2" || date == "22")
    date += "nd";
  else if (date == "3" || date == "23")
    date += "rd";
  else
    date += "th";

  return fmt::format("{:%B} {}", localTime, date);
}

fn main() -> i32 {
  const Config& config = Config::getInstance();

  // Fetching weather information
  Weather       weather     = config.weather.get();
  WeatherOutput weatherInfo = weather.getWeatherInfo();

  // Fetching OS version
  std::string osVersion = GetOSVersion();

  // Checking if now playing is enabled
  bool nowPlayingEnabled = config.now_playing.get().enabled;

  // Fetching current date
  std::string date = GetDate();

  // Fetching memory info
  u64 memInfo = GetMemInfo();

  const std::string& name = config.general.get().name.get();

  fmt::println("Hello {}!", name);
  fmt::println("Today is: {}", date);
  fmt::println("Installed RAM: {:.2f}", BytesToGiB(memInfo));
  fmt::println("OS: {}", osVersion);

  if (weather.enabled)
    fmt::println("It is {}°F in {}", std::lround(weatherInfo.main.temp), weatherInfo.name);

  if (nowPlayingEnabled) {
    const string nowPlaying = GetNowPlaying();
    if (!nowPlaying.empty())
      fmt::println("{}", nowPlaying);
    else fmt::println("No song playing");
  }

  return 0;
}
