#include <ctime>
#include <fmt/chrono.h>
#include <fmt/core.h>
#include <future>

#include "config/config.h"
#include "os/os.h"

struct BytesToGiB {
  u64 value;
};

constexpr u64 GIB = 1'073'741'824;

template <>
struct fmt::formatter<BytesToGiB> : formatter<double> {
  template <typename FmtCtx>
  fn format(const BytesToGiB BTG, FmtCtx& ctx)->typename FmtCtx::iterator {
    typename FmtCtx::iterator out =
      formatter<double>::format(static_cast<double>(BTG.value) / GIB, ctx);
    *out++ = 'G';
    *out++ = 'i';
    *out++ = 'B';
    return out;
  }
};

fn GetDate()->string {
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

fn main()->i32 {
  using std::future;

  const Config& config = Config::getInstance();

  auto weatherFuture =
    std::async(std::launch::async, [&config]() { return config.weather.get().getWeatherInfo(); });

  auto osVersionFuture = std::async(std::launch::async, GetOSVersion);
  auto nowPlayingEnabledFuture =
    std::async(std::launch::async, [&config]() { return config.now_playing.get().enabled; });

  future<string> dateFuture    = std::async(std::launch::async, GetDate);
  future<u64>    memInfoFuture = std::async(std::launch::async, GetMemInfo);

  const auto
    [clouds,
     tz,
     visibility,
     main,
     coords,
     rain,
     snow,
     base,
     townName,
     weather,
     sys,
     cod,
     dt,
     id,
     wind] = weatherFuture.get();

  const i64 temp = std::lround(main.temp);

  const bool   nowPlayingEnabled = nowPlayingEnabledFuture.get();
  const string version           = osVersionFuture.get();
  const string date              = dateFuture.get();
  const string name              = config.general.get().name.get();
  const u64    mem               = memInfoFuture.get();

  fmt::println("Hello {}!", name);
  fmt::println("Today is: {}", date);
  fmt::println("It is {}°F in {}", temp, townName);
  fmt::println("Installed RAM: {:.2f}", BytesToGiB(mem));
  fmt::println("{}", version);

  if (nowPlayingEnabled)
    fmt::println("{}", GetNowPlaying());

  delete &config;
}
