#include <cpr/cpr.h>
#include <curl/curl.h>
#include <fmt/chrono.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <boost/json/src.hpp>
#include <toml++/toml.hpp>

import OS;

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
  enum { coords, city } type;
  union {
    LatLon coords;
    const char* city;
  } data;
};

void use_location(const Location& l) {
  switch (l.type) {
    case Location::city:
      {
        char* c = const_cast<char*>(l.data.city);
        puts(c);
        free(c);
        break;
      }
    case Location::coords:
      {
        const auto [lat, lon] = l.data.coords;
        printf("%.f, %.f", lon, lat);
        break;
      }
  }
}

struct Weather {
  Location location;
  string api_key;
  string units;
};

struct Config {
  General general;
  NowPlaying now_playing;
  Weather weather;

  explicit Config(auto toml) {
    general = General{.name = toml["general"]["name"].value_or(getlogin())};

    now_playing =
        NowPlaying{.enable = toml["now_playing"]["enable"].value_or(false)};

    const char* location = toml["weather"]["location"].value_or("");

    const size_t len = strlen(location);
    char* loc = new char[len + 1];
    strncpy(loc, location, len);
    loc[len] = '\0';

    if (toml["weather"]["location"].is_string()) {
      weather = Weather{
          .location = Location{.type = Location::city, .data = {.city = loc}},
          .api_key = toml["weather"]["api_key"].value_or(""),
          .units = toml["weather"]["units"].value_or("metric"),
      };
    } else {
      weather = Weather{
          .location =
              Location{
                  .type = Location::coords,
                  .data = {.coords =
                               LatLon{
                                   toml["weather"]["location"]["lat"].value_or(
                                       0.0),
                                   toml["weather"]["location"]["lon"].value_or(
                                       0.0)}}},
          .api_key = toml["weather"]["api_key"].value_or(""),
          .units = toml["weather"]["units"].value_or("metric"),
      };
    }
  }
};

static Config CONFIG = Config(toml::parse_file("./config.toml"));

struct BytesToGiB {
  uint64_t value;
};

template <>
struct fmt::formatter<BytesToGiB> : formatter<double> {
  template <typename FormatContext>
  auto format(const BytesToGiB b, FormatContext& ctx) {
    auto out = formatter<double>::format(
        b.value / pow(1024, 3), ctx);  // NOLINT(*-narrowing-conversions);
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

  if (CONFIG.weather.location.type == Location::city) {
    const char* location =
        curl_easy_escape(nullptr, CONFIG.weather.location.data.city,
                         strlen(CONFIG.weather.location.data.city));
    const char* api_key = CONFIG.weather.api_key.c_str();
    const char* units = CONFIG.weather.units.c_str();

    const Response r =
        Get(Url{format("https://api.openweathermap.org/data/2.5/"
                       "weather?q={}&appid={}&units={}",
                       location, api_key, units)});

    value json = parse(r.text);

    return json.as_object();
  } else {
    const auto [lat, lon] = CONFIG.weather.location.data.coords;
    const char* api_key = CONFIG.weather.api_key.c_str();
    const char* units = CONFIG.weather.units.c_str();

    const Response r =
        Get(Url{format("https://api.openweathermap.org/data/2.5/"
                       "weather?lat={}&lon={}&appid={}&units={}",
                       lat, lon, api_key, units)});

    value json = parse(r.text);

    return json.as_object();
  }
}

int main() {
  const auto toml = toml::parse_file("./config.toml");

  if (CONFIG.now_playing.enable)
    fmt::println("{}", get_nowplaying());

  fmt::println("Hello {}!", CONFIG.general.name);

  const uint64_t meminfo = get_meminfo();

  fmt::println("{:.2f}", BytesToGiB{meminfo});

  const std::time_t t = std::time(nullptr);

  string date = fmt::format("{:%d}", fmt::localtime(t));

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

  fmt::println("{:%B} {}, {:%-I:%0M %p}", fmt::localtime(t), date,
               fmt::localtime(t));

  boost::json::object json = get_weather();

  const char* town_name =
      json["name"].is_string() ? json["name"].as_string().c_str() : "Unknown";

  fmt::println("{}", town_name);

  return 0;
}
