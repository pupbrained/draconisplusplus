#include "config.h"
#include <fmt/core.h>
#include <toml++/toml.h>
#include <unistd.h>

#define DEFINE_GETTER(class_name, type, name) \
  type class_name::get##name() const {        \
    return m_##name;                          \
  }

DEFINE_GETTER(Config, const General, General)
DEFINE_GETTER(Config, const NowPlaying, NowPlaying)
DEFINE_GETTER(Config, const Weather, Weather)
DEFINE_GETTER(General, const string, Name)
DEFINE_GETTER(NowPlaying, bool, Enabled)
DEFINE_GETTER(Weather, const Location, Location)
DEFINE_GETTER(Weather, const string, ApiKey)
DEFINE_GETTER(Weather, const string, Units)

const Config& Config::getInstance() {
  static const Config& INSTANCE =
      *new Config(toml::parse_file("./config.toml"));
  return INSTANCE;
}

Config::Config(toml::table toml)
    : m_General(toml["general"]["name"].value_or(getlogin())),
      m_NowPlaying(toml["now_playing"]["enable"].value_or(false)),
      m_Weather([location = toml["weather"]["location"],
                 apiKey   = toml["weather"]["api_key"].value_or(""),
                 units    = toml["weather"]["units"].value_or("metric")] {
        return location.is_string()
                   ? Weather(location.value_or(""), apiKey, units)
                   : Weather(
                         Coords {
                             .lat = location["lat"].value_or(0.0),
                             .lon = location["lon"].value_or(0.0),
                         },
                         apiKey, units);
      }()) {}

Config::Config(General general, NowPlaying now_playing, Weather weather)
    : m_General(std::move(general)),
      m_NowPlaying(now_playing),
      m_Weather(std::move(weather)) {}

General::General(string name) : m_Name(std::move(name)) {}

NowPlaying::NowPlaying(bool enable) : m_Enabled(enable) {}

Weather::Weather(Location location, string api_key, string units)
    : m_Location(std::move(location)),
      m_ApiKey(std::move(api_key)),
      m_Units(std::move(units)) {}

WeatherImpl WeatherImpl::from_class(const Weather& weather) noexcept {
  return {
      .location = weather.getLocation(),
      .api_key  = weather.getApiKey(),
      .units    = weather.getUnits(),
  };
}

Weather WeatherImpl::to_class() const {
  return {location, api_key, units};
}

GeneralImpl GeneralImpl::from_class(const General& general) noexcept {
  return {general.getName()};
}

General GeneralImpl::to_class() const {
  return {name};
}

NowPlayingImpl NowPlayingImpl::from_class(
    const NowPlaying& now_playing) noexcept {
  return {.enabled = now_playing.getEnabled()};
}

NowPlaying NowPlayingImpl::to_class() const {
  return {enabled};
}

ConfigImpl ConfigImpl::from_class(const Config& config) noexcept {
  return {
      .general     = config.getGeneral(),
      .now_playing = config.getNowPlaying(),
      .weather     = config.getWeather(),
  };
}

Config ConfigImpl::to_class() const {
  return {general, now_playing, weather};
}
