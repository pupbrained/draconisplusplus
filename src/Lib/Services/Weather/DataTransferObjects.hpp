#pragma once

#if DRAC_ENABLE_WEATHER

// clang-format off
  // we need glaze.hpp include before any other includes that might use it
  // because core/meta.hpp complains about not having uint8_t defined otherwise
  #include <glaze/glaze.hpp>
  #include <glaze/core/meta.hpp>
  #include <glaze/json/read.hpp>

  #include "DracUtils/Types.hpp"
// clang-format on

namespace weather::dto {
  // MetNo Data Transfer Objects
  namespace metno {
    struct Details {
      drac::types::f64 airTemperature;
    };

    struct Next1hSummary {
      drac::types::String symbolCode;
    };

    struct Next1h {
      Next1hSummary summary;
    };

    struct Instant {
      Details details;
    };

    struct Data {
      Instant                     instant;
      drac::types::Option<Next1h> next1Hours;
    };

    struct Timeseries {
      drac::types::String time;
      Data                data;
    };

    struct Properties {
      drac::types::Vec<Timeseries> timeseries;
    };

    struct Response {
      Properties properties;
    };
  } // namespace metno

  // OpenMeteo Data Transfer Objects
  namespace openmeteo {
    struct Response {
      struct Current {
        drac::types::f64    temperature;
        drac::types::i32    weathercode;
        drac::types::String time;
      } currentWeather;
    };
  } // namespace openmeteo

  // OpenWeatherMap Data Transfer Objects
  namespace owm {
    struct OWMResponse {
      struct Main {
        drac::types::f64 temp;
      };

      struct Weather {
        drac::types::String description;
      };

      Main                                     main;
      drac::types::Vec<Weather>                weather;
      drac::types::String                      name;
      drac::types::i64                         dt;
      drac::types::Option<drac::types::i32>    cod;
      drac::types::Option<drac::types::String> message;
    };
  } // namespace owm
} // namespace weather::dto

namespace glz {
  // MetNo Glaze meta definitions
  template <>
  struct meta<weather::dto::metno::Details> {
    static constexpr detail::Object value = object("air_temperature", &weather::dto::metno::Details::airTemperature);
  };

  template <>
  struct meta<weather::dto::metno::Next1hSummary> {
    static constexpr detail::Object value = object("symbol_code", &weather::dto::metno::Next1hSummary::symbolCode);
  };

  template <>
  struct meta<weather::dto::metno::Next1h> {
    static constexpr detail::Object value = object("summary", &weather::dto::metno::Next1h::summary);
  };

  template <>
  struct meta<weather::dto::metno::Instant> {
    static constexpr detail::Object value = object("details", &weather::dto::metno::Instant::details);
  };

  template <>
  struct meta<weather::dto::metno::Data> {
    static constexpr detail::Object value = object("instant", &weather::dto::metno::Data::instant, "next_1_hours", &weather::dto::metno::Data::next1Hours);
  };

  template <>
  struct meta<weather::dto::metno::Timeseries> {
    static constexpr detail::Object value = object("time", &weather::dto::metno::Timeseries::time, "data", &weather::dto::metno::Timeseries::data);
  };

  template <>
  struct meta<weather::dto::metno::Properties> {
    static constexpr detail::Object value = object("timeseries", &weather::dto::metno::Properties::timeseries);
  };

  template <>
  struct meta<weather::dto::metno::Response> {
    static constexpr detail::Object value = object("properties", &weather::dto::metno::Response::properties);
  };

  // OpenMeteo Glaze meta definitions
  template <>
  struct meta<weather::dto::openmeteo::Response::Current> {
    static constexpr detail::Object value = object("temperature", &weather::dto::openmeteo::Response::Current::temperature, "weathercode", &weather::dto::openmeteo::Response::Current::weathercode, "time", &weather::dto::openmeteo::Response::Current::time);
  };

  template <>
  struct meta<weather::dto::openmeteo::Response> {
    static constexpr detail::Object value = object("current_weather", &weather::dto::openmeteo::Response::currentWeather);
  };

  // OpenWeatherMap Glaze meta definitions
  template <>
  struct meta<weather::dto::owm::OWMResponse::Main> {
    static constexpr detail::Object value = object("temp", &weather::dto::owm::OWMResponse::Main::temp);
  };

  template <>
  struct meta<weather::dto::owm::OWMResponse::Weather> {
    static constexpr detail::Object value = object("description", &weather::dto::owm::OWMResponse::Weather::description);
  };

  template <>
  struct meta<weather::dto::owm::OWMResponse> {
    static constexpr detail::Object value = object("main", &weather::dto::owm::OWMResponse::main, "weather", &weather::dto::owm::OWMResponse::weather, "name", &weather::dto::owm::OWMResponse::name, "dt", &weather::dto::owm::OWMResponse::dt, "cod", &weather::dto::owm::OWMResponse::cod, "message", &weather::dto::owm::OWMResponse::message);
  };
} // namespace glz

#endif // DRAC_ENABLE_WEATHER