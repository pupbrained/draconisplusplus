#pragma once

#include <rfl.hpp>
#include <rfl/Field.hpp>
#include <windows.h>

#include "../util/macros.h"
#include "../util/types.h"
#include "weather.h"

using Location = std::variant<string, Coords>;

struct General {
  // TODO: implement for the other OSes idiot
  string name =
#ifdef _WIN32
    []() -> string {
    std::array<char, 256> username;
    DWORD                 size = sizeof(username);

    if (GetUserNameA(username.data(), &size))
      return { username.data() };

    return "Unknown";
  }()
#elif defined(__linux__)
    "Linux"
#elif defined(__APPLE__)
    "MacOS"
#else
    "Unknown"
#endif
    ;
};

struct NowPlaying {
  bool enabled = false;
};

struct Weather {
  bool enabled        = false;
  bool show_town_name = false;

  Location location;
  string   api_key;
  string   units;

  [[nodiscard]] fn getWeatherInfo() const -> WeatherOutput;
};

struct Config {
  rfl::Field<"general", General>        general     = General();
  rfl::Field<"now_playing", NowPlaying> now_playing = NowPlaying();
  rfl::Field<"weather", Weather>        weather     = Weather();

  static fn getInstance() -> Config;
};
