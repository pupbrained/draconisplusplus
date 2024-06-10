#include <ctime>
#include <fmt/chrono.h>
#include <fmt/core.h>

#include "config/config.h"
#include "os/os.h"

using std::string;

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

fn main() -> int {
  const Config& config = Config::getInstance();

  if (config.getNowPlaying().getEnabled())
    fmt::println("{}", GetNowPlaying());

  fmt::println("Hello {}!", config.getGeneral().getName());
  fmt::println("Installed RAM: {:.2f}", BytesToGiB {GetMemInfo()});
  fmt::println("Today is: {}", GetDate());

  Weather::WeatherOutput json = config.getWeather().getWeatherInfo();

  const long   temp     = std::lround(json.main.temp);
  const string townName = json.name;

  fmt::println("It is {}°F in {}", temp, townName);

  const char* version = GetOSVersion();

  fmt::println("{}", version);

  delete[] version;
  delete &config;

  return 0;
}
