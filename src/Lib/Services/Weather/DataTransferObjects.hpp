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

namespace draconis::services::weather::dto {
  // MetNo Data Transfer Objects
  namespace metno {
    struct Details {
      draconis::utils::types::f64 airTemperature;
    };

    struct Next1hSummary {
      draconis::utils::types::String symbolCode;
    };

    struct Next1h {
      Next1hSummary summary;
    };

    struct Instant {
      Details details;
    };

    struct Data {
      Instant                                instant;
      draconis::utils::types::Option<Next1h> next1Hours;
    };

    struct Timeseries {
      draconis::utils::types::String time;
      Data                           data;
    };

    struct Properties {
      draconis::utils::types::Vec<Timeseries> timeseries;
    };

    struct Response {
      Properties properties;
    };
  } // namespace metno

  // OpenMeteo Data Transfer Objects
  namespace openmeteo {
    struct Response {
      struct Current {
        draconis::utils::types::f64    temperature;
        draconis::utils::types::i32    weathercode;
        draconis::utils::types::String time;
      } currentWeather;
    };
  } // namespace openmeteo

  // OpenWeatherMap Data Transfer Objects
  namespace owm {
    struct OWMResponse {
      struct Main {
        draconis::utils::types::f64 temp;
      };

      struct Weather {
        draconis::utils::types::String description;
      };

      Main                                                           main;
      draconis::utils::types::Vec<Weather>                           weather;
      draconis::utils::types::String                                 name;
      draconis::utils::types::i64                                    dt;
      draconis::utils::types::Option<draconis::utils::types::i32>    cod;
      draconis::utils::types::Option<draconis::utils::types::String> message;
    };
  } // namespace owm
} // namespace draconis::services::weather::dto

namespace glz {
  // MetNo Glaze meta definitions
  template <>
  struct meta<draconis::services::weather::dto::metno::Details> {
    static constexpr detail::Object value = object("air_temperature", &draconis::services::weather::dto::metno::Details::airTemperature);
  };

  template <>
  struct meta<draconis::services::weather::dto::metno::Next1hSummary> {
    static constexpr detail::Object value = object("symbol_code", &draconis::services::weather::dto::metno::Next1hSummary::symbolCode);
  };

  template <>
  struct meta<draconis::services::weather::dto::metno::Next1h> {
    static constexpr detail::Object value = object("summary", &draconis::services::weather::dto::metno::Next1h::summary);
  };

  template <>
  struct meta<draconis::services::weather::dto::metno::Instant> {
    static constexpr detail::Object value = object("details", &draconis::services::weather::dto::metno::Instant::details);
  };

  template <>
  struct meta<draconis::services::weather::dto::metno::Data> {
    static constexpr detail::Object value = object("instant", &draconis::services::weather::dto::metno::Data::instant, "next_1_hours", &draconis::services::weather::dto::metno::Data::next1Hours);
  };

  template <>
  struct meta<draconis::services::weather::dto::metno::Timeseries> {
    static constexpr detail::Object value = object("time", &draconis::services::weather::dto::metno::Timeseries::time, "data", &draconis::services::weather::dto::metno::Timeseries::data);
  };

  template <>
  struct meta<draconis::services::weather::dto::metno::Properties> {
    static constexpr detail::Object value = object("timeseries", &draconis::services::weather::dto::metno::Properties::timeseries);
  };

  template <>
  struct meta<draconis::services::weather::dto::metno::Response> {
    static constexpr detail::Object value = object("properties", &draconis::services::weather::dto::metno::Response::properties);
  };

  // OpenMeteo Glaze meta definitions
  template <>
  struct meta<draconis::services::weather::dto::openmeteo::Response::Current> {
    static constexpr detail::Object value = object("temperature", &draconis::services::weather::dto::openmeteo::Response::Current::temperature, "weathercode", &draconis::services::weather::dto::openmeteo::Response::Current::weathercode, "time", &draconis::services::weather::dto::openmeteo::Response::Current::time);
  };

  template <>
  struct meta<draconis::services::weather::dto::openmeteo::Response> {
    static constexpr detail::Object value = object("current_weather", &draconis::services::weather::dto::openmeteo::Response::currentWeather);
  };

  // OpenWeatherMap Glaze meta definitions
  template <>
  struct meta<draconis::services::weather::dto::owm::OWMResponse::Main> {
    static constexpr detail::Object value = object("temp", &draconis::services::weather::dto::owm::OWMResponse::Main::temp);
  };

  template <>
  struct meta<draconis::services::weather::dto::owm::OWMResponse::Weather> {
    static constexpr detail::Object value = object("description", &draconis::services::weather::dto::owm::OWMResponse::Weather::description);
  };

  template <>
  struct meta<draconis::services::weather::dto::owm::OWMResponse> {
    static constexpr detail::Object value = object("main", &draconis::services::weather::dto::owm::OWMResponse::main, "weather", &draconis::services::weather::dto::owm::OWMResponse::weather, "name", &draconis::services::weather::dto::owm::OWMResponse::name, "dt", &draconis::services::weather::dto::owm::OWMResponse::dt, "cod", &draconis::services::weather::dto::owm::OWMResponse::cod, "message", &draconis::services::weather::dto::owm::OWMResponse::message);
  };
} // namespace glz

#endif // DRAC_ENABLE_WEATHER