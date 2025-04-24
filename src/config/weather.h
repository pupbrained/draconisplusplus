#pragma once

#include <glaze/glaze.hpp>

#include "../util/types.h"

// NOLINTBEGIN(readability-identifier-naming) - Needs to specifically use `glaze`
struct Condition {
  String description;

  struct [[maybe_unused]] glaze {
    using T = Condition;

    static constexpr glz::detail::Object value = glz::object("description", &T::description);
  };
};

struct Main {
  f64 temp;

  struct [[maybe_unused]] glaze {
    using T = Main;

    static constexpr glz::detail::Object value = glz::object("temp", &T::temp);
  };
};

struct Coords {
  double lat;
  double lon;
};

struct WeatherOutput {
  Main           main;
  String         name;
  Vec<Condition> weather;
  usize          dt;

  struct [[maybe_unused]] glaze {
    using T = WeatherOutput;

    static constexpr glz::detail::Object value =
      glz::object("main", &T::main, "name", &T::name, "weather", &T::weather, "dt", &T::dt);
  };
};
// NOLINTEND(readability-identifier-naming)
