#pragma once

// clang-format off
// we need glaze.hpp include before any other includes that might use it
// because core/meta.hpp complains about not having uint8_t defined otherwise
#include <glaze/glaze.hpp>
#include <glaze/core/meta.hpp>
#include <glaze/json/read.hpp>
// clang-format on

#include "Util/Types.hpp" // For f64, String, Option, Vec, i32, i64

namespace weather {
  namespace dto {
    using ::util::types::f64, ::util::types::String, ::util::types::Option, ::util::types::Vec, ::util::types::i32, ::util::types::i64;

    // MetNo Data Transfer Objects
    namespace metno {
      struct Details {
        f64 airTemperature;
      };

      struct Next1hSummary {
        String symbolCode;
      };

      struct Next1h {
        Next1hSummary summary;
      };

      struct Instant {
        Details details;
      };

      struct Data {
        Instant        instant;
        Option<Next1h> next1Hours;
      };

      struct Timeseries {
        String time;
        Data   data;
      };

      struct Properties {
        Vec<Timeseries> timeseries;
      };

      struct Response {
        Properties properties;
      };
    } // namespace metno

    // OpenMeteo Data Transfer Objects
    namespace openmeteo {
      struct Response {
        struct Current {
          f64    temperature;
          i32    weathercode;
          String time;
        } currentWeather;
      };
    } // namespace openmeteo

    // OpenWeatherMap Data Transfer Objects
    namespace owm {
      struct OWMResponse {
        struct Main {
          f64 temp;
        };

        struct Weather {
          String description;
        };

        Main           main;
        Vec<Weather>   weather;
        String         name;
        i64            dt;
        Option<i32>    cod;
        Option<String> message;
      };
    } // namespace owm
  } // namespace dto
} // namespace weather

namespace glz {
  // MetNo Glaze meta definitions
  template <>
  struct meta<weather::dto::metno::Details> {
    using T                     = weather::dto::metno::Details;
    static constexpr auto value = object("air_temperature", &T::airTemperature);
  };

  template <>
  struct meta<weather::dto::metno::Next1hSummary> {
    using T                     = weather::dto::metno::Next1hSummary;
    static constexpr auto value = object("symbol_code", &T::symbolCode);
  };

  template <>
  struct meta<weather::dto::metno::Next1h> {
    using T                     = weather::dto::metno::Next1h;
    static constexpr auto value = object("summary", &T::summary);
  };

  template <>
  struct meta<weather::dto::metno::Instant> {
    using T                     = weather::dto::metno::Instant;
    static constexpr auto value = object("details", &T::details);
  };

  template <>
  struct meta<weather::dto::metno::Data> {
    using T                     = weather::dto::metno::Data;
    static constexpr auto value = object("instant", &T::instant, "next_1_hours", &T::next1Hours);
  };

  template <>
  struct meta<weather::dto::metno::Timeseries> {
    using T                     = weather::dto::metno::Timeseries;
    static constexpr auto value = object("time", &T::time, "data", &T::data);
  };

  template <>
  struct meta<weather::dto::metno::Properties> {
    using T                     = weather::dto::metno::Properties;
    static constexpr auto value = object("timeseries", &T::timeseries);
  };

  template <>
  struct meta<weather::dto::metno::Response> {
    using T                     = weather::dto::metno::Response;
    static constexpr auto value = object("properties", &T::properties);
  };

  // OpenMeteo Glaze meta definitions
  template <>
  struct meta<weather::dto::openmeteo::Response::Current> {
    using T                     = weather::dto::openmeteo::Response::Current;
    static constexpr auto value = object("temperature", &T::temperature, "weathercode", &T::weathercode, "time", &T::time);
  };

  template <>
  struct meta<weather::dto::openmeteo::Response> {
    using T                     = weather::dto::openmeteo::Response;
    static constexpr auto value = object("current_weather", &T::currentWeather);
  };

  // OpenWeatherMap Glaze meta definitions
  template <>
  struct meta<weather::dto::owm::OWMResponse::Main> {
    using T                     = weather::dto::owm::OWMResponse::Main;
    static constexpr auto value = object("temp", &T::temp);
  };

  template <>
  struct meta<weather::dto::owm::OWMResponse::Weather> {
    using T                     = weather::dto::owm::OWMResponse::Weather;
    static constexpr auto value = object("description", &T::description);
  };

  template <>
  struct meta<weather::dto::owm::OWMResponse> {
    using T                     = weather::dto::owm::OWMResponse;
    static constexpr auto value = object("main", &T::main, "weather", &T::weather, "name", &T::name, "dt", &T::dt, "cod", &T::cod, "message", &T::message);
  };
} // namespace glz
