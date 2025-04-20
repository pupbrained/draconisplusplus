#pragma once

#ifdef _WIN32
#include <windows.h>
#else
#include <pwd.h>    // For getpwuid
#include <unistd.h> // For getuid
#endif

#include <toml++/toml.hpp>

#include "src/util/macros.h"
#include "weather.h"

using Location = std::variant<string, Coords>;

struct General {
  string name = []() -> string {
#ifdef _WIN32
    std::array<char, 256> username;
    DWORD                 size = sizeof(username);
    return GetUserNameA(username.data(), &size) ? username.data() : "User";
#else
    if (struct passwd* pwd = getpwuid(getuid()); pwd)
      return pwd->pw_name;

    if (const char* envUser = getenv("USER"))
      return envUser;

    return "User";
#endif
  }();

  static fn fromToml(const toml::table& tbl) -> General {
    General gen;

    if (const std::optional<string> name = tbl["name"].value<string>())
      gen.name = *name;

    return gen;
  }
};

struct NowPlaying {
  bool enabled = false;

  static fn fromToml(const toml::table& tbl) -> NowPlaying {
    NowPlaying nowPlaying;
    nowPlaying.enabled = tbl["enabled"].value<bool>().value_or(false);
    return nowPlaying;
  }
};

struct Weather {
  bool     enabled        = false;
  bool     show_town_name = false;
  Location location;
  string   api_key;
  string   units;

  static fn fromToml(const toml::table& tbl) -> Weather {
    Weather weather;
    weather.enabled = tbl["enabled"].value_or<bool>(false);

    if (auto apiKey = tbl["api_key"].value<string>()) {
      const string& keyVal = apiKey.value();

      if (keyVal.empty())
        weather.enabled = false;

      weather.api_key = keyVal;
    } else {
      weather.enabled = false;
    }

    if (!weather.enabled)
      return weather;

    weather.show_town_name = tbl["show_town_name"].value_or<bool>(false);
    weather.units          = tbl["units"].value<string>().value_or("metric");

    if (const toml::node_view<const toml::node> location = tbl["location"]) {
      if (location.is_string()) {
        weather.location = location.value<string>().value();
      } else if (location.is_table()) {
        const auto* coord = location.as_table();
        Coords      coords;
        coords.lat       = coord->get("lat")->value<double>().value();
        coords.lon       = coord->get("lon")->value<double>().value();
        weather.location = coords;
      } else {
        throw std::runtime_error("Invalid location type");
      }
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
    Config cfg;

    if (const auto* general = tbl["general"].as_table())
      cfg.general = General::fromToml(*general);

    if (const auto* nowPlaying = tbl["now_playing"].as_table())
      cfg.now_playing = NowPlaying::fromToml(*nowPlaying);

    if (const auto* weather = tbl["weather"].as_table())
      cfg.weather = Weather::fromToml(*weather);

    return cfg;
  }

  static fn getInstance() -> Config;
};
