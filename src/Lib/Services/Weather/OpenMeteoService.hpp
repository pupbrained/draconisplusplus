#pragma once

#if DRAC_ENABLE_WEATHER

  #include "Drac++/Services/Weather.hpp"

  #include "Drac++/Utils/Types.hpp"

namespace draconis::services::weather {
  class OpenMeteoService final : public IWeatherService {
   public:
    OpenMeteoService(utils::types::f64 lat, utils::types::f64 lon, UnitSystem units);
    [[nodiscard]] fn getWeatherInfo() const -> utils::types::Result<Report> override;

   private:
    utils::types::f64 m_lat;
    utils::types::f64 m_lon;
    UnitSystem        m_units;
  };
} // namespace draconis::services::weather

#endif // DRAC_ENABLE_WEATHER
