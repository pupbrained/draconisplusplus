#include <ctime>
#include <fmt/chrono.h>
#include <fmt/core.h>

#include "config/config.h"
#include "os/os.h"

struct BytesToGiB {
  u64 value;
};

constexpr u64 GIB = 1'073'741'824;

template <>
struct fmt::formatter<BytesToGiB> : formatter<double> {
  template <typename FmtCtx>
  fn format(const BytesToGiB BTG, FmtCtx& ctx) -> typename FmtCtx::iterator {
    typename FmtCtx::iterator out =
      formatter<double>::format(static_cast<double>(BTG.value) / GIB, ctx);
    *out++ = 'G';
    *out++ = 'i';
    *out++ = 'B';
    return out;
  }
};

fn GetDate() -> string {
  const std::tm localTime = fmt::localtime(time(nullptr));

  string date = fmt::format("{:%e}", localTime);

  if (!date.empty() && std::isspace(date.front()))
    date.erase(date.begin());

  if (date == "1" || date == "21" || date == "31")
    date += "st";
  else if (date == "2" || date == "22")
    date += "nd";
  else if (date == "3" || date == "23")
    date += "rd";
  else
    date += "th";

  return fmt::format("{:%B} {}, {:%-I:%0M %p}", localTime, date, localTime);
}

fn main() -> i32 {
  const Config& config = Config::getInstance();

  // Fetching weather information
  auto weatherInfo = config.weather.get().getWeatherInfo();

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
  fmt::println("{}", osVersion);

  if (config.weather.get().enabled) {
    const auto& [clouds, tz, visibility, main, coords, rain, snow, base, townName, weather, sys, cod, dt, id, wind] =
      weatherInfo;
    i64 temp = std::lround(main.temp);

    fmt::println("It is {}°F in {}", temp, townName);
  }

  if (nowPlayingEnabled) {
    fmt::println("{}", GetNowPlaying());
  }

  return 0;
}
