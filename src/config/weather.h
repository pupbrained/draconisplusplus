#pragma once

#include <glaze/glaze.hpp>

#include "../util/types.h"

// NOLINTBEGIN(readability-identifier-naming)
struct Condition {
  String description;

  struct glaze {
    using T                     = Condition;
    static constexpr auto value = glz::object("description", &T::description);
  };
};

struct Main {
  f64 temp;

  struct glaze {
    using T                     = Main;
    static constexpr auto value = glz::object("temp", &T::temp);
  };
};

struct Coords {
  double lat;
  double lon;
};

struct WeatherOutput {
  Main                   main;
  String                 name;
  std::vector<Condition> weather;
  usize                  dt;

  struct glaze {
    using T                     = WeatherOutput;
    static constexpr auto value = glz::object("main", &T::main, "name", &T::name, "weather", &T::weather, "dt", &T::dt);
  };
};
// NOLINTEND(readability-identifier-naming)
