#include <cpr/cpr.h>
#include <curl/curl.h>
#include <fmt/chrono.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <boost/json/src.hpp>
#include <ctime>
#include <toml++/toml.hpp>
#include <variant>
#include "config/config.h"
#include "os/os.h"

using std::string;

static Config& CONFIG = *new Config(toml::parse_file("./config.toml"));

struct BytesToGiB {
  uint64_t value;
};

template <>
struct fmt::formatter<BytesToGiB> : formatter<double> {
  template <typename FormatContext>
  auto format(const BytesToGiB b, FormatContext& ctx) {
    auto out = formatter<double>::format(
        static_cast<double>(b.value) / pow(1024, 3), ctx);
    *out++ = 'G';
    *out++ = 'i';
    *out++ = 'B';
    return out;
  }
};

enum date_num { Ones, Twos, Threes, Default };

date_num parse_date(string const& input) {
  if (input == "1" || input == "21" || input == "31")
    return Ones;

  if (input == "2" || input == "22")
    return Twos;

  if (input == "3" || input == "23")
    return Threes;

  return Default;
}

boost::json::object get_weather() {
  using namespace std;
  using namespace cpr;
  using namespace boost;

  Weather weather = CONFIG.get_weather();
  Location loc = weather.get_location();
  string api_key = weather.get_api_key();
  string units = weather.get_units();

  if (holds_alternative<string>(loc)) {
    const string city = get<string>(loc);

    const char* location = curl_easy_escape(nullptr, city.c_str(),
                                            static_cast<int>(city.length()));

    const Response r =
        Get(Url {fmt::format("https://api.openweathermap.org/data/2.5/"
                             "weather?q={}&appid={}&units={}",
                             location, api_key, units)});

    json::value json = json::parse(r.text);

    return json.as_object();
  } else {
    const auto [lat, lon] = get<Coords>(loc);

    const Response r =
        Get(Url {format("https://api.openweathermap.org/data/2.5/"
                        "weather?lat={}&lon={}&appid={}&units={}",
                        lat, lon, api_key, units)});

    json::value json = json::parse(r.text);

    return json.as_object();
  }
}

int main() {
  using boost::json::object;
  using std::time_t;

  if (CONFIG.get_now_playing().get_enabled())
    fmt::println("{}", get_nowplaying());

  fmt::println("Hello {}!", CONFIG.get_general().get_name());

  const uint64_t meminfo = get_meminfo();

  fmt::println("{:.2f}", BytesToGiB {meminfo});

  const std::tm t = fmt::localtime(time(nullptr));

  string date = fmt::format("{:%d}", t);

  switch (parse_date(date)) {
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

  fmt::println("{:%B} {}, {:%-I:%0M %p}", t, date, t);

  object json = get_weather();

  const char* town_name =
      json["name"].is_string() ? json["name"].as_string().c_str() : "Unknown";

  fmt::println("{}", town_name);

  return 0;
}
