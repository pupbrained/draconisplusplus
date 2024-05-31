#include <cpr/cpr.h>
#include <curl/curl.h>
#include <fmt/chrono.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <boost/json/src.hpp>
#include <ctime>
#include <toml++/toml.hpp>
#include "os/os.h"

using std::string;

struct General {
  string name;
};

struct NowPlaying {
  bool enable;
};

struct LatLon {
  double lat;
  double lon;
};

struct Location {
  enum {
    coords,
    city,
  } type;

  union {
    const char* city;
    LatLon coords;
  } data;
};

struct Weather {
  Location location;
  string api_key;
  string units;

  Weather() = default;

  Weather(const char* _city, string _api_key, string _units) {
    this->api_key = _api_key;
    this->units = _units;
    this->location = Location {
        Location::city,
        {.city = _city},
    };
  }

  Weather(LatLon _coords, string _api_key, string _units) {
    this->api_key = _api_key;
    this->units = _units;
    this->location = Location {
        Location::coords,
        {.coords = _coords},
    };
  }
};

struct Config {
  General general;
  NowPlaying now_playing;
  Weather weather;

  Config(toml::table toml) {
    general = General {toml["general"]["name"].value_or(getlogin())};

    now_playing = NowPlaying {toml["now_playing"]["enable"].value_or(false)};

    const auto location = toml["weather"]["location"];
    const string api_key = toml["weather"]["api_key"].value_or("");
    const string units = toml["weather"]["units"].value_or("metric");

    if (location.is_string())
      weather = Weather(location.value_or(""), api_key, units);
    else
      weather = Weather(
          LatLon {location["lat"].value_or(0.0), location["lon"].value_or(0.0)},
          api_key, units);
  }
};

static const Config& CONFIG() {
  static const Config& CONFIG = *new Config(toml::parse_file("./config.toml"));
  return CONFIG;
}

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
  using namespace cpr;
  using namespace boost::json;
  using namespace fmt;

  if (CONFIG().weather.location.type == Location::city) {
    const char* location = curl_easy_escape(
        nullptr, CONFIG().weather.location.data.city,
        static_cast<int>(strlen(CONFIG().weather.location.data.city)));
    const char* api_key = CONFIG().weather.api_key.c_str();
    const char* units = CONFIG().weather.units.c_str();

    const Response r =
        Get(Url {format("https://api.openweathermap.org/data/2.5/"
                        "weather?q={}&appid={}&units={}",
                        location, api_key, units)});

    value json = parse(r.text);

    return json.as_object();
  } else {
    const auto [lat, lon] = CONFIG().weather.location.data.coords;
    const char* api_key = CONFIG().weather.api_key.c_str();
    const char* units = CONFIG().weather.units.c_str();

    const Response r =
        Get(Url {format("https://api.openweathermap.org/data/2.5/"
                        "weather?lat={}&lon={}&appid={}&units={}",
                        lat, lon, api_key, units)});

    value json = parse(r.text);

    return json.as_object();
  }
}

int main() {
  using boost::json::object;
  using fmt::format;
  using fmt::localtime;
  using fmt::println;
  using std::time;
  using std::time_t;
  using toml::parse_result;

  const parse_result toml = toml::parse_file("./config.toml");

  if (CONFIG().now_playing.enable)
    println("{}", get_nowplaying());

  println("Hello {}!", CONFIG().general.name);

  const uint64_t meminfo = get_meminfo();

  println("{:.2f}", BytesToGiB {meminfo});

  const time_t t = time(nullptr);

  string date = format("{:%d}", localtime(t));

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

  println("{:%B} {}, {:%-I:%0M %p}", localtime(t), date, localtime(t));

  object json = get_weather();

  const char* town_name =
      json["name"].is_string() ? json["name"].as_string().c_str() : "Unknown";

  println("{}", town_name);

  return 0;
}
