#pragma once

#include <fmt/core.h>
#include <toml++/toml.h>
#include <unistd.h>
#include <rfl.hpp>
#include <string>
#include <toml++/impl/parser.hpp>
#include <variant>

using std::string;

struct Coords {
  double lat;
  double lon;
};

using Location = std::variant<string, Coords>;

class Weather {
 private:
  Location m_Location;
  string m_ApiKey;
  string m_Units;

 public:
  Weather(Location location, string api_key, string units);

  [[nodiscard]] const Location getLocation() const;
  [[nodiscard]] const string getApiKey() const;
  [[nodiscard]] const string getUnits() const;
};

struct WeatherImpl {
  Location location;
  string api_key;
  string units;

  static WeatherImpl from_class(const Weather& weather) noexcept;

  [[nodiscard]] Weather to_class() const;
};

class General {
 private:
  string m_Name;

 public:
  General(string name);

  [[nodiscard]] const string getName() const;
};

struct GeneralImpl {
  string name;

  static GeneralImpl from_class(const General& general) noexcept;

  [[nodiscard]] General to_class() const;
};

class NowPlaying {
 private:
  bool m_Enabled;

 public:
  NowPlaying(bool enable);

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

  Config(toml::table toml);

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
