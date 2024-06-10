#include "config.h"

#define DEFINE_GETTER(class_name, type, name) \
  fn class_name::get##name() const->type { return m_##name; }

DEFINE_GETTER(Config, const General, General)
DEFINE_GETTER(Config, const NowPlaying, NowPlaying)
DEFINE_GETTER(Config, const Weather, Weather)
DEFINE_GETTER(General, const std::string, Name)
DEFINE_GETTER(NowPlaying, bool, Enabled)
DEFINE_GETTER(Weather, const Weather::Location, Location)
DEFINE_GETTER(Weather, const std::string, ApiKey)
DEFINE_GETTER(Weather, const std::string, Units)

fn Config::getInstance() -> const Config& {
  static const auto* INSTANCE =
      new Config(rfl::toml::load<Config>("./config.toml").value());
  return *INSTANCE;
}

Config::Config(General general, NowPlaying now_playing, Weather weather)
    : m_General(std::move(general)),
      m_NowPlaying(now_playing),
      m_Weather(std::move(weather)) {}

General::General(std::string name) : m_Name(std::move(name)) {}

NowPlaying::NowPlaying(bool enabled) : m_Enabled(enabled) {}

Weather::Weather(Location location, std::string api_key, std::string units)
    : m_Location(std::move(location)),
      m_ApiKey(std::move(api_key)),
      m_Units(std::move(units)) {}

fn WeatherImpl::from_class(const Weather& weather) noexcept -> WeatherImpl {
  return {
      .location = weather.getLocation(),
      .api_key  = weather.getApiKey(),
      .units    = weather.getUnits(),
  };
}

fn WeatherImpl::to_class() const -> Weather {
  return {location, api_key, units};
}

fn GeneralImpl::from_class(const General& general) noexcept -> GeneralImpl {
  return {general.getName()};
}

fn GeneralImpl::to_class() const -> General { return {name}; }

// clang-format off
fn NowPlayingImpl::from_class(
  const NowPlaying& now_playing
) noexcept -> NowPlayingImpl {
  return {.enabled = now_playing.getEnabled()};
}
//clang-format on

fn NowPlayingImpl::to_class() const -> NowPlaying { return {enabled}; }

fn ConfigImpl::from_class(const Config& config) noexcept -> ConfigImpl {
  return {
      .general     = config.getGeneral(),
      .now_playing = config.getNowPlaying(),
      .weather     = config.getWeather(),
  };
}

fn ConfigImpl::to_class() const -> Config {
  return {general, now_playing, weather};
}
