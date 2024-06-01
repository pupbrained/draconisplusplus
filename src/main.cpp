#include <cpr/cpr.h>
#include <curl/curl.h>
#include <fmt/chrono.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <boost/json/src.hpp>
#include <ctime>
#include <rfl.hpp>
#include <rfl/toml.hpp>
#include <rfl/toml/load.hpp>
#include <rfl/toml/read.hpp>
#include <toml++/toml.hpp>
#include <variant>
#include "config/config.h"
#include "os/os.h"

using std::string;

struct BytesToGiB {
  uint64_t value;
};

template <>
struct fmt::formatter<BytesToGiB> : formatter<double> {
  template <typename FormatContext>
  auto format(const BytesToGiB BTG, FormatContext& ctx) {
    auto out = formatter<double>::format(
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

boost::json::object GetWeather() {
  using namespace std;
  using namespace cpr;
  using namespace boost;

  const Config& config = Config::getInstance();

  Weather weather = config.getWeather();
  Location loc    = weather.getLocation();
  string apiKey   = weather.getApiKey();
  string units    = weather.getUnits();

  fmt::println("Hello!");

  if (holds_alternative<string>(loc)) {
    const string city = get<string>(loc);

    const char* location = curl_easy_escape(nullptr, city.c_str(),
                                            static_cast<int>(city.length()));

    const Response res =
        Get(Url {fmt::format("https://api.openweathermap.org/data/2.5/"
                             "weather?q={}&appid={}&units={}",
                             location, apiKey, units)});

    json::value json = json::parse(res.text);

    return json.as_object();
  }

  const auto [lat, lon] = get<Coords>(loc);

  const Response res =
      Get(Url {format("https://api.openweathermap.org/data/2.5/"
                      "weather?lat={:.3f}&lon={:.3f}&appid={}&units={}",
                      lat, lon, apiKey, units)});

  json::value json = json::parse(res.text);

  return json.as_object();
}

int main() {
  using boost::json::object;
  using std::time_t;

  const Config& config = rfl::toml::load<Config>("./config.toml").value();

  if (config.getNowPlaying().getEnabled())
    fmt::println("{}", GetNowPlaying());

  fmt::println("Hello {}!", config.getGeneral().getName());

  const uint64_t memInfo = GetMemInfo();

  fmt::println("{:.2f}", BytesToGiB {memInfo});

  const std::tm localTime = fmt::localtime(time(nullptr));

  auto trimStart = [](std::string& str) {
    auto start = str.begin();
    while (start != str.end() && std::isspace(*start)) {
      start++;
    }
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

  object json = GetWeather();

  const char* townName =
      json["name"].is_string() ? json["name"].as_string().c_str() : "Unknown";

  fmt::println("{}", townName);

  return 0;
}
