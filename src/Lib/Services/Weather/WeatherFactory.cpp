#include "Drac++/Services/Weather.hpp"

#include "DracUtils/Types.hpp"

#include "Services/Weather/MetNoService.hpp"
#include "Services/Weather/OpenMeteoService.hpp"
#include "Services/Weather/OpenWeatherMapService.hpp"

namespace weather {
  fn CreateWeatherService(Provider provider, const Location& location, const util::types::SZString& apiKey, Unit units) -> util::types::UniquePointer<IWeatherService> {
    switch (provider) {
      case Provider::OPENWEATHERMAP:
        return std::make_unique<OpenWeatherMapService>(location, apiKey, units);
      default:
        return nullptr;
    }
  }

  fn CreateWeatherService(Provider provider, const Coords& coords, Unit units) -> util::types::UniquePointer<IWeatherService> {
    switch (provider) {
      case Provider::OPENMETEO:
        return std::make_unique<OpenMeteoService>(coords.lat, coords.lon, units);
      case Provider::METNO:
        return std::make_unique<MetNoService>(coords.lat, coords.lon, units);
      default:
        return nullptr;
    }
  }
} // namespace weather