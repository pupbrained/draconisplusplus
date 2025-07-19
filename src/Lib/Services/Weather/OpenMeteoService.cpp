#if DRAC_ENABLE_WEATHER

  #include "OpenMeteoService.hpp"

  #include "Drac++/Utils/CacheManager.hpp"
  #include "Drac++/Utils/Error.hpp"
  #include "Drac++/Utils/Types.hpp"

  #include "DataTransferObjects.hpp"
  #include "WeatherUtils.hpp"
  #include "Wrappers/Curl.hpp"

using namespace draconis::utils::types;
using draconis::utils::error::DracError;
using enum draconis::utils::error::DracErrorCode;
using draconis::services::weather::OpenMeteoService;
using draconis::services::weather::Report;
using draconis::services::weather::UnitSystem;

OpenMeteoService::OpenMeteoService(const f64 lat, const f64 lon, const UnitSystem units)
  : m_lat(lat), m_lon(lon), m_units(units) {}

fn OpenMeteoService::getWeatherInfo() const -> Result<Report> {
  using glz::error_ctx, glz::read, glz::error_code;

  return GetCacheManager()->getOrSet<Report>(
    "openmeteo_weather",
    [&]() -> Result<Report> {
      String url = std::format(
        "https://api.open-meteo.com/v1/forecast?latitude={:.4f}&longitude={:.4f}&current_weather=true&temperature_unit={}",
        m_lat,
        m_lon,
        m_units == UnitSystem::Imperial ? "fahrenheit" : "celsius"
      );

      String responseBuffer;

      Curl::Easy curl({
        .url                = url,
        .writeBuffer        = &responseBuffer,
        .timeoutSecs        = 10L,
        .connectTimeoutSecs = 5L,
      });

      if (!curl) {
        if (const Option<DracError>& initError = curl.getInitializationError())
          ERR_FROM(*initError);

        ERR(ApiUnavailable, "Failed to initialize cURL (Easy handle is invalid after construction)");
      }

      if (Result res = curl.perform(); !res)
        ERR_FROM(res.error());

      draconis::services::weather::dto::openmeteo::Response apiResp {};

      if (error_ctx errc = read<glz::opts { .error_on_unknown_keys = false }>(apiResp, responseBuffer.data()); errc.ec != error_code::none)
        ERR_FMT(ParseError, "Failed to parse JSON response: {}", format_error(errc, responseBuffer.data()));

      if (Result<usize> timestamp = draconis::services::weather::utils::ParseIso8601ToEpoch(apiResp.currentWeather.time); !timestamp)
        ERR_FROM(timestamp.error());

      Report out = {
        .temperature = apiResp.currentWeather.temperature,
        .name        = None,
        .description = draconis::services::weather::utils::GetOpenmeteoWeatherDescription(apiResp.currentWeather.weathercode),
      };
      return out;
    }
  );
}

#endif // DRAC_ENABLE_WEATHER
