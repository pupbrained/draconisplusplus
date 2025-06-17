#if DRAC_ENABLE_WEATHER

  #include "OpenMeteoService.hpp"

  #include "DracUtils/Error.hpp"
  #include "DracUtils/Types.hpp"

  #include "DataTransferObjects.hpp"
  #include "Utils/Caching.hpp"
  #include "WeatherUtils.hpp"
  #include "Wrappers/Curl.hpp"

using namespace drac::types;
using drac::error::DracError;
using enum drac::error::DracErrorCode;
using weather::OpenMeteoService;
using weather::Report;
using weather::Unit;

OpenMeteoService::OpenMeteoService(const f64 lat, const f64 lon, const Unit units)
  : m_lat(lat), m_lon(lon), m_units(units) {}

fn OpenMeteoService::getWeatherInfo() const -> Result<Report> {
  using drac::cache::GetValidCache, drac::cache::WriteCache;
  using glz::error_ctx, glz::read, glz::error_code;

  if (Result<Report> cachedDataResult = GetValidCache<Report>("weather"))
    return *cachedDataResult;
  else
    debug_at(cachedDataResult.error());

  String url = std::format(
    "https://api.open-meteo.com/v1/forecast?latitude={:.4f}&longitude={:.4f}&current_weather=true&temperature_unit={}",
    m_lat,
    m_lon,
    m_units == Unit::IMPERIAL ? "fahrenheit" : "celsius"
  );

  String responseBuffer;

  Curl::Easy curl({
    .url                = url,
    .writeBuffer        = &responseBuffer,
    .timeoutSecs        = 10L,
    .connectTimeoutSecs = 5L,
  });

  if (!curl) {
    if (Option<DracError> initError = curl.getInitializationError())
      return Err(*initError);

    return Err(DracError(ApiUnavailable, "Failed to initialize cURL (Easy handle is invalid after construction)"));
  }

  if (Result res = curl.perform(); !res)
    return Err(res.error());

  weather::dto::openmeteo::Response apiResp {};

  if (error_ctx errc = read<glz::opts { .error_on_unknown_keys = false }>(apiResp, responseBuffer.data()); errc.ec != error_code::none)
    return Err(DracError(ParseError, std::format("Failed to parse JSON response: {}", format_error(errc, responseBuffer.data()))));

  if (Result<usize> timestamp = weather::utils::ParseIso8601ToEpoch(apiResp.currentWeather.time); !timestamp)
    return Err(timestamp.error());

  Report out = {
    .temperature = apiResp.currentWeather.temperature,
    .name        = None,
    .description = weather::utils::GetOpenmeteoWeatherDescription(apiResp.currentWeather.weathercode),
  };

  if (Result writeResult = WriteCache("weather", out); !writeResult)
    return Err(writeResult.error());

  return out;
}

#endif // DRAC_ENABLE_WEATHER
