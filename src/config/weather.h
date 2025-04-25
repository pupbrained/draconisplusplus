#pragma once

#include <glaze/glaze.hpp>

#include "../util/types.h"

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

    static constexpr glz::detail::Object value = glz::object("description", &T::description);
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

    static constexpr glz::detail::Object value = glz::object("temp", &T::temp);
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
 * @struct WeatherOutput
 * @brief Represents the output of the weather API.
 *
 * Contains main weather data, location name, and weather conditions.
 */
struct WeatherOutput {
  Main           main;    ///< Main weather data (temperature, etc.).
  String         name;    ///< Location name (e.g., city name).
  Vec<Condition> weather; ///< List of weather conditions (e.g., clear, rain).
  usize          dt;      ///< Timestamp of the weather data (in seconds since epoch).

  /**
   * @brief Glaze serialization and deserialization for WeatherOutput.
   */
  struct [[maybe_unused]] glaze {
    using T = WeatherOutput;

    static constexpr glz::detail::Object value =
      glz::object("main", &T::main, "name", &T::name, "weather", &T::weather, "dt", &T::dt);
  };
};
// NOLINTEND(readability-identifier-naming)
