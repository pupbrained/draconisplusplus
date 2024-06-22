#pragma once

#include <rfl.hpp>
#include <rfl/Field.hpp>

#include "../util/macros.h"
#include "../util/types.h"
#include "weather.h"

using Location = std::variant<string, Coords>;

struct General {
  rfl::Field<"name", string> name = "user";
};

struct NowPlaying {
  bool enabled = false;
};

struct Weather {
  Location location;
  string   api_key;
  string   units;

  [[nodiscard]] fn getWeatherInfo() const->WeatherOutput;
};

struct Config {
  rfl::Field<"general", General>        general     = General { .name = "user" };
  rfl::Field<"now_playing", NowPlaying> now_playing = NowPlaying();
  rfl::Field<"weather", Weather>        weather     = Weather();

  static fn getInstance()->Config;
};
