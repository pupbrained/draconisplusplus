#include <utility>

#include "Drac++/Services/Weather.hpp"

#include "Drac++/Utils/Types.hpp"

#include "Services/Weather/MetNoService.hpp"
#include "Services/Weather/OpenMeteoService.hpp"
#include "Services/Weather/OpenWeatherMapService.hpp"

using namespace draconis::utils::types;

namespace draconis::services::weather {
  fn CreateWeatherService(const Provider provider, const Location& location, Unit units, const Option<String>& apiKey) -> UniquePointer<IWeatherService> {
    assert(provider == Provider::OPENWEATHERMAP || provider == Provider::OPENMETEO || provider == Provider::METNO);
    assert(apiKey.has_value() || provider != Provider::OPENWEATHERMAP);

    switch (provider) {
      case Provider::OPENWEATHERMAP:
        return std::make_unique<OpenWeatherMapService>(std::get<String>(location), *apiKey, units);
      case Provider::OPENMETEO:
        return std::make_unique<OpenMeteoService>(std::get<Coords>(location).lat, std::get<Coords>(location).lon, units);
      case Provider::METNO:
        return std::make_unique<MetNoService>(std::get<Coords>(location).lat, std::get<Coords>(location).lon, units);
      default:
        std::unreachable();
    }
  }
} // namespace draconis::services::weather