#if DRAC_ENABLE_WEATHER

  #include "MetNoService.hpp"

  #include "Drac++/Utils/CacheManager.hpp"
  #include "Drac++/Utils/Error.hpp"
  #include "Drac++/Utils/Types.hpp"

  #include "DataTransferObjects.hpp"
  #include "WeatherUtils.hpp"
  #include "Wrappers/Curl.hpp"

using namespace draconis::utils::types;
using draconis::utils::error::DracError;
using enum draconis::utils::error::DracErrorCode;
using draconis::services::weather::MetNoService;
using draconis::services::weather::Report;
using draconis::services::weather::UnitSystem;

MetNoService::MetNoService(const f64 lat, const f64 lon, const UnitSystem units)
  : m_lat(lat), m_lon(lon), m_units(units) {}

fn MetNoService::getWeatherInfo() const -> Result<Report> {
  using glz::error_ctx, glz::read, glz::error_code;

  return GetCacheManager()->getOrSet<Report>(
    "metno_weather",
    [&]() -> Result<Report> {
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

      draconis::services::weather::dto::metno::Response apiResp {};

      if (error_ctx errc = read<glz::opts { .error_on_unknown_keys = false }>(apiResp, responseBuffer); errc.ec != error_code::none)
        return Err(DracError(ParseError, std::format("Failed to parse JSON response: {}", format_error(errc, responseBuffer.data()))));

      if (apiResp.properties.timeseries.empty())
        return Err(DracError(ParseError, "No timeseries data in met.no response"));

      const auto& [time, data] = apiResp.properties.timeseries.front();

      f64 temp = data.instant.details.airTemperature;

      if (m_units == UnitSystem::IMPERIAL)
        temp = temp * 9.0 / 5.0 + 32.0;

      String symbolCode = data.next1Hours ? data.next1Hours->summary.symbolCode : "";

      if (!symbolCode.empty()) {
        const String strippedSymbol = draconis::services::weather::utils::StripTimeOfDayFromSymbol(symbolCode);

        if (auto iter = draconis::services::weather::utils::GetMetnoSymbolDescriptions().find(strippedSymbol); iter != draconis::services::weather::utils::GetMetnoSymbolDescriptions().end())
          symbolCode = iter->second;
      }

      if (Result<usize> timestamp = draconis::services::weather::utils::ParseIso8601ToEpoch(time); !timestamp)
        return Err(timestamp.error());

      Report out = {
        .temperature = temp,
        .name        = None,
        .description = symbolCode,
      };
      return out;
    }
  );
}

#endif // DRAC_ENABLE_WEATHER
