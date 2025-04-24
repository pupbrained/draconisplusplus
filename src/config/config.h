#pragma once

#ifdef _WIN32
#include <windows.h>
#else
#include <pwd.h>
#include <unistd.h>
#endif

#include <toml++/toml.hpp>

#include "src/util/macros.h"
#include "weather.h"

using Location = std::variant<String, Coords>;

struct General {
  String name = []() -> String {
#ifdef _WIN32
    Array<char, 256> username;
    DWORD            size = sizeof(username);
    return GetUserNameA(username.data(), &size) ? username.data() : "User";
#else
    if (const passwd* pwd = getpwuid(getuid()))
      return pwd->pw_name;

    if (Result<String, EnvError> envUser = GetEnv("USER"))
      return *envUser;

    return "User";
#endif
  }();

  static fn fromToml(const toml::table& tbl) -> General {
    General gen;
    return {
      .name = tbl["name"].value_or(gen.name),
    };
  }
};

struct NowPlaying {
  bool enabled = false;

  static fn fromToml(const toml::table& tbl) -> NowPlaying { return { .enabled = tbl["enabled"].value_or(false) }; }
};

struct Weather {
  bool     enabled        = false;
  bool     show_town_name = false;
  Location location;
  String   api_key;
  String   units;

  static fn fromToml(const toml::table& tbl) -> Weather {
    Weather weather;

    const Option<String> apiKey = tbl["api_key"].value<String>();

    weather.enabled = tbl["enabled"].value_or<bool>(false) && apiKey;

    if (!weather.enabled)
      return weather;

    weather.api_key        = *apiKey;
    weather.show_town_name = tbl["show_town_name"].value_or(false);
    weather.units          = tbl["units"].value_or("metric");

    if (const toml::node_view<const toml::node> location = tbl["location"]) {
      if (location.is_string())
        weather.location = *location.value<String>();
      else if (location.is_table())
        weather.location = Coords {
          .lat = *location.as_table()->get("lat")->value<double>(),
          .lon = *location.as_table()->get("lon")->value<double>(),
        };
      else
        throw std::runtime_error("Invalid location type");
    }

    return weather;
  }

  [[nodiscard]] fn getWeatherInfo() const -> WeatherOutput;
};

struct Config {
  General    general;
  NowPlaying now_playing;
  Weather    weather;

  static fn fromToml(const toml::table& tbl) -> Config {
    const toml::node_view genTbl = tbl["general"];
    const toml::node_view npTbl  = tbl["now_playing"];
    const toml::node_view wthTbl = tbl["weather"];

    return {
      .general     = genTbl.is_table() ? General::fromToml(*genTbl.as_table()) : General {},
      .now_playing = npTbl.is_table() ? NowPlaying::fromToml(*npTbl.as_table()) : NowPlaying {},
      .weather     = wthTbl.is_table() ? Weather::fromToml(*wthTbl.as_table()) : Weather {},
    };
  }

  static fn getInstance() -> Config;
};
