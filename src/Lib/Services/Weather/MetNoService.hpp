#pragma once

#if DRAC_ENABLE_WEATHER

  #include "Drac++/Services/Weather.hpp"

  #include "Drac++/Utils/Types.hpp"

namespace draconis::services::weather {
  class MetNoService final : public IWeatherService {
   public:
    MetNoService(utils::types::f64 lat, utils::types::f64 lon, Unit units);
    [[nodiscard]] fn getWeatherInfo() const -> utils::types::Result<Report> override;

   private:
    utils::types::f64 m_lat;
    utils::types::f64 m_lon;
    Unit              m_units;
  };
} // namespace draconis::services::weather

#endif // DRAC_ENABLE_WEATHER
