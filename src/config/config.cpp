#include "config.h"
#include <fmt/core.h>
#include <toml++/toml.h>
#include <unistd.h>

Weather make_weather(toml::node_view<toml::node> location,
                     const char* api_key,
                     const char* units) {
  return location.is_string()
             ? Weather(location.value_or(""), api_key, units)
             : Weather(std::make_tuple(location["lat"].value_or(0.0),
                                       location["lon"].value_or(0.0)),
                       api_key, units);
}

Config::Config(toml::table toml)
    : m_general(toml["general"]["name"].value_or(getlogin())),
      m_now_playing(toml["now_playing"]["enable"].value_or(false)),
      m_weather(make_weather(toml["weather"]["location"],
                             toml["weather"]["api_key"].value_or(""),
                             toml["weather"]["units"].value_or("metric"))) {}
