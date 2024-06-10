#pragma once

#include <fmt/core.h>
#include <rfl.hpp>
#include <rfl/toml.hpp>
#include <string>
#include <variant>

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
    Main                         main;
    Sys                          sys;
    Wind                         wind;
    isize                        timezone;
    isize                        visibility;
    rfl::Rename<"coord", Coords> coords;
    std::optional<Precipitation> rain;
    std::optional<Precipitation> snow;
    std::string                  base;
    std::string                  name;
    std::vector<Condition>       weather;
    usize                        cod;
    usize                        dt;
    usize                        id;
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

struct WeatherImpl {
  Weather::Location location;
  std::string       api_key;
  std::string       units;

  static fn from_class(const Weather& weather) noexcept -> WeatherImpl;

  [[nodiscard]] fn to_class() const -> Weather;
};

class General {
 private:
  std::string m_Name;

 public:
  General(std::string name);

  [[nodiscard]] fn getName() const -> const std::string;
};

struct GeneralImpl {
  std::string name;

  static fn from_class(const General& general) noexcept -> GeneralImpl;

  [[nodiscard]] fn to_class() const -> General;
};

class NowPlaying {
 private:
  bool m_Enabled;

 public:
  NowPlaying(bool enabled);

  [[nodiscard]] fn getEnabled() const -> bool;
};

struct NowPlayingImpl {
  bool enabled;

  static fn from_class(const NowPlaying& now_playing
  ) noexcept -> NowPlayingImpl;

  [[nodiscard]] fn to_class() const -> NowPlaying;
};

class Config {
 private:
  General    m_General;
  NowPlaying m_NowPlaying;
  Weather    m_Weather;

 public:
  Config(General general, NowPlaying now_playing, Weather weather);

  static fn getInstance() -> const Config&;

  [[nodiscard]] fn getWeather() const -> const Weather;
  [[nodiscard]] fn getGeneral() const -> const General;
  [[nodiscard]] fn getNowPlaying() const -> const NowPlaying;
};

struct ConfigImpl {
  General    general;
  NowPlaying now_playing;
  Weather    weather;

  static fn from_class(const Config& config) noexcept -> ConfigImpl;

  [[nodiscard]] fn to_class() const -> Config;
};

// Parsers for Config classes
namespace rfl::parsing {
  template <class ReaderType, class WriterType, class ProcessorsType>
  struct Parser<ReaderType, WriterType, Weather, ProcessorsType>
      : public CustomParser<
            ReaderType,
            WriterType,
            ProcessorsType,
            Weather,
            WeatherImpl> {};

  template <class ReaderType, class WriterType, class ProcessorsType>
  struct Parser<ReaderType, WriterType, General, ProcessorsType>
      : public CustomParser<
            ReaderType,
            WriterType,
            ProcessorsType,
            General,
            GeneralImpl> {};

  template <class ReaderType, class WriterType, class ProcessorsType>
  struct Parser<ReaderType, WriterType, NowPlaying, ProcessorsType>
      : public CustomParser<
            ReaderType,
            WriterType,
            ProcessorsType,
            NowPlaying,
            NowPlayingImpl> {};

  template <class ReaderType, class WriterType, class ProcessorsType>
  struct Parser<ReaderType, WriterType, Config, ProcessorsType>
      : public CustomParser<
            ReaderType,
            WriterType,
            ProcessorsType,
            Config,
            ConfigImpl> {};
} // namespace rfl::parsing
