#pragma once

#if DRAC_ENABLE_WEATHER

// clang-format off
#include <glaze/core/common.hpp> // object
#include <glaze/core/meta.hpp>   // Object

#include "Util/Types.hpp"
// clang-format on

namespace weather {
  /**
   * @struct WeatherReport
   * @brief Represents a weather report.
   *
   * Contains temperature, conditions, and timestamp.
   */
  struct WeatherReport {
    util::types::f64                         temperature; ///< Degrees (C/F)
    util::types::Option<util::types::String> name;        ///< Optional town/city name (may be missing for some providers)
    util::types::String                      description; ///< Weather description (e.g., "clear sky", "rain")
  };

  struct Coords {
    util::types::f64 lat;
    util::types::f64 lon;
  };
} // namespace weather

namespace glz {
  template <>
  struct meta<weather::WeatherReport> {
    // clang-format off
    static constexpr detail::Object value = object(
      "temperature", &weather::WeatherReport::temperature,
      "name",        &weather::WeatherReport::name,
      "description", &weather::WeatherReport::description
    );
    // clang-format on
  };
} // namespace glz

#endif // DRAC_ENABLE_WEATHER