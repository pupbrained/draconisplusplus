#pragma once

#include <variant>

#include "IWeatherService.hpp"

namespace weather {
  using util::types::StringView;

  class OpenWeatherMapService final : public IWeatherService {
   public:
    OpenWeatherMapService(std::variant<String, Coords> location, String apiKey, String units);
    fn getWeatherInfo() const -> Result<WeatherReport> override;

   private:
    std::variant<String, Coords> m_location;
    String                       m_apiKey;
    String                       m_units;
  };
} // namespace weather