#pragma once

#include <glaze/core/common.hpp> // object
#include <glaze/core/meta.hpp>   // Object

#include "Util/Types.hpp"

namespace weather {
  using glz::detail::Object, glz::object;
  using util::types::String, util::types::Vec, util::types::f64, util::types::usize;

  /**
   * @struct WeatherReport
   * @brief Represents a weather report.
   *
   * Contains temperature, conditions, and timestamp.
   */
  struct WeatherReport {
    f64                         temperature; ///< Degrees (C/F)
    util::types::Option<String> name;        ///< Optional town/city name (may be missing for some providers)
    String                      description; ///< Weather description (e.g., "clear sky", "rain")
    usize                       timestamp;   ///< Seconds since epoch
  };

  struct WeatherReportGlaze {
    using T = WeatherReport;

    // clang-format off
    static constexpr Object value = object(
      "temperature", &T::temperature,
      "name", &T::name,
      "description", &T::description,
      "timestamp", &T::timestamp
    );
    // clang-format on
  };

  struct Coords {
    f64 lat;
    f64 lon;
  };
} // namespace weather

template <>
struct glz::meta<weather::WeatherReport> : weather::WeatherReportGlaze {};
