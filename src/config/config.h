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

using Location = std::variant<std::string, Coords>;

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
#endif
    return "User";
  }();

  static fn fromToml(const toml::table& tbl) -> General {
    General gen;
    if (auto name = tbl["name"].value<std::string>()) {
      gen.name = *name;
    } else {
#ifdef _WIN32
      std::array<char, 256> username;
      DWORD                 size = sizeof(username);
      g.name                     = GetUserNameA(username.data(), &size) ? username.data() : "User";
#else
      if (struct passwd* pwd = getpwuid(getuid()); pwd) {
        gen.name = pwd->pw_name;
      } else if (const char* envUser = getenv("USER")) {
        gen.name = envUser;
      } else {
        gen.name = "User";
      }
#endif
    }
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
  bool        enabled        = false;
  bool        show_town_name = false;
  Location    location;
  std::string api_key;
  std::string units;

  static fn fromToml(const toml::table& tbl) -> Weather {
    Weather weather;
    weather.enabled        = tbl["enabled"].value<bool>().value_or(false);
    weather.show_town_name = tbl["show_town_name"].value<bool>().value_or(false);
    weather.api_key        = tbl["api_key"].value<std::string>().value_or("");
    weather.units          = tbl["units"].value<std::string>().value_or("metric");

    if (auto location = tbl["location"]) {
      if (location.is_string()) {
        weather.location = location.value<std::string>().value();
      } else if (location.is_table()) {
        const auto* coord = location.as_table();
        Coords      coords;
        coords.lat       = coord->get("lat")->value<double>().value();
        coords.lon       = coord->get("lon")->value<double>().value();
        weather.location = coords;
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
