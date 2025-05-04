#include "weather.hpp"

#include <chrono>                 // std::chrono::{duration, operator-}
#include <curl/curl.h>            // curl_easy_setopt
#include <curl/easy.h>            // curl_easy_init, curl_easy_perform, curl_easy_cleanup
#include <expected>               // std::{expected (Result), unexpected (Err)}
#include <filesystem>             // std::filesystem::{path, remove, rename}
#include <format>                 // std::format
#include <fstream>                // std::{ifstream, ofstream}
#include <glaze/beve/read.hpp>    // glz::read_beve
#include <glaze/beve/write.hpp>   // glz::write_beve
#include <glaze/core/context.hpp> // glz::{error_ctx, error_code}
#include <glaze/core/opts.hpp>    // glz::opts
#include <glaze/core/reflect.hpp> // glz::format_error
#include <glaze/json/read.hpp> // NOLINT(misc-include-cleaner) - glaze/json/read.hpp is needed for glz::read<glz::opts>
#include <ios>                 // std::ios::{binary, trunc}
#include <iterator>            // std::istreambuf_iterator
#include <variant>             // std::{get, holds_alternative}

#include "src/util/cache.hpp"
#include "src/util/defs.hpp"
#include "src/util/error.hpp"
#include "src/util/logging.hpp"
#include "src/util/types.hpp"

#include "config.hpp"

namespace fs = std::filesystem;

using weather::Output;

namespace {
  using glz::opts, glz::error_ctx, glz::error_code, glz::read, glz::read_beve, glz::write_beve, glz::format_error;
  using util::error::DracError, util::error::DracErrorCode;
  using util::types::usize, util::types::Err, util::types::Exception;
  using weather::Coords;
  using namespace util::cache;

  constexpr opts glaze_opts = { .error_on_unknown_keys = false };

  fn WriteCallback(void* contents, const usize size, const usize nmemb, String* str) -> usize {
    const usize totalSize = size * nmemb;
    str->append(static_cast<char*>(contents), totalSize);
    return totalSize;
  }

  fn MakeApiRequest(const String& url) -> Result<Output, String> {
    debug_log("Making API request to URL: {}", url);
    CURL*  curl = curl_easy_init();
    String responseBuffer;

    if (!curl)
      return Err("Failed to initialize cURL");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBuffer);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5);

    const CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
      return Err(std::format("cURL error: {}", curl_easy_strerror(res)));

    Output output;

    if (const error_ctx errc = read<glaze_opts>(output, responseBuffer); errc.ec != error_code::none)
      return Err("API response parse error: " + format_error(errc, responseBuffer));

    return output;
  }
} // namespace

fn Weather::getWeatherInfo() const -> Result<Output, DracError> {
  using namespace std::chrono;
  using util::types::i32;

  if (Result<Output, DracError> data = ReadCache<Output>("weather")) {
    const Output& dataVal = *data;

    if (const duration<double> cacheAge = system_clock::now() - system_clock::time_point(seconds(dataVal.dt));
        cacheAge < 60min) { // NOLINT(misc-include-cleaner) - inherited from <chrono>
      debug_log("Using valid cache");
      return dataVal;
    }

    debug_log("Cache expired");
  } else {
    error_at(data.error());
  }

  fn handleApiResult = [](const Result<Output, String>& result) -> Result<Output, DracError> {
    if (!result)
      return Err(DracError(DracErrorCode::ApiUnavailable, result.error()));

    if (Result<void, DracError> writeResult = WriteCache("weather", *result); !writeResult)
      error_at(writeResult.error());

    return *result;
  };

  if (std::holds_alternative<String>(location)) {
    const auto& city = std::get<String>(location);

    char* escaped = curl_easy_escape(nullptr, city.c_str(), static_cast<i32>(city.length()));

    debug_log("Requesting city: {}", escaped);

    const String apiUrl =
      std::format("https://api.openweathermap.org/data/2.5/weather?q={}&appid={}&units={}", escaped, apiKey, units);

    curl_free(escaped);

    return handleApiResult(MakeApiRequest(apiUrl));
  }

  if (std::holds_alternative<Coords>(location)) {
    const auto& [lat, lon] = std::get<Coords>(location);
    debug_log("Requesting coordinates: lat={:.3f}, lon={:.3f}", lat, lon);

    const String apiUrl = std::format(
      "https://api.openweathermap.org/data/2.5/weather?lat={:.3f}&lon={:.3f}&appid={}&units={}", lat, lon, apiKey, units
    );

    return handleApiResult(MakeApiRequest(apiUrl));
  }

  return Err(DracError(DracErrorCode::ParseError, "Invalid location type in configuration."));
}
