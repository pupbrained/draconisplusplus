#pragma once

#if DRAC_ENABLE_WEATHER

// clang-format off
#include "Services/Weather.hpp"

#include "Util/Error.hpp"
// clang-format on

namespace weather {
  class IWeatherService {
   public:
    IWeatherService(const IWeatherService&) = delete;
    IWeatherService(IWeatherService&&)      = delete;

    fn operator=(const IWeatherService&)->IWeatherService& = delete;
    fn operator=(IWeatherService&&)->IWeatherService&      = delete;

    virtual ~IWeatherService() = default;

    [[nodiscard]] virtual fn getWeatherInfo() const -> util::types::Result<WeatherReport> = 0;

   protected:
    IWeatherService() = default;
  };
} // namespace weather

#endif // DRAC_ENABLE_WEATHER