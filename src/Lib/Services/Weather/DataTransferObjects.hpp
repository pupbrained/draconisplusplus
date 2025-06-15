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
      util::types::f64 airTemperature;
    };

    struct Next1hSummary {
      util::types::SZString symbolCode;
    };

    struct Next1h {
      Next1hSummary summary;
    };

    struct Instant {
      Details details;
    };

    struct Data {
      Instant                     instant;
      util::types::Option<Next1h> next1Hours;
    };

    struct Timeseries {
      util::types::SZString time;
      Data                  data;
    };

    struct Properties {
      util::types::Vec<Timeseries> timeseries;
    };

    struct Response {
      Properties properties;
    };
  } // namespace metno

  // OpenMeteo Data Transfer Objects
  namespace openmeteo {
    struct Response {
      struct Current {
        util::types::f64      temperature;
        util::types::i32      weathercode;
        util::types::SZString time;
      } currentWeather;
    };
  } // namespace openmeteo

  // OpenWeatherMap Data Transfer Objects
  namespace owm {
    struct OWMResponse {
      struct Main {
        util::types::f64 temp;
      };

      struct Weather {
        util::types::SZString description;
      };

      Main                                       main;
      util::types::Vec<Weather>                  weather;
      util::types::SZString                      name;
      util::types::i64                           dt;
      util::types::Option<util::types::i32>      cod;
      util::types::Option<util::types::SZString> message;
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