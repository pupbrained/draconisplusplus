#pragma once

#include "Services/Weather.hpp"

#include "Util/Error.hpp"

namespace weather {
  using util::types::Result;

  class IWeatherService {
   public:
    IWeatherService(const IWeatherService&) = delete;
    IWeatherService(IWeatherService&&)      = delete;

    fn operator=(const IWeatherService&)->IWeatherService& = delete;
    fn operator=(IWeatherService&&)->IWeatherService&      = delete;

    virtual ~IWeatherService() = default;

    [[nodiscard]] virtual fn getWeatherInfo() const -> Result<WeatherReport> = 0;

   protected:
    IWeatherService() = default;
  };
} // namespace weather