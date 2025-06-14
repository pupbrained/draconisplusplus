#include <Drac++/Config/Config.hpp>
#include <Drac++/Services/Weather.hpp>
#include <DracUtils/Error.hpp>
#include <DracUtils/Types.hpp>

using util::error::DracError;
using util::types::Err;
using enum util::error::DracErrorCode;

namespace weather {
  fn GetWeatherInfo(const Config& config) -> util::types::Result<weather::WeatherReport> {
    if (!config.weather.enabled)
      return Err(DracError(ApiUnavailable, "Weather API disabled"));

    if (!config.weather.service)
      return Err(DracError(ApiUnavailable, "Weather service not configured"));

    return config.weather.service->getWeatherInfo();
  }
} // namespace weather