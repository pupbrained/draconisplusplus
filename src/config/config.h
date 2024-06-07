#pragma once

#include <fmt/core.h>
#include <rfl.hpp>
#include <rfl/toml.hpp>
#include <string>
#include <variant>

#include "util/numtypes.h"

class Weather {
 public:
  using percentage = rfl::Validator<i8, rfl::Minimum<0>, rfl::Maximum<100>>;
  using degrees    = rfl::Validator<u16, rfl::Minimum<0>, rfl::Maximum<360>>;

  struct Condition {
    usize id;
    rfl::Rename<"main", std::string> group;
    std::string description;
    rfl::Rename<"icon", std::string> icon_id;
  };

  struct Main {
    f64 temp;
    f64 temp_max;
    f64 temp_min;
    f64 feels_like;
    isize pressure;
    std::optional<isize> sea_level;
    std::optional<isize> grnd_level;
    percentage humidity;
  };

  struct Wind {
    f64 speed;
    degrees deg;
    std::optional<f64> gust;
  };

  struct Precipitation {
    rfl::Rename<"1h", f64> one_hour;
    rfl::Rename<"3h", f64> three_hours;
  };

  struct Sys {
    std::string country;
    usize id;
    usize sunrise;
    usize sunset;
    usize type;
  };

  struct Clouds {
    percentage all;
  };

  struct Coords {
    double lat;
    double lon;
  };

  struct WeatherOutput {
    isize timezone;
    isize visibility;
    Main main;
    Clouds clouds;
    rfl::Rename<"coord", Coords> coords;
    std::optional<Precipitation> rain;
    std::optional<Precipitation> snow;
    std::vector<Condition> weather;
    std::string base;
    std::string name;
    Sys sys;
    usize cod;
    usize dt;
    usize id;
    Wind wind;
  };

  using Location = std::variant<std::string, Coords>;

 private:
  Location m_Location;
  std::string m_ApiKey;
  std::string m_Units;

 public:
  Weather(Location location, std::string api_key, std::string units);

  [[nodiscard]] WeatherOutput getWeatherInfo() const;
  [[nodiscard]] const Location getLocation() const;
  [[nodiscard]] const std::string getApiKey() const;
  [[nodiscard]] const std::string getUnits() const;
};

struct WeatherImpl {
  Weather::Location location;
  std::string api_key;
  std::string units;

  static WeatherImpl from_class(const Weather& weather) noexcept;

  [[nodiscard]] Weather to_class() const;
};

class General {
 private:
  std::string m_Name;

 public:
  General(std::string name);

  [[nodiscard]] const std::string getName() const;
};

struct GeneralImpl {
  std::string name;

  static GeneralImpl from_class(const General& general) noexcept;

  [[nodiscard]] General to_class() const;
};

class NowPlaying {
 private:
  bool m_Enabled;

 public:
  NowPlaying(bool enabled);

  [[nodiscard]] bool getEnabled() const;
};

struct NowPlayingImpl {
  bool enabled;

  static NowPlayingImpl from_class(const NowPlaying& now_playing) noexcept;

  [[nodiscard]] NowPlaying to_class() const;
};

class Config {
 private:
  General m_General;
  NowPlaying m_NowPlaying;
  Weather m_Weather;

 public:
  Config(General general, NowPlaying now_playing, Weather weather);

  static const Config& getInstance();

  [[nodiscard]] const Weather getWeather() const;
  [[nodiscard]] const General getGeneral() const;
  [[nodiscard]] const NowPlaying getNowPlaying() const;
};

struct ConfigImpl {
  General general;
  NowPlaying now_playing;
  Weather weather;

  static ConfigImpl from_class(const Config& config) noexcept;

  [[nodiscard]] Config to_class() const;
};

// Parsers for Config classes
namespace rfl::parsing {
  template <class ReaderType, class WriterType, class ProcessorsType>
  struct Parser<ReaderType, WriterType, Weather, ProcessorsType>
      : public CustomParser<ReaderType,
                            WriterType,
                            ProcessorsType,
                            Weather,
                            WeatherImpl> {};

  template <class ReaderType, class WriterType, class ProcessorsType>
  struct Parser<ReaderType, WriterType, General, ProcessorsType>
      : public CustomParser<ReaderType,
                            WriterType,
                            ProcessorsType,
                            General,
                            GeneralImpl> {};

  template <class ReaderType, class WriterType, class ProcessorsType>
  struct Parser<ReaderType, WriterType, NowPlaying, ProcessorsType>
      : public CustomParser<ReaderType,
                            WriterType,
                            ProcessorsType,
                            NowPlaying,
                            NowPlayingImpl> {};

  template <class ReaderType, class WriterType, class ProcessorsType>
  struct Parser<ReaderType, WriterType, Config, ProcessorsType>
      : public CustomParser<ReaderType,
                            WriterType,
                            ProcessorsType,
                            Config,
                            ConfigImpl> {};
} // namespace rfl::parsing
