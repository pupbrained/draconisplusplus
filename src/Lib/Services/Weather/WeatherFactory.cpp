#include <glaze/core/common.hpp>
#include <glaze/core/meta.hpp>
#include <glaze/json.hpp>

#include "Drac++/Services/Weather.hpp"

#include "Drac++/Utils/CacheManager.hpp"
#include "Drac++/Utils/Error.hpp"
#include "Drac++/Utils/Types.hpp"

#include "Services/Weather/MetNoService.hpp"
#include "Services/Weather/OpenMeteoService.hpp"
#include "Services/Weather/OpenWeatherMapService.hpp"
#include "Wrappers/Curl.hpp"

using namespace draconis::utils::types;
using draconis::utils::error::DracError;
using enum draconis::utils::error::DracErrorCode;

// IP API response structure
struct IPApiResponse {
  f64    lat;
  f64    lon;
  String city;
  String regionName;
  String country;
  String status;
  String message;
};

template <>
struct glz::meta<IPApiResponse> {
  using T = IPApiResponse;

  // clang-format off
  static constexpr glz::detail::Object value = glz::object(
    "lat",        &T::lat,
    "lon",        &T::lon,
    "city",       &T::city,
    "regionName", &T::regionName,
    "country",    &T::country,
    "status",     &T::status,
    "message",    &T::message
  );
  // clang-format on
};

// Nominatim API response structure
struct NominatimResult {
  String lat;
  String lon;
  String displayName;
};

template <>
struct glz::meta<NominatimResult> {
  using T = NominatimResult;

  // clang-format off
  static constexpr glz::detail::Object value = glz::object(
    "lat",         &T::lat,
    "lon",         &T::lon,
    "displayName", &T::displayName
  );
  // clang-format on
};

namespace draconis::services::weather {
  fn CreateWeatherService(const Provider provider, const Location& location, UnitSystem units, const Option<String>& apiKey) -> UniquePointer<IWeatherService> {
    using enum Provider;

    if (GetCacheManager() == nullptr) {
      GetCacheManager() = std::make_unique<draconis::utils::cache::CacheManager>();
      GetCacheManager()->setGlobalPolicy({ .location = draconis::utils::cache::CacheLocation::Persistent, .ttl = std::chrono::minutes(15) });
    }

    assert(provider == OpenWeatherMap || provider == OpenMeteo || provider == MetNo);
    assert(apiKey.has_value() || provider != OpenWeatherMap);

    switch (provider) {
      case OpenWeatherMap:
        return std::make_unique<OpenWeatherMapService>(location, *apiKey, units);
      case OpenMeteo:
        return std::make_unique<OpenMeteoService>(std::get<Coords>(location).lat, std::get<Coords>(location).lon, units);
      case MetNo:
        return std::make_unique<MetNoService>(std::get<Coords>(location).lat, std::get<Coords>(location).lon, units);
      default:
        std::unreachable();
    }
  }

  fn Geocode(const String& placeName) -> Result<Coords> {
    Result<String> escapedPlaceName = Curl::Easy::escape(placeName);
    if (!escapedPlaceName)
      ERR_FROM(escapedPlaceName.error());

    String url = std::format("https://nominatim.openstreetmap.org/search?q={}&format=json&limit=1", *escapedPlaceName);

    String responseBuffer;

    Curl::Easy curl({
      .url                = url,
      .writeBuffer        = &responseBuffer,
      .timeoutSecs        = 10L,
      .connectTimeoutSecs = 5L,
      .userAgent          = String("draconisplusplus/" DRAC_VERSION " git.pupbrained.xyz/draconisplusplus"),
    });

    if (!curl) {
      if (const Option<DracError>& initError = curl.getInitializationError())
        ERR_FROM(*initError);

      ERR(ApiUnavailable, "Failed to initialize cURL for Nominatim request");
    }

    if (Result res = curl.perform(); !res)
      ERR_FROM(res.error());

    Vec<NominatimResult> results;

    if (glz::error_ctx errc = glz::read<glz::opts { .error_on_unknown_keys = false }>(results, responseBuffer.data()); errc != glz::error_code::none)
      ERR_FMT(ParseError, "Failed to parse Nominatim JSON response: {}", glz::format_error(errc, responseBuffer.data()));

    if (results.empty())
      ERR_FMT(NotFound, "No results found for place: {}", placeName);

    const auto& result = results.front();

    f64 lat = 0.0, lon = 0.0;

    try {
      lat = std::stod(result.lat);
      lon = std::stod(result.lon);
    } catch (const std::exception& e) {
      ERR_FMT(ParseError, "Failed to parse coordinates from Nominatim response: {}", e.what());
    }

    return Coords { .lat = lat, .lon = lon };
  }

  fn GetCurrentLocationInfoFromIP() -> Result<IPLocationInfo> {
    String responseBuffer;

    Curl::Easy curl({
      .url                = "http://ip-api.com/json/",
      .writeBuffer        = &responseBuffer,
      .timeoutSecs        = 10L,
      .connectTimeoutSecs = 5L,
    });

    if (!curl) {
      if (const Option<DracError>& initError = curl.getInitializationError())
        ERR_FROM(*initError);

      ERR(ApiUnavailable, "Failed to initialize cURL for IP geolocation");
    }

    if (Result res = curl.perform(); !res)
      ERR_FROM(res.error());

    try {
      IPApiResponse        response;
      const glz::error_ctx result = glz::read<glz::opts { .error_on_unknown_keys = false }>(response, responseBuffer);

      if (result)
        ERR_FMT(ParseError, "Failed to parse IP geolocation response: {}", glz::format_error(result, responseBuffer));

      if (response.status != "success") {
        String errorMsg = response.message.empty() ? "Unknown error" : response.message;
        ERR_FMT(ApiUnavailable, "IP geolocation service error: {}", errorMsg);
      }

      if (response.city.empty() && response.regionName.empty() && response.country.empty())
        ERR(ParseError, "IP geolocation response missing location information");

      String locationName;
      if (!response.city.empty() && !response.regionName.empty() && !response.country.empty())
        locationName = response.city + ", " + response.regionName + ", " + response.country;
      else if (!response.city.empty() && !response.country.empty())
        locationName = response.city + ", " + response.country;
      else if (!response.regionName.empty() && !response.country.empty())
        locationName = response.regionName + ", " + response.country;
      else if (!response.country.empty())
        locationName = response.country;
      else
        locationName = "Unknown Location";

      return IPLocationInfo {
        .coords       = { .lat = response.lat, .lon = response.lon },
        .city         = response.city.empty() ? "Unknown" : response.city,
        .region       = response.regionName.empty() ? "Unknown" : response.regionName,
        .country      = response.country.empty() ? "Unknown" : response.country,
        .locationName = locationName
      };
    } catch (const std::exception& e) {
      ERR_FMT(ParseError, "Failed to parse IP geolocation response: {}", e.what());
    }
  }
} // namespace draconis::services::weather
