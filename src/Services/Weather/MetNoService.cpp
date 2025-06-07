#if DRAC_ENABLE_WEATHER

// clang-format off
#include "MetNoService.hpp"

#include <format> // std::format

#include "Services/Weather/DataTransferObjects.hpp"
#include "Services/Weather/WeatherUtils.hpp"

#include "Util/Caching.hpp"
#include "Util/Error.hpp"
#include "Util/Types.hpp"

#include "Wrappers/Curl.hpp"
// clang-format on

using weather::MetNoService;
using weather::WeatherReport;

MetNoService::MetNoService(const f64 lat, const f64 lon, config::WeatherUnit units)
  : m_lat(lat), m_lon(lon), m_units(units) {}

fn MetNoService::getWeatherInfo() const -> Result<WeatherReport> {
  using glz::error_ctx, glz::read, glz::error_code;
  using util::cache::GetValidCache, util::cache::WriteCache;
  using util::error::DracError, util::error::DracErrorCode;
  using util::types::None, util::types::Err, util::types::StringView;

  if (Result<WeatherReport> cachedDataResult = GetValidCache<WeatherReport>("weather"))
    return *cachedDataResult;
  else
    debug_at(cachedDataResult.error());

  String responseBuffer;

  // clang-format off
  Curl::Easy curl({
    .url             = std::format("https://api.met.no/weatherapi/locationforecast/2.0/compact?lat={:.4f}&lon={:.4f}", m_lat, m_lon),
    .writeBuffer     = &responseBuffer,
    .timeoutSecs        = 10L,
    .connectTimeoutSecs = 5L,
    .userAgent       = "draconisplusplus/" DRACONISPLUSPLUS_VERSION " git.pupbrained.xyz/draconisplusplus"
  });
  // clang-format on

  if (!curl) {
    if (Option<DracError> initError = curl.getInitializationError())
      return Err(*initError);

    return Err(DracError(DracErrorCode::ApiUnavailable, "Failed to initialize cURL (Easy handle is invalid after construction)"));
  }

  if (Result res = curl.perform(); !res)
    return Err(res.error());

  weather::dto::metno::Response apiResp {};

  if (error_ctx errc = read<glz::opts { .error_on_unknown_keys = false }>(apiResp, responseBuffer); errc.ec != error_code::none)
    return Err(DracError(DracErrorCode::ParseError, std::format("Failed to parse JSON response: {}", format_error(errc, responseBuffer))));

  if (apiResp.properties.timeseries.empty())
    return Err(DracError(DracErrorCode::ParseError, "No timeseries data in met.no response"));

  const auto& [time, data] = apiResp.properties.timeseries.front();

  f64 temp = data.instant.details.airTemperature;

  if (m_units == config::WeatherUnit::IMPERIAL)
    temp = temp * 9.0 / 5.0 + 32.0;

  String symbolCode = data.next1Hours ? data.next1Hours->summary.symbolCode : "";

  if (!symbolCode.empty()) {
    const StringView strippedSymbol = weather::utils::StripTimeOfDayFromSymbol(symbolCode);
    if (auto iter = weather::utils::GetMetnoSymbolDescriptions().find(strippedSymbol); iter != weather::utils::GetMetnoSymbolDescriptions().end())
      symbolCode = iter->second;
  }

  Result<usize> timestamp = weather::utils::ParseIso8601ToEpoch(time);

  if (!timestamp)
    return Err(timestamp.error());

  WeatherReport out = {
    .temperature = temp,
    .name        = None,
    .description = std::move(symbolCode),
  };

  if (Result writeResult = WriteCache("weather", out); !writeResult)
    return Err(writeResult.error());

  return out;
}

#endif // DRAC_ENABLE_WEATHER
