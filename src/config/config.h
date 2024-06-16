#pragma once

#include <rfl.hpp>
#include <string>

#include "util/macros.h"
#include "weather.h"

// TODO: Make config values optional and supply defaults

class General {
 private:
  std::string m_Name;

 public:
  explicit General(std::string name);

  [[nodiscard]] fn getName() const -> const std::string;
};

class NowPlaying {
 private:
  bool m_Enabled;

 public:
  explicit NowPlaying(bool enabled);

  [[nodiscard]] fn getEnabled() const -> bool;
};

class Config {
 private:
  General    m_General;
  NowPlaying m_NowPlaying;
  Weather    m_Weather;

 public:
  /**
   * @brief Creates a new Config instance.
   *
   * @param general     The general section of the configuration.
   * @param now_playing The now playing section of the configuration.
   * @param weather     The weather section of the configuration.
   */
  Config(General general, NowPlaying now_playing, Weather weather);

  /**
   * @brief Gets the current (read-only) configuration.
   *
   * @return The current Config instance.
   */
  static fn getInstance() -> const Config&;

  [[nodiscard]] fn getWeather() const -> const Weather;
  [[nodiscard]] fn getGeneral() const -> const General;
  [[nodiscard]] fn getNowPlaying() const -> const NowPlaying;
};

// reflect-cpp Stuff
DEF_IMPL(General, std::string name)
DEF_IMPL(NowPlaying, std::optional<bool> enabled)
DEF_IMPL(Config, General general; NowPlaying now_playing; Weather weather)

namespace rfl::parsing {
  template <class ReaderType, class WriterType, class ProcessorsType>
  struct Parser<ReaderType, WriterType, General, ProcessorsType>
    : CustomParser<ReaderType, WriterType, ProcessorsType, General, GeneralImpl> {};

  template <class ReaderType, class WriterType, class ProcessorsType>
  struct Parser<ReaderType, WriterType, NowPlaying, ProcessorsType>
    : CustomParser<ReaderType, WriterType, ProcessorsType, NowPlaying, NowPlayingImpl> {};

  template <class ReaderType, class WriterType, class ProcessorsType>
  struct Parser<ReaderType, WriterType, Config, ProcessorsType>
    : CustomParser<ReaderType, WriterType, ProcessorsType, Config, ConfigImpl> {};
}
