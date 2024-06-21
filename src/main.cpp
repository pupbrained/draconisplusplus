#include <ctime>
#include <fmt/chrono.h>
#include <fmt/core.h>
#include <future>

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

  auto weatherFuture =
    std::async(std::launch::async, [&config]() { return config.weather.get().getWeatherInfo(); });

  auto osVersionFuture = std::async(std::launch::async, GetOSVersion);
  auto nowPlayingEnabledFuture =
    std::async(std::launch::async, [&config]() { return config.now_playing.get().enabled; });

  auto dateFuture    = std::async(std::launch::async, GetDate);
  auto memInfoFuture = std::async(std::launch::async, GetMemInfo);

  WeatherOutput     json     = weatherFuture.get();
  const long        temp     = std::lround(json.main.temp);
  const std::string townName = json.name;

  const char* version = osVersionFuture.get();

  const std::string name              = config.general.value().name.get();
  const bool        nowPlayingEnabled = nowPlayingEnabledFuture.get();

  fmt::println("Hello {}!", name);
  fmt::println("Today is: {}", dateFuture.get());
  fmt::println("It is {}°F in {}", temp, townName);
  fmt::println("Installed RAM: {:.2f} GiB", BytesToGiB(memInfoFuture.get()));
  fmt::println("{}", version);

  if (nowPlayingEnabled) {
    fmt::println("{}", GetNowPlaying());
  }

  delete[] version;

  return 0;
}
