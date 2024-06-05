#include <co/log.h>
#include <ctime>
#include <fmt/chrono.h>
#include <fmt/core.h>

#include "config/config.h"
#include "os/macos/NowPlayingBridge.h"
#include "os/os.h"

using std::string;

struct BytesToGiB {
  uint64_t value;
};

template <>
struct fmt::formatter<BytesToGiB> : formatter<double> {
  template <typename FormatContext>
  typename FormatContext::iterator format(const BytesToGiB BTG,
                                          FormatContext& ctx) {
    typename FormatContext::iterator out = formatter<double>::format(
        static_cast<double>(BTG.value) / pow(1024, 3), ctx);
    *out++ = 'G';
    *out++ = 'i';
    *out++ = 'B';
    return out;
  }
};

enum DateNum { Ones, Twos, Threes, Default };

DateNum ParseDate(string const& input) {
  if (input == "1" || input == "21" || input == "31")
    return Ones;

  if (input == "2" || input == "22")
    return Twos;

  if (input == "3" || input == "23")
    return Threes;

  return Default;
}

int main(int argc, char** argv) {
  flag::parse(argc, argv);

  LOG << "hello " << 23;

  const Config& config = Config::getInstance();

  if (config.getNowPlaying().getEnabled())
    fmt::println("{}", GetNowPlaying());

  fmt::println("Hello {}!", config.getGeneral().getName());

  const uint64_t memInfo = GetMemInfo();

  fmt::println("{:.2f}", BytesToGiB {memInfo});

  const std::tm localTime = fmt::localtime(time(nullptr));

  auto trimStart = [](std::string& str) {
    auto start = str.begin();
    while (start != str.end() && std::isspace(*start))
      ++start;
    str.erase(str.begin(), start);
  };

  string date = fmt::format("{:%e}", localTime);

  trimStart(date);

  switch (ParseDate(date)) {
    case Ones:
      date += "st";
      break;

    case Twos:
      date += "nd";
      break;

    case Threes:
      date += "rd";
      break;

    case Default:
      date += "th";
      break;
  }

  fmt::println("{:%B} {}, {:%-I:%0M %p}", localTime, date, localTime);

  co::Json json = config.getWeather().getWeatherInfo();

  const int temp = json.get("main", "temp").as_int();
  const char* townName =
      json["name"].is_string() ? json["name"].as_string().c_str() : "Unknown";

  fmt::println("It is {}°F in {}", temp, townName);

  return 0;
}
