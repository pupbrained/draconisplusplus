#if DRAC_ENABLE_WEATHER

  #include "MetNoService.hpp"

  #include "DracUtils/Error.hpp"
  #include "DracUtils/Types.hpp"

  #include "DataTransferObjects.hpp"
  #include "Utils/Caching.hpp"
  #include "WeatherUtils.hpp"
  #include "Wrappers/Curl.hpp"

using namespace drac::types;
using drac::error::DracError;
using enum drac::error::DracErrorCode;
using weather::MetNoService;
using weather::Report;
using weather::Unit;

MetNoService::MetNoService(const f64 lat, const f64 lon, const Unit units)
  : m_lat(lat), m_lon(lon), m_units(units) {}

fn MetNoService::getWeatherInfo() const -> Result<Report> {
  using drac::cache::GetValidCache, drac::cache::WriteCache;
  using glz::error_ctx, glz::read, glz::error_code;

  if (Result<Report> cachedDataResult = GetValidCache<Report>("weather"))
    return *cachedDataResult;
  else
    debug_at(cachedDataResult.error());

  String responseBuffer;

  Curl::Easy curl({
    .url                = std::format("https://api.met.no/weatherapi/locationforecast/2.0/compact?lat={:.4f}&lon={:.4f}", m_lat, m_lon),
    .writeBuffer        = &responseBuffer,
    .timeoutSecs        = 10L,
    .connectTimeoutSecs = 5L,
    .userAgent          = String("draconisplusplus/" DRAC_VERSION " git.pupbrained.xyz/draconisplusplus"),
  });

  if (!curl) {
    if (Option<DracError> initError = curl.getInitializationError())
      return Err(*initError);

    return Err(DracError(ApiUnavailable, "Failed to initialize cURL (Easy handle is invalid after construction)"));
  }

  if (Result res = curl.perform(); !res)
    return Err(res.error());

  weather::dto::metno::Response apiResp {};

  if (error_ctx errc = read<glz::opts { .error_on_unknown_keys = false }>(apiResp, responseBuffer); errc.ec != error_code::none)
    return Err(DracError(ParseError, std::format("Failed to parse JSON response: {}", format_error(errc, responseBuffer.data()))));

  if (apiResp.properties.timeseries.empty())
    return Err(DracError(ParseError, "No timeseries data in met.no response"));

  const auto& [time, data] = apiResp.properties.timeseries.front();

  f64 temp = data.instant.details.airTemperature;

  if (m_units == Unit::IMPERIAL)
    temp = temp * 9.0 / 5.0 + 32.0;

  String symbolCode = data.next1Hours ? data.next1Hours->summary.symbolCode : "";

  if (!symbolCode.empty()) {
    const String strippedSymbol = weather::utils::StripTimeOfDayFromSymbol(symbolCode);

    if (auto iter = weather::utils::GetMetnoSymbolDescriptions().find(strippedSymbol); iter != weather::utils::GetMetnoSymbolDescriptions().end())
      symbolCode = iter->second;
  }

  if (Result<usize> timestamp = weather::utils::ParseIso8601ToEpoch(time); !timestamp)
    return Err(timestamp.error());

  Report out = {
    .temperature = temp,
    .name        = None,
    .description = symbolCode,
  };

  if (Result writeResult = WriteCache("weather", out); !writeResult)
    return Err(writeResult.error());

  return out;
}

#endif // DRAC_ENABLE_WEATHER
