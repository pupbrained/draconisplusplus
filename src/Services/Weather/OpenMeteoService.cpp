#define NOMINMAX

#include "OpenMeteoService.hpp"

#ifdef __HAIKU__
  #define _DEFAULT_SOURCE // exposes timegm
#endif

#include <chrono> // std::chrono::{system_clock, minutes, seconds}
#include <format> // std::format
// glz::read is included via DataTransferObjects.hpp

#include "Services/Weather.hpp"
#include "Services/Weather/DataTransferObjects.hpp"
#include "Services/Weather/WeatherUtils.hpp"

#include "Util/Caching.hpp"
#include "Util/Error.hpp"
#include "Util/Types.hpp"

#include "Wrappers/Curl.hpp"

using weather::OpenMeteoService;
using weather::WeatherReport;
// DTOs and their Glaze meta definitions are now in Services/Weather/DataTransferObjects.hpp

OpenMeteoService::OpenMeteoService(const f64 lat, const f64 lon, String units)
  : m_lat(lat), m_lon(lon), m_units(std::move(units)) {}

fn OpenMeteoService::getWeatherInfo() const -> Result<WeatherReport> {
  using glz::error_ctx, glz::read, glz::error_code;
  using util::cache::ReadCache, util::cache::WriteCache;
  using util::error::DracError, util::error::DracErrorCode;
  using util::types::Array, util::types::None, util::types::StringView, util::types::Err, util::types::Result;

  if (Result<WeatherReport> data = ReadCache<WeatherReport>("weather")) {
    using std::chrono::system_clock, std::chrono::minutes, std::chrono::seconds, std::chrono::duration;

    const WeatherReport& dataVal = *data;

    if (const duration<double> cacheAge = system_clock::now() - system_clock::time_point(seconds(dataVal.timestamp)); cacheAge < minutes(60))
      return dataVal;
  } else {
    if (const DracError& err = data.error(); err.code == DracErrorCode::NotFound)
      debug_at(err);
    else
      error_at(err);
  }

  String url = std::format(
    "https://api.open-meteo.com/v1/forecast?latitude={:.4f}&longitude={:.4f}&current_weather=true&temperature_unit={}",
    m_lat,
    m_lon,
    m_units == "imperial" ? "fahrenheit" : "celsius"
  );

  String responseBuffer;

  // clang-format off
  Curl::Easy curl({
    .url             = url,
    .writeBuffer     = &responseBuffer,
    .timeoutSecs        = 10L,
    .connectTimeoutSecs = 5L
  });
  // clang-format on

  if (!curl) {
    if (Option<DracError> initError = curl.getInitializationError())
      return Err(*initError);

    return Err(DracError(DracErrorCode::ApiUnavailable, "Failed to initialize cURL (Easy handle is invalid after construction)"));
  }

  if (Result res = curl.perform(); !res)
    return Err(res.error());

  weather::dto::openmeteo::Response apiResp {};

  if (error_ctx errc = read<glz::opts { .error_on_unknown_keys = false }>(apiResp, responseBuffer); errc.ec != error_code::none)
    return Err(DracError(DracErrorCode::ParseError, std::format("Failed to parse JSON response: {}", format_error(errc, responseBuffer))));

  Result<usize> timestamp = weather::utils::ParseIso8601ToEpoch(apiResp.currentWeather.time);

  if (!timestamp)
    return Err(timestamp.error());

  WeatherReport out = {
    .temperature = apiResp.currentWeather.temperature,
    .name        = None,
    .description = String(weather::utils::GetOpenmeteoWeatherDescription(apiResp.currentWeather.weathercode)),
    .timestamp   = *timestamp,
  };

  if (Result writeResult = WriteCache("weather", out); !writeResult)
    return Err(writeResult.error());

  return out;
}
