#pragma once

#include <nlohmann/json.hpp>

#include "../util/types.h"

using degrees    = unsigned short; // 0-360, validate manually
using percentage = unsigned char;  // 0-100, validate manually

struct Condition {
  std::string description;
  std::string icon;
  std::string main;
  usize       id;
};

struct Main {
  f64                  feels_like;
  f64                  temp;
  f64                  temp_max;
  f64                  temp_min;
  isize                pressure;
  percentage           humidity;
  std::optional<isize> grnd_level;
  std::optional<isize> sea_level;
};

struct Wind {
  degrees            deg;
  f64                speed;
  std::optional<f64> gust;
};

struct Precipitation {
  std::optional<f64> one_hour;
  std::optional<f64> three_hours;
};

struct Sys {
  std::string country;
  usize       id;
  usize       sunrise;
  usize       sunset;
  usize       type;
};

struct Clouds {
  percentage all;
};

struct Coords {
  double lat;
  double lon;
};

struct WeatherOutput {
  Clouds                       clouds;
  isize                        timezone;
  isize                        visibility;
  Main                         main;
  Coords                       coords; // JSON key: "coord"
  std::optional<Precipitation> rain;
  std::optional<Precipitation> snow;
  std::string                  base;
  std::string                  name;
  std::vector<Condition>       weather;
  Sys                          sys;
  usize                        cod;
  usize                        dt;
  usize                        id;
  Wind                         wind;
};

// JSON Serialization Definitions
// NOLINTBEGIN(readability-identifier-naming)
namespace nlohmann {
  using namespace nlohmann;
  template <>
  struct adl_serializer<Condition> {
    static void to_json(json& jsonOut, const Condition& cond) {
      jsonOut = json {
        { "description", cond.description },
        {        "icon",        cond.icon },
        {        "main",        cond.main },
        {          "id",          cond.id }
      };
    }

    static void from_json(const json& jsonIn, Condition& cond) {
      jsonIn.at("description").get_to(cond.description);
      jsonIn.at("icon").get_to(cond.icon);
      jsonIn.at("main").get_to(cond.main);
      jsonIn.at("id").get_to(cond.id);
    }
  };

  template <>
  struct adl_serializer<Main> {
    static void to_json(json& jsonOut, const Main& main) {
      jsonOut = json {
        { "feels_like", main.feels_like },
        {       "temp",       main.temp },
        {   "temp_max",   main.temp_max },
        {   "temp_min",   main.temp_min },
        {   "pressure",   main.pressure },
        {   "humidity",   main.humidity }
      };
      if (main.grnd_level)
        jsonOut["grnd_level"] = *main.grnd_level;
      if (main.sea_level)
        jsonOut["sea_level"] = *main.sea_level;
    }

    static void from_json(const json& jsonIn, Main& main) {
      jsonIn.at("feels_like").get_to(main.feels_like);
      jsonIn.at("temp").get_to(main.temp);
      jsonIn.at("temp_max").get_to(main.temp_max);
      jsonIn.at("temp_min").get_to(main.temp_min);
      jsonIn.at("pressure").get_to(main.pressure);
      jsonIn.at("humidity").get_to(main.humidity);
      if (jsonIn.contains("grnd_level"))
        main.grnd_level = jsonIn["grnd_level"].get<isize>();
      if (jsonIn.contains("sea_level"))
        main.sea_level = jsonIn["sea_level"].get<isize>();
    }
  };

  template <>
  struct adl_serializer<Wind> {
    static void to_json(json& jsonOut, const Wind& wind) {
      jsonOut = json {
        {   "deg",   wind.deg },
        { "speed", wind.speed }
      };
      if (wind.gust)
        jsonOut["gust"] = *wind.gust;
    }
    static void from_json(const json& jsonIn, Wind& wind) {
      jsonIn.at("deg").get_to(wind.deg);
      jsonIn.at("speed").get_to(wind.speed);
      if (jsonIn.contains("gust"))
        wind.gust = jsonIn["gust"].get<f64>();
      // Validate degrees (0-360)
      if (wind.deg > 360)
        throw std::runtime_error("Invalid wind degree");
    }
  };

  template <>
  struct adl_serializer<Precipitation> {
    static void to_json(json& jsonOut, const Precipitation& precip) {
      if (precip.one_hour)
        jsonOut["1h"] = *precip.one_hour;
      if (precip.three_hours)
        jsonOut["3h"] = *precip.three_hours;
    }

    static void from_json(const json& jsonIn, Precipitation& precip) {
      if (jsonIn.contains("1h"))
        precip.one_hour = jsonIn["1h"].get<f64>();
      if (jsonIn.contains("3h"))
        precip.three_hours = jsonIn["3h"].get<f64>();
    }
  };

  template <>
  struct adl_serializer<Sys> {
    static void to_json(json& jsonOut, const Sys& sys) {
      jsonOut = json {
        { "country", sys.country },
        {      "id",      sys.id },
        { "sunrise", sys.sunrise },
        {  "sunset",  sys.sunset },
        {    "type",    sys.type }
      };
    }

    static void from_json(const json& jsonIn, Sys& sys) {
      jsonIn.at("country").get_to(sys.country);
      jsonIn.at("id").get_to(sys.id);
      jsonIn.at("sunrise").get_to(sys.sunrise);
      jsonIn.at("sunset").get_to(sys.sunset);
      jsonIn.at("type").get_to(sys.type);
    }
  };

  template <>
  struct adl_serializer<Clouds> {
    static void to_json(json& jsonOut, const Clouds& clouds) {
      jsonOut = json {
        { "all", clouds.all }
      };
    }

    static void from_json(const json& jsonIn, Clouds& clouds) { jsonIn.at("all").get_to(clouds.all); }
  };

  template <>
  struct adl_serializer<Coords> {
    static void to_json(json& jsonOut, const Coords& coords) {
      jsonOut = json {
        { "lat", coords.lat },
        { "lon", coords.lon }
      };
    }
    static void from_json(const json& jsonIn, Coords& coords) {
      jsonIn.at("lat").get_to(coords.lat);
      jsonIn.at("lon").get_to(coords.lon);
    }
  };

  template <>
  struct adl_serializer<WeatherOutput> {
    static void to_json(json& jsonOut, const WeatherOutput& weatherOut) {
      jsonOut = json {
        {     "clouds",     weatherOut.clouds },
        {   "timezone",   weatherOut.timezone },
        { "visibility", weatherOut.visibility },
        {       "main",       weatherOut.main },
        {      "coord",     weatherOut.coords },
        {       "base",       weatherOut.base },
        {       "name",       weatherOut.name },
        {    "weather",    weatherOut.weather },
        {        "sys",        weatherOut.sys },
        {        "cod",        weatherOut.cod },
        {         "dt",         weatherOut.dt },
        {         "id",         weatherOut.id },
        {       "wind",       weatherOut.wind }
      };
      if (weatherOut.rain)
        jsonOut["rain"] = *weatherOut.rain;
      if (weatherOut.snow)
        jsonOut["snow"] = *weatherOut.snow;
    }

    static void from_json(const json& jsonIn, WeatherOutput& weatherOut) {
      jsonIn.at("clouds").get_to(weatherOut.clouds);
      jsonIn.at("timezone").get_to(weatherOut.timezone);
      jsonIn.at("visibility").get_to(weatherOut.visibility);
      jsonIn.at("main").get_to(weatherOut.main);
      jsonIn.at("coord").get_to(weatherOut.coords);
      if (jsonIn.contains("rain"))
        weatherOut.rain = jsonIn["rain"].get<Precipitation>();
      if (jsonIn.contains("snow"))
        weatherOut.snow = jsonIn["snow"].get<Precipitation>();
      jsonIn.at("base").get_to(weatherOut.base);
      jsonIn.at("name").get_to(weatherOut.name);
      jsonIn.at("weather").get_to(weatherOut.weather);
      jsonIn.at("sys").get_to(weatherOut.sys);
      jsonIn.at("cod").get_to(weatherOut.cod);
      jsonIn.at("dt").get_to(weatherOut.dt);
      jsonIn.at("id").get_to(weatherOut.id);
      jsonIn.at("wind").get_to(weatherOut.wind);
    }
  };
}
// NOLINTEND(readability-identifier-naming)
