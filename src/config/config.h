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

class NowPlaying {
 private:
  bool m_Enabled;

 public:
  NowPlaying(bool enabled);

  [[nodiscard]] fn getEnabled() const -> bool;
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

// Reflect-CPP Stuff
DEF_IMPL(General, general, std::string name)
DEF_IMPL(NowPlaying, now_playing, std::optional<bool> enabled)
DEF_IMPL(Config, config, General general; NowPlaying now_playing; Weather weather)

namespace rfl::parsing {
  template <class ReaderType, class WriterType, class ProcessorsType>
  struct Parser<ReaderType, WriterType, General, ProcessorsType>
      : public CustomParser<ReaderType, WriterType, ProcessorsType, General, GeneralImpl> {};

  template <class ReaderType, class WriterType, class ProcessorsType>
  struct Parser<ReaderType, WriterType, NowPlaying, ProcessorsType>
      : public CustomParser<ReaderType, WriterType, ProcessorsType, NowPlaying, NowPlayingImpl> {};

  template <class ReaderType, class WriterType, class ProcessorsType>
  struct Parser<ReaderType, WriterType, Config, ProcessorsType>
      : public CustomParser<ReaderType, WriterType, ProcessorsType, Config, ConfigImpl> {};
}
