#include <curl/curl.h>
#include <rfl/json.hpp>
#include <rfl/json/load.hpp>
#include <util/result.h>

#include "config.h"

using WeatherOutput = Weather::WeatherOutput;

// Function to read cache from file
Result<WeatherOutput> ReadCacheFromFile() {
  const std::string cacheFile = "/tmp/weather_cache.json";
  std::ifstream ifs(cacheFile);

  if (!ifs.is_open())
    return Error("Cache file not found.");

  fmt::println("Reading from cache file...");

  WeatherOutput val;

  try {
    std::stringstream buf;

    buf << ifs.rdbuf();

    val = rfl::json::read<WeatherOutput>(buf.str()).value();
  } catch (Error& e) {
    return e;
  }

  fmt::println("Successfully read from cache file.");

  return Ok(val);
}

// Function to write cache to file
Result<> WriteCacheToFile(const WeatherOutput& data) {
  const std::string cacheFile = "/tmp/weather_cache.json";
  fmt::println("Writing to cache file...");
  std::ofstream ofs(cacheFile);

  if (!ofs.is_open())
    return Error("Failed to open cache file for writing.");

  ofs << rfl::json::write(data);

  fmt::println("Successfully wrote to cache file.");

  return Ok();
}

size_t WriteCallback(void* contents,
                     size_t size,
                     size_t nmemb,
                     std::string* buffer) {
  size_t realsize = size * nmemb;

  buffer->append(static_cast<char*>(contents), realsize);

  return realsize;
}

// Function to make API request
Result<WeatherOutput> MakeApiRequest(const std::string& url) {
  fmt::println("Making API request...");

  CURL* curl = curl_easy_init();

  std::string responseBuffer;

  if (curl) {
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBuffer);
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
      return Error(fmt::format("Failed to perform cURL request: {}",
                               curl_easy_strerror(res)));

    fmt::println("Received response from API.");
    // Parse the JSON response
    WeatherOutput output =
        rfl::json::read<WeatherOutput>(responseBuffer).value();

    return Ok(output);
  }

  return Error("Failed to initialize cURL.");
}

// Core function to get weather information
WeatherOutput Weather::getWeatherInfo() const {
  using namespace std::chrono;

  const Location loc       = m_Location;
  const std::string apiKey = m_ApiKey;
  const std::string units  = m_Units;

  // Check if cache is valid
  if (Result<WeatherOutput> data = ReadCacheFromFile(); data.isOk()) {
    WeatherOutput dataVal = data.value();

    if (system_clock::now() - system_clock::time_point(seconds(dataVal.dt)) <
        minutes(10)) { // Assuming cache duration is always 10 minutes
      fmt::println("Cache is valid. Returning cached data.");

      return dataVal;
    }

    fmt::println("Cache is expired.");
  } else {
    fmt::println("No valid cache found.");
  }

  WeatherOutput result;

  if (holds_alternative<std::string>(loc)) {
    const std::string city = get<std::string>(loc);

    const char* location = curl_easy_escape(nullptr, city.c_str(),
                                            static_cast<int>(city.length()));

    fmt::println("City: {}", location);

    const std::string apiUrl = fmt::format(
        "https://api.openweathermap.org/data/2.5/"
        "weather?q={}&appid={}&units={}",
        location, apiKey, units);

    result = MakeApiRequest(apiUrl).value();
  } else {
    const auto [lat, lon] = get<Coords>(loc);

    fmt::println("Coordinates: lat = {:.3f}, lon = {:.3f}", lat, lon);

    const std::string apiUrl = fmt::format(
        "https://api.openweathermap.org/data/2.5/"
        "weather?lat={:.3f}&lon={:.3f}&appid={}&units={}",
        lat, lon, apiKey, units);

    result = MakeApiRequest(apiUrl).value();
  }

  // Update the cache with the new data
  WriteCacheToFile(result);

  fmt::println("Returning new data.");

  return result;
}
