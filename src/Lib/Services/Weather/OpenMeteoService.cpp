#if DRAC_ENABLE_WEATHER

  #include "OpenMeteoService.hpp"

  #include "DracUtils/Error.hpp"
  #include "DracUtils/Types.hpp"

  #include "DataTransferObjects.hpp"
  #include "Utils/Caching.hpp"
  #include "WeatherUtils.hpp"
  #include "Wrappers/Curl.hpp"

using namespace util::types;
using util::error::DracError;
using enum util::error::DracErrorCode;
using weather::OpenMeteoService;
using weather::Unit;
using weather::WeatherReport;

OpenMeteoService::OpenMeteoService(const f64 lat, const f64 lon, Unit units)
  : m_lat(lat), m_lon(lon), m_units(units) {}

fn OpenMeteoService::getWeatherInfo() const -> Result<WeatherReport> {
  using glz::error_ctx, glz::read, glz::error_code;
  using util::cache::GetValidCache, util::cache::WriteCache;

  if (Result<WeatherReport> cachedDataResult = GetValidCache<WeatherReport>("weather"))
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

  Result<usize> timestamp = weather::utils::ParseIso8601ToEpoch(apiResp.currentWeather.time);

  if (!timestamp)
    return Err(timestamp.error());

  WeatherReport out = {
    .temperature = apiResp.currentWeather.temperature,
    .name        = None,
    .description = weather::utils::GetOpenmeteoWeatherDescription(apiResp.currentWeather.weathercode),
  };

  if (Result writeResult = WriteCache("weather", out); !writeResult)
    return Err(writeResult.error());

  return out;
}

#endif // DRAC_ENABLE_WEATHER
