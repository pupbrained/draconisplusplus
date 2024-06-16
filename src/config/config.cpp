#include "config.h"

// -------------
// -- General --
// -------------
DEFINE_GETTER(General, const std::string, Name)

General::General(std::string name) : m_Name(std::move(name)) {}

fn GeneralImpl::from_class(const General& instance) noexcept -> GeneralImpl {
  return { instance.getName() };
}

fn GeneralImpl::to_class() const -> General { return General { name }; }
// -------------

// ----------------
// -- NowPlaying --
// ----------------
DEFINE_GETTER(NowPlaying, bool, Enabled)

NowPlaying::NowPlaying(bool enabled) : m_Enabled(enabled) {}

fn NowPlayingImpl::from_class(const NowPlaying& instance) noexcept -> NowPlayingImpl {
  return { .enabled = instance.getEnabled() };
}

fn NowPlayingImpl::to_class() const -> NowPlaying { return NowPlaying { enabled.value_or(false) }; }
// ----------------

// ------------
// -- Config --
// ------------
DEFINE_GETTER(Config, const General, General)
DEFINE_GETTER(Config, const NowPlaying, NowPlaying)
DEFINE_GETTER(Config, const Weather, Weather)

Config::Config(General general, NowPlaying now_playing, Weather weather)
  : m_General(std::move(general)), m_NowPlaying(now_playing), m_Weather(std::move(weather)) {}

fn Config::getInstance() -> const Config& {
  static const auto* INSTANCE = new Config(rfl::toml::load<Config>("./config.toml").value());
  return *INSTANCE;
}

fn ConfigImpl::from_class(const Config& instance) noexcept -> ConfigImpl {
  return {
    instance.getGeneral(),
    instance.getNowPlaying(),
    instance.getWeather(),
  };
}

fn ConfigImpl::to_class() const -> Config { return { general, now_playing, weather }; }
// ------------
