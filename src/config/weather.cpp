#include <chrono>
#include <curl/curl.h>
#include <fmt/core.h>
#include <rfl/json.hpp>
#include <rfl/json/load.hpp>

#include "config.h"

using rfl::Error;
using rfl::Result;

// Function to read cache from file
fn ReadCacheFromFile() -> Result<WeatherOutput> {
#ifdef __WIN32__
  const char*   tempPath = getenv("TEMP");
  const string  path     = string(tempPath) + "\\weather_cache.json";
  std::ifstream ifs(path);
#else
  std::ifstream ifs("/tmp/weather_cache.json");
#endif

  if (!ifs.is_open())
    return Error("Cache file not found.");

  fmt::println("Reading from cache file...");

  std::stringstream buf;

  buf << ifs.rdbuf();

  Result<WeatherOutput> val = rfl::json::read<WeatherOutput>(buf.str());

  fmt::println("Successfully read from cache file.");

  return val;
}

// Function to write cache to file
fn WriteCacheToFile(const WeatherOutput& data) -> Result<u8> {
  fmt::println("Writing to cache file...");

#ifdef __WIN32__
  const char*   tempPath = getenv("TEMP");
  const string  path     = string(tempPath) + "\\weather_cache.json";
  std::ofstream ofs(path);
#else
  std::ofstream ofs("/tmp/weather_cache.json");
#endif

  if (!ofs.is_open())
    return Error("Failed to open cache file for writing.");

  ofs << rfl::json::write(data);

  fmt::println("Successfully wrote to cache file.");

  return 0;
}

fn WriteCallback(void* contents, const usize size, const usize nmemb, string* str) -> usize {
  const usize totalSize = size * nmemb;
  str->append(static_cast<char*>(contents), totalSize);
  return totalSize;
}

// Function to make API request
fn MakeApiRequest(const string& url) -> Result<WeatherOutput> {
  fmt::println("Making API request to URL: {}", url);

  CURL*  curl = curl_easy_init();
  string responseBuffer;

  if (curl) {
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBuffer);
    const CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
      return Error(fmt::format("Failed to perform cURL request: {}", curl_easy_strerror(res)));

    fmt::println("Received response from API. Response size: {}", responseBuffer.size());
    fmt::println("Response: {}", responseBuffer);

    WeatherOutput output = rfl::json::read<WeatherOutput>(responseBuffer).value();

    return output; // Return an empty result for now
  }

  return Error("Failed to initialize cURL.");
}

// Core function to get weather information
fn Weather::getWeatherInfo() const -> WeatherOutput {
  using namespace std::chrono;

  // Check if cache is valid
  if (Result<WeatherOutput> data = ReadCacheFromFile()) {
    WeatherOutput dataVal = *data;

    if (system_clock::now() - system_clock::time_point(seconds(dataVal.dt)) < minutes(10)) {
      fmt::println("Cache is valid. Returning cached data.");

      return dataVal;
    }

    fmt::println("Cache is expired.");
  } else {
    fmt::println("No valid cache found.");
  }

  WeatherOutput result;

  if (holds_alternative<string>(location)) {
    const string city = get<string>(location);

    const char* loc = curl_easy_escape(nullptr, city.c_str(), static_cast<int>(city.length()));

    fmt::println("City: {}", loc);

    const string apiUrl = fmt::format(
      "https://api.openweathermap.org/data/2.5/"
      "weather?q={}&appid={}&units={}",
      loc,
      api_key,
      units
    );

    result = MakeApiRequest(apiUrl).value();
  } else {
    const auto [lat, lon] = get<Coords>(location);

    fmt::println("Coordinates: lat = {:.3f}, lon = {:.3f}", lat, lon);

    const string apiUrl = fmt::format(
      "https://api.openweathermap.org/data/2.5/"
      "weather?lat={:.3f}&lon={:.3f}&appid={}&units={}",
      lat,
      lon,
      api_key,
      units
    );

    result = MakeApiRequest(apiUrl).value();
  }

  // Update the cache with the new data
  WriteCacheToFile(result);

  fmt::println("Returning new data.");

  return result;
}
