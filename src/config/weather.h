#pragma once

#include <fmt/core.h>
#include <rfl.hpp>
#include <rfl/toml.hpp>
#include <string>
#include <variant>

#include "util/macros.h"
#include "util/numtypes.h"

class Weather {
 public:
  using degrees    = rfl::Validator<u16, rfl::Minimum<0>, rfl::Maximum<360>>;
  using percentage = rfl::Validator<i8, rfl::Minimum<0>, rfl::Maximum<100>>;

  struct Condition {
    std::string description;
    std::string icon;
    std::string main;
    usize       id;
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
    std::string country;
    usize       id;
    usize       sunrise;
    usize       sunset;
    usize       type;
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
    std::string                  base;
    std::string                  name;
    std::vector<Condition>       weather;
    Sys                          sys;
    usize                        cod;
    usize                        dt;
    usize                        id;
    Wind                         wind;
  };

  using Location = std::variant<std::string, Coords>;

 private:
  Location    m_Location;
  std::string m_ApiKey;
  std::string m_Units;

 public:
  Weather(Location location, std::string api_key, std::string units);

  [[nodiscard]] fn getWeatherInfo() const -> WeatherOutput;
  [[nodiscard]] fn getLocation() const -> const Location;
  [[nodiscard]] fn getApiKey() const -> const std::string;
  [[nodiscard]] fn getUnits() const -> const std::string;
};

DEF_IMPL(Weather, Weather::Location location; std::string api_key; std::string units)

namespace rfl::parsing {
  template <class ReaderType, class WriterType, class ProcessorsType>
  struct Parser<ReaderType, WriterType, Weather, ProcessorsType>
    : CustomParser<ReaderType, WriterType, ProcessorsType, Weather, WeatherImpl> {};
}
