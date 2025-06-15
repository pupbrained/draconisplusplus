#if DRAC_ENABLE_WEATHER

  #include "WeatherUtils.hpp"

  #include <charconv> // For std::from_chars
  #include <ctime>    // For std::tm, timegm, _mkgmtime

  #ifdef __HAIKU__
    #define _DEFAULT_SOURCE // exposes timegm
  #endif

using namespace util::types;
using util::error::DracError;
using enum util::error::DracErrorCode;

namespace weather::utils {
  fn StripTimeOfDayFromSymbol(SZStringView symbol_code) -> SZStringView {
    static constexpr Array<SZStringView, 3> SUFFIXES = { "_day", "_night", "_polartwilight" };

    for (const SZStringView& suffix : SUFFIXES)
      if (symbol_code.size() > suffix.size() && symbol_code.ends_with(suffix))
        return symbol_code.substr(0, symbol_code.size() - suffix.size());

    return symbol_code;
  }

  fn ParseIso8601ToEpoch(SZStringView iso8601_string) -> Result<time_t> {
    const usize stringLen = iso8601_string.size();

    // Supported lengths:
    // 20: "YYYY-MM-DDTHH:MM:SSZ"
    // 16: "YYYY-MM-DDTHH:MM" (seconds assumed 00, UTC assumed)
    if (stringLen != 20 && stringLen != 16)
      return Err(DracError(ParseError, util::formatting::SzFormat("Failed to parse ISO8601 time \'{}\', unexpected length {}. Expected 16 or 20 characters.", iso8601_string, stringLen)));

    std::tm timeStruct = {};
    i32     year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0; // Default second to 0

    auto parseInt = [](SZStringView s_view, i32& out_val) -> bool {
      auto [ptr, ec] = std::from_chars(s_view.data(), s_view.data() + s_view.size(), out_val);
      return ec == std::errc() && ptr == s_view.data() + s_view.size();
    };

    // Common parsing for YYYY-MM-DDTHH:MM
    // Structure: YYYY-MM-DDTHH:MM
    // Indices:   0123456789012345
    if (!parseInt(iso8601_string.substr(0, 4), year) || // YYYY
        iso8601_string[4] != '-' ||
        !parseInt(iso8601_string.substr(5, 2), month) || // MM
        iso8601_string[7] != '-' ||
        !parseInt(iso8601_string.substr(8, 2), day) || // DD
        iso8601_string[10] != 'T' ||
        !parseInt(iso8601_string.substr(11, 2), hour) || // HH
        iso8601_string[13] != ':' ||
        !parseInt(iso8601_string.substr(14, 2), minute) // MM
    )
      return Err(DracError(ParseError, util::formatting::SzFormat("Failed to parse common date/time components from ISO8601 string: \'{}\'", iso8601_string)));

    if (stringLen == 20) // Format: YYYY-MM-DDTHH:MM:SSZ
      if (iso8601_string[16] != ':' || !parseInt(iso8601_string.substr(17, 2), second) || iso8601_string[19] != 'Z')
        return Err(DracError(ParseError, util::formatting::SzFormat("Failed to parse seconds or UTC zone from 20-character ISO8601 string: \'{}\'", iso8601_string)));

    timeStruct.tm_year  = year - 1900;
    timeStruct.tm_mon   = month - 1;
    timeStruct.tm_mday  = day;
    timeStruct.tm_hour  = hour;
    timeStruct.tm_min   = minute;
    timeStruct.tm_sec   = second;
    timeStruct.tm_isdst = 0;

  #ifdef _WIN32
    time_t epochTime = _mkgmtime(&timeStruct);

    if (epochTime == -1)
      return Err(DracError(ParseError, "Failed to convert time to epoch using _mkgmtime (invalid date components or out of range)"));

    return static_cast<usize>(epochTime);
  #else
    time_t epochTime = timegm(&timeStruct);

    if (epochTime == static_cast<time_t>(-1))
      return Err(DracError(ParseError, util::formatting::SzFormat("Failed to convert time to epoch using timegm (invalid date components or out of range)")));

    return epochTime;
  #endif
  }

  fn GetMetnoSymbolDescriptions() -> const std::unordered_map<SZStringView, SZStringView>& {
    static const std::unordered_map<SZStringView, SZStringView> MAP = {
      // Clear / Fair
      {             "clearsky",               "clear sky" },
      {                 "fair",                    "fair" },
      {         "partlycloudy",           "partly cloudy" },
      {               "cloudy",                  "cloudy" },
      {                  "fog",                     "fog" },

      // Rain
      {            "lightrain",              "light rain" },
      {     "lightrainshowers",      "light rain showers" },
      {  "lightrainandthunder",  "light rain and thunder" },
      {                 "rain",                    "rain" },
      {          "rainshowers",            "rain showers" },
      {       "rainandthunder",        "rain and thunder" },
      {            "heavyrain",              "heavy rain" },
      {     "heavyrainshowers",      "heavy rain showers" },
      {  "heavyrainandthunder",  "heavy rain and thunder" },

      // Sleet
      {           "lightsleet",             "light sleet" },
      {    "lightsleetshowers",     "light sleet showers" },
      { "lightsleetandthunder", "light sleet and thunder" },
      {                "sleet",                   "sleet" },
      {         "sleetshowers",           "sleet showers" },
      {      "sleetandthunder",       "sleet and thunder" },
      {           "heavysleet",             "heavy sleet" },
      {    "heavysleetshowers",     "heavy sleet showers" },
      { "heavysleetandthunder", "heavy sleet and thunder" },

      // Snow
      {            "lightsnow",              "light snow" },
      {     "lightsnowshowers",      "light snow showers" },
      {  "lightsnowandthunder",  "light snow and thunder" },
      {                 "snow",                    "snow" },
      {          "snowshowers",            "snow showers" },
      {       "snowandthunder",        "snow and thunder" },
      {            "heavysnow",              "heavy snow" },
      {     "heavysnowshowers",      "heavy snow showers" },
      {  "heavysnowandthunder",  "heavy snow and thunder" },
    };
    return MAP;
  }

  fn GetOpenmeteoWeatherDescription(int weather_code) -> SZStringView {
    // Based on WMO Weather interpretation codes (WW)
    // https://open-meteo.com/en/docs
    if (weather_code == 0)
      return "clear sky";
    if (weather_code == 1)
      return "mainly clear";
    if (weather_code == 2)
      return "partly cloudy";
    if (weather_code == 3)
      return "overcast";
    if (weather_code == 45 || weather_code == 48)
      return "fog";
    if (weather_code >= 51 && weather_code <= 55)
      return "drizzle";
    if (weather_code == 56 || weather_code == 57)
      return "freezing drizzle";
    if (weather_code >= 61 && weather_code <= 65)
      return "rain";
    if (weather_code == 66 || weather_code == 67)
      return "freezing rain";
    if (weather_code >= 71 && weather_code <= 75)
      return "snow fall";
    if (weather_code == 77)
      return "snow grains";
    if (weather_code >= 80 && weather_code <= 82)
      return "rain showers";
    if (weather_code == 85 || weather_code == 86)
      return "snow showers";
    if (weather_code == 95)
      return "thunderstorm";
    if (weather_code >= 96 && weather_code <= 99)
      return "thunderstorm with hail";
    return "unknown";
  }

} // namespace weather::utils

#endif // DRAC_ENABLE_WEATHER
