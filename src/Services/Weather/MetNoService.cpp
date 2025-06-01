#ifdef DRAC_ENABLE_WEATHER

  #ifdef __HAIKU__
    #define _DEFAULT_SOURCE // exposes timegm
  #endif

  #include "MetNoService.hpp"

  #include <chrono> // std::chrono::{system_clock, minutes, seconds}
  #include <format> // std::format
                    // glz::read is included via DataTransferObjects.hpp

  #include "Services/Weather/DataTransferObjects.hpp"
  #include "Services/Weather/WeatherUtils.hpp"

  #include "Util/Caching.hpp"
  #include "Util/Error.hpp"
  #include "Util/Types.hpp"

  #include "Wrappers/Curl.hpp"

using weather::MetNoService;
using weather::WeatherReport;
// DTOs and their Glaze meta definitions are now in Services/Weather/DataTransferObjects.hpp

MetNoService::MetNoService(const f64 lat, const f64 lon, String units)
  : m_lat(lat), m_lon(lon), m_units(std::move(units)) {}

fn MetNoService::getWeatherInfo() const -> Result<WeatherReport> {
  using glz::error_ctx, glz::read, glz::error_code;
  using util::cache::ReadCache, util::cache::WriteCache;
  using util::error::DracError, util::error::DracErrorCode;
  using util::types::None, util::types::Err, util::types::StringView;

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

  if (m_units == "imperial")
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
    .timestamp   = *timestamp,
  };

  if (Result writeResult = WriteCache("weather", out); !writeResult)
    return Err(writeResult.error());

  return out;
}

#endif // DRAC_ENABLE_WEATHER
