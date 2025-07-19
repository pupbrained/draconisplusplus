#pragma once

#if DRAC_ENABLE_WEATHER

// clang-format off
  // we need glaze.hpp include before any other includes that might use it
  // because core/meta.hpp complains about not having uint8_t defined otherwise
  #include <glaze/glaze.hpp>
  #include <glaze/core/meta.hpp>

  #include "Drac++/Utils/Types.hpp"
// clang-format on

namespace draconis::services::weather::dto {
  namespace {
    using draconis::utils::types::f64;
    using draconis::utils::types::i32;
    using draconis::utils::types::i64;
    using draconis::utils::types::Option;
    using draconis::utils::types::String;
    using draconis::utils::types::Vec;
  } // namespace

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
} // namespace draconis::services::weather::dto

namespace glz {
  namespace {
    using namespace draconis::services::weather::dto;
  } // namespace

  // MetNo Glaze meta definitions
  template <>
  struct meta<metno::Details> {
    static constexpr detail::Object value = object("air_temperature", &metno::Details::airTemperature);
  };

  template <>
  struct meta<metno::Next1hSummary> {
    static constexpr detail::Object value = object("symbol_code", &metno::Next1hSummary::symbolCode);
  };

  template <>
  struct meta<metno::Next1h> {
    static constexpr detail::Object value = object("summary", &metno::Next1h::summary);
  };

  template <>
  struct meta<metno::Instant> {
    static constexpr detail::Object value = object("details", &metno::Instant::details);
  };

  template <>
  struct meta<metno::Data> {
    static constexpr detail::Object value = object("instant", &metno::Data::instant, "next_1_hours", &metno::Data::next1Hours);
  };

  template <>
  struct meta<metno::Timeseries> {
    static constexpr detail::Object value = object("time", &metno::Timeseries::time, "data", &metno::Timeseries::data);
  };

  template <>
  struct meta<metno::Properties> {
    static constexpr detail::Object value = object("timeseries", &metno::Properties::timeseries);
  };

  template <>
  struct meta<metno::Response> {
    static constexpr detail::Object value = object("properties", &metno::Response::properties);
  };

  // OpenMeteo Glaze meta definitions
  template <>
  struct meta<openmeteo::Response::Current> {
    static constexpr detail::Object value = object("temperature", &openmeteo::Response::Current::temperature, "weathercode", &openmeteo::Response::Current::weathercode, "time", &openmeteo::Response::Current::time);
  };

  template <>
  struct meta<openmeteo::Response> {
    static constexpr detail::Object value = object("current_weather", &openmeteo::Response::currentWeather);
  };

  // OpenWeatherMap Glaze meta definitions
  template <>
  struct meta<owm::OWMResponse::Main> {
    static constexpr detail::Object value = object("temp", &owm::OWMResponse::Main::temp);
  };

  template <>
  struct meta<owm::OWMResponse::Weather> {
    static constexpr detail::Object value = object("description", &owm::OWMResponse::Weather::description);
  };

  template <>
  struct meta<owm::OWMResponse> {
    static constexpr detail::Object value = object("main", &owm::OWMResponse::main, "weather", &owm::OWMResponse::weather, "name", &owm::OWMResponse::name, "dt", &owm::OWMResponse::dt, "cod", &owm::OWMResponse::cod, "message", &owm::OWMResponse::message);
  };
} // namespace glz

#endif // DRAC_ENABLE_WEATHER