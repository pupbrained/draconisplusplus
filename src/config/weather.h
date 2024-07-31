#pragma once

#include <rfl.hpp>
#include <rfl/toml.hpp>

#include "../util/types.h"

using degrees    = rfl::Validator<u16, rfl::Minimum<0>, rfl::Maximum<360>>;
using percentage = rfl::Validator<i8, rfl::Minimum<0>, rfl::Maximum<100>>;

struct Condition {
  string description;
  string icon;
  string main;
  usize  id;
};

struct Main {
  f64                  feels_like;
  f64                  temp;
  f64                  temp_max;
  f64                  temp_min;
  isize                pressure;
  percentage           humidity;
  std::optional<isize> grnd_level;
  std::optional<isize> sea_level;
};

struct Wind {
  degrees            deg;
  f64                speed;
  std::optional<f64> gust;
};

struct Precipitation {
  rfl::Rename<"1h", f64> one_hour;
  rfl::Rename<"3h", f64> three_hours;
};

struct Sys {
  string country;
  usize  id;
  usize  sunrise;
  usize  sunset;
  usize  type;
};

struct Clouds {
  percentage all;
};

struct Coords {
  double lat;
  double lon;
};

struct WeatherOutput {
  Clouds                       clouds;
  isize                        timezone;
  isize                        visibility;
  Main                         main;
  rfl::Rename<"coord", Coords> coords;
  std::optional<Precipitation> rain;
  std::optional<Precipitation> snow;
  string                       base;
  string                       name;
  std::vector<Condition>       weather;
  Sys                          sys;
  usize                        cod;
  usize                        dt;
  usize                        id;
  Wind                         wind;
};
