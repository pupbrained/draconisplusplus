#include <chrono>
#include <curl/curl.h>
#include <expected>
#include <filesystem>
#include <fmt/core.h>
#include <fstream>

#include "weather.h"

#include "config.h"
#include "src/util/macros.h"

namespace fs = std::filesystem;
using namespace std::string_literals;

namespace {
  constexpr glz::opts glaze_opts = { .error_on_unknown_keys = false };

  fn GetCachePath() -> std::expected<fs::path, string> {
    std::error_code errc;
    fs::path        cachePath = fs::temp_directory_path(errc);

    if (errc)
      return std::unexpected("Failed to get temp directory: " + errc.message());

    cachePath /= "weather_cache.json";
    return cachePath;
  }

  fn ReadCacheFromFile() -> std::expected<WeatherOutput, string> {
    std::expected<fs::path, string> cachePath = GetCachePath();

    if (!cachePath)
      return std::unexpected(cachePath.error());

    std::ifstream ifs(*cachePath, std::ios::binary);

    if (!ifs.is_open())
      return std::unexpected("Cache file not found: " + cachePath->string());

    DEBUG_LOG("Reading from cache file...");

    try {
      const string  content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
      WeatherOutput result;

      if (const glz::error_ctx errc = glz::read<glaze_opts>(result, content); errc.ec != glz::error_code::none)
        return std::unexpected("JSON parse error: " + glz::format_error(errc, content));

      DEBUG_LOG("Successfully read from cache file.");
      return result;
    } catch (const std::exception& e) { return std::unexpected("Error reading cache: "s + e.what()); }
  }

  fn WriteCacheToFile(const WeatherOutput& data) -> std::expected<void, string> {
    std::expected<fs::path, string> cachePath = GetCachePath();

    if (!cachePath)
      return std::unexpected(cachePath.error());

    DEBUG_LOG("Writing to cache file...");
    fs::path tempPath = *cachePath;
    tempPath += ".tmp";

    try {
      {
        std::ofstream ofs(tempPath, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open())
          return std::unexpected("Failed to open temp file: " + tempPath.string());

        string jsonStr;

        if (const glz::error_ctx errc = glz::write_json(data, jsonStr); errc.ec != glz::error_code::none)
          return std::unexpected("JSON serialization error: " + glz::format_error(errc, jsonStr));

        ofs << jsonStr;
        if (!ofs)
          return std::unexpected("Failed to write to temp file");
      }

      std::error_code errc;
      fs::rename(tempPath, *cachePath, errc);
      if (errc) {
        fs::remove(tempPath, errc);
        return std::unexpected("Failed to replace cache file: " + errc.message());
      }

      DEBUG_LOG("Successfully wrote to cache file.");
      return {};
    } catch (const std::exception& e) { return std::unexpected("File operation error: "s + e.what()); }
  }

  fn WriteCallback(void* contents, const size_t size, const size_t nmemb, string* str) -> size_t {
    const size_t totalSize = size * nmemb;
    str->append(static_cast<char*>(contents), totalSize);
    return totalSize;
  }

  fn MakeApiRequest(const string& url) -> std::expected<WeatherOutput, string> {
    DEBUG_LOG("Making API request to URL: {}", url);
    CURL*  curl = curl_easy_init();
    string responseBuffer;

    if (!curl)
      return std::unexpected("Failed to initialize cURL");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBuffer);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5);

    const CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
      return std::unexpected(fmt::format("cURL error: {}", curl_easy_strerror(res)));

    WeatherOutput output;

    if (const glz::error_ctx errc = glz::read<glaze_opts>(output, responseBuffer); errc.ec != glz::error_code::none)
      return std::unexpected("API response parse error: " + glz::format_error(errc, responseBuffer));

    return std::move(output);
  }
}

fn Weather::getWeatherInfo() const -> WeatherOutput {
  using namespace std::chrono;

  if (std::expected<WeatherOutput, string> data = ReadCacheFromFile()) {
    const WeatherOutput& dataVal = *data;

    if (const duration<double> cacheAge = system_clock::now() - system_clock::time_point(seconds(dataVal.dt));
        cacheAge < 10min) {
      DEBUG_LOG("Using valid cache");
      return dataVal;
    }

    DEBUG_LOG("Cache expired");
  } else {
    DEBUG_LOG("Cache error: {}", data.error());
  }

  fn handleApiResult = [](const std::expected<WeatherOutput, string>& result) -> WeatherOutput {
    if (!result) {
      ERROR_LOG("API request failed: {}", result.error());
      return WeatherOutput {};
    }

    if (std::expected<void, string> writeResult = WriteCacheToFile(*result); !writeResult)
      ERROR_LOG("Failed to write cache: {}", writeResult.error());

    return *result;
  };

  if (std::holds_alternative<string>(location)) {
    const auto& city    = std::get<string>(location);
    char*       escaped = curl_easy_escape(nullptr, city.c_str(), static_cast<int>(city.length()));
    DEBUG_LOG("Requesting city: {}", escaped);

    const string apiUrl =
      fmt::format("https://api.openweathermap.org/data/2.5/weather?q={}&appid={}&units={}", escaped, api_key, units);

    curl_free(escaped);
    return handleApiResult(MakeApiRequest(apiUrl));
  }

  const auto& [lat, lon] = std::get<Coords>(location);
  DEBUG_LOG("Requesting coordinates: lat={:.3f}, lon={:.3f}", lat, lon);

  const string apiUrl = fmt::format(
    "https://api.openweathermap.org/data/2.5/weather?lat={:.3f}&lon={:.3f}&appid={}&units={}", lat, lon, api_key, units
  );

  return handleApiResult(MakeApiRequest(apiUrl));
}
