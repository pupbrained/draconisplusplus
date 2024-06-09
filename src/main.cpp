#include <ctime>
#include <fmt/chrono.h>
#include <fmt/core.h>

#include "config/config.h"
#include "os/os.h"

using std::string;

struct BytesToGiB {
  u64 value;
};

template <>
struct fmt::formatter<BytesToGiB> : formatter<double> {
  template <typename FormatContext>
  typename FormatContext::iterator
  format(const BytesToGiB BTG, FormatContext& ctx) {
    typename FormatContext::iterator out = formatter<double>::format(
        static_cast<double>(BTG.value) / pow(1024, 3), ctx
    );
    *out++ = 'G';
    *out++ = 'i';
    *out++ = 'B';
    return out;
  }
};

enum DateNum { Ones, Twos, Threes, Default };

DateNum ParseDate(string const& input) {
  if (input == "1" || input == "21" || input == "31") return Ones;
  if (input == "2" || input == "22") return Twos;
  if (input == "3" || input == "23") return Threes;
  return Default;
}

int main() {
  const Config& config = Config::getInstance();

  if (config.getNowPlaying().getEnabled()) fmt::println("{}", GetNowPlaying());

  fmt::println("Hello {}!", config.getGeneral().getName());

  const u64 memInfo = GetMemInfo();

  fmt::println("{:.2f}", BytesToGiB {memInfo});

  const std::tm localTime = fmt::localtime(time(nullptr));

  string date = fmt::format("{:%e}", localTime);

  auto start = date.begin();
  while (start != date.end() && std::isspace(*start)) ++start;
  date.erase(date.begin(), start);

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
