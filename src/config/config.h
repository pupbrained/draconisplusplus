#pragma once

#include <toml++/toml.h>
#include <string>

using std::string;

typedef std::tuple<double, double> Coords;
typedef std::variant<string, Coords> Location;

class Weather {
 private:
  Location m_location;
  string m_api_key;
  string m_units;

 public:
  Weather(string city, string api_key, string units)
      : m_location(city), m_api_key(api_key), m_units(units) {}

  Weather(Coords coords, string api_key, string units)
      : m_location(coords), m_api_key(api_key), m_units(units) {}

  inline Location get_location() { return m_location; }
  inline string get_api_key() { return m_api_key; }
  inline string get_units() { return m_units; }
};

class General {
 private:
  string m_name;

 public:
  General(string name) { this->m_name = name; }

  inline string get_name() { return m_name; }
};

class NowPlaying {
 private:
  bool m_enable;

 public:
  NowPlaying(bool enable) { this->m_enable = enable; }

  inline bool get_enabled() { return m_enable; }
};

class Config {
 private:
  General m_general;
  NowPlaying m_now_playing;
  Weather m_weather;

 public:
  Config(toml::table toml);

  ~Config();

  inline Weather get_weather() { return m_weather; }
  inline General get_general() { return m_general; }
  inline NowPlaying get_now_playing() { return m_now_playing; }
};
