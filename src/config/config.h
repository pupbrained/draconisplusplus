#pragma once

#include <fmt/core.h>
#include <rfl.hpp>
#include <rfl/toml.hpp>
#include <string>

#include "util/macros.h"
#include "weather.h"

// TODO: Make config values optional and supply defaults

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
  std::optional<bool> enabled;

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
}
