#include "config.h"

// -------------
// -- Weather --
// -------------
DEFINE_GETTER(Weather, const Weather::Location, Location)
DEFINE_GETTER(Weather, const std::string, ApiKey)
DEFINE_GETTER(Weather, const std::string, Units)

Weather::Weather(Location location, std::string api_key, std::string units)
    : m_Location(std::move(location)), m_ApiKey(std::move(api_key)), m_Units(std::move(units)) {}

fn WeatherImpl::from_class(const Weather& weather) noexcept -> WeatherImpl {
  return {
      .location = weather.getLocation(),
      .api_key  = weather.getApiKey(),
      .units    = weather.getUnits(),
  };
}

fn WeatherImpl::to_class() const -> Weather { return {location, api_key, units}; }
// ------------

// -------------
// -- General --
// -------------
DEFINE_GETTER(General, const std::string, Name)

General::General(std::string name) : m_Name(std::move(name)) {}

fn GeneralImpl::from_class(const General& general) -> GeneralImpl { return {general.getName()}; }

fn GeneralImpl::to_class() const -> General { return {name}; }
// -------------

// ----------------
// -- NowPlaying --
// ----------------
DEFINE_GETTER(NowPlaying, bool, Enabled)

NowPlaying::NowPlaying(bool enabled) : m_Enabled(enabled) {}

fn NowPlayingImpl::from_class(const NowPlaying& now_playing) -> NowPlayingImpl {
  return {.enabled = now_playing.getEnabled()};
}

fn NowPlayingImpl::to_class() const -> NowPlaying { return {enabled.value_or(false)}; }
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

fn ConfigImpl::from_class(const Config& config) -> ConfigImpl {
  return {
      .general     = config.getGeneral(),
      .now_playing = config.getNowPlaying(),
      .weather     = config.getWeather(),
  };
}

fn ConfigImpl::to_class() const -> Config { return {general, now_playing, weather}; }
// ------------
