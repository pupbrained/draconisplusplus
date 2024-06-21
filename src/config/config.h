#pragma once

#include <cstdlib>
#include <rfl.hpp>
#include <rfl/Field.hpp>
#include <rfl/default.hpp>
#include <string>
#include <util/macros.h>

#include "weather.h"

using Location = std::variant<std::string, Coords>;

struct General {
  rfl::Field<"name", std::string> name = "user";
};

struct NowPlaying {
  bool enabled = false;
};

struct Weather {
  Location    location;
  std::string api_key;
  std::string units;

  fn getWeatherInfo() const -> WeatherOutput;
};

struct Config {
  rfl::Field<"general", General>        general     = General { .name = "user" };
  rfl::Field<"now_playing", NowPlaying> now_playing = NowPlaying();
  rfl::Field<"weather", Weather>        weather     = Weather();

  static fn getInstance() -> const Config&;
};
