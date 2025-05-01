#pragma once

#include <glaze/core/common.hpp> // object
#include <glaze/core/meta.hpp>   // Object

#include "src/util/types.hpp"

namespace weather {
  using glz::detail::Object, glz::object;
  using util::types::String, util::types::Vec, util::types::f64, util::types::usize;

  // NOLINTBEGIN(readability-identifier-naming) - Needs to specifically use `glaze`
  /**
   * @struct Condition
   * @brief Represents weather conditions.
   */
  struct Condition {
    String description; ///< Weather condition description (e.g., "clear sky", "light rain").

    /**
     * @brief Glaze serialization and deserialization for Condition.
     */
    struct [[maybe_unused]] glaze {
      using T = Condition;

      static constexpr Object value = object("description", &T::description);
    };
  };

  /**
   * @struct Main
   * @brief Represents the main weather data.
   */
  struct Main {
    f64 temp; ///< Temperature in degrees (C/F, depending on config).

    /**
     * @brief Glaze serialization and deserialization for Main.
     */
    struct [[maybe_unused]] glaze {
      using T = Main;

      static constexpr Object value = object("temp", &T::temp);
    };
  };

  /**
   * @struct Coords
   * @brief Represents geographical coordinates.
   */
  struct Coords {
    double lat; ///< Latitude coordinate.
    double lon; ///< Longitude coordinate.
  };

  /**
   * @struct Output
   * @brief Represents the output of the weather API.
   *
   * Contains main weather data, location name, and weather conditions.
   */
  struct Output {
    Main           main;    ///< Main weather data (temperature, etc.).
    String         name;    ///< Location name (e.g., city name).
    Vec<Condition> weather; ///< List of weather conditions (e.g., clear, rain).
    usize          dt;      ///< Timestamp of the weather data (in seconds since epoch).

    /**
     * @brief Glaze serialization and deserialization for WeatherOutput.
     */
    struct [[maybe_unused]] glaze {
      using T = Output;

      static constexpr Object value = object("main", &T::main, "name", &T::name, "weather", &T::weather, "dt", &T::dt);
    };
  };
  // NOLINTEND(readability-identifier-naming)
} // namespace weather
