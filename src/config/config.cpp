#include "config.h"
#include <fmt/core.h>
#include <toml++/toml.h>
#include <unistd.h>
#include <rfl.hpp>
#include <rfl/toml.hpp>

#define DEFINE_GETTER(class_name, type, name) \
  type class_name::get##name() const {        \
    return m_##name;                          \
  }

DEFINE_GETTER(Config, const General, General)
DEFINE_GETTER(Config, const NowPlaying, NowPlaying)
DEFINE_GETTER(Config, const Weather, Weather)
DEFINE_GETTER(General, const string, Name)
DEFINE_GETTER(NowPlaying, bool, Enabled)
DEFINE_GETTER(Weather, const Weather::Location, Location)
DEFINE_GETTER(Weather, const string, ApiKey)
DEFINE_GETTER(Weather, const string, Units)

const Config& Config::getInstance() {
  static const Config& INSTANCE =
      *new Config(rfl::toml::load<Config>("./config.toml").value());
  return INSTANCE;
}

Config::Config(General general, NowPlaying now_playing, Weather weather)
    : m_General(std::move(general)),
      m_NowPlaying(now_playing),
      m_Weather(std::move(weather)) {}

General::General(string name) : m_Name(std::move(name)) {}

NowPlaying::NowPlaying(bool enable) : m_Enabled(enable) {}

Weather::Weather(Location location, string api_key, string units)
    : m_Location(std::move(location)),
      m_ApiKey(std::move(api_key)),
      m_Units(std::move(units)) {}

WeatherImpl WeatherImpl::from_class(const Weather& weather) noexcept {
  return {
      .location = weather.getLocation(),
      .api_key  = weather.getApiKey(),
      .units    = weather.getUnits(),
  };
}

Weather WeatherImpl::to_class() const {
  return {location, api_key, units};
}

GeneralImpl GeneralImpl::from_class(const General& general) noexcept {
  return {general.getName()};
}

General GeneralImpl::to_class() const {
  return {name};
}

NowPlayingImpl NowPlayingImpl::from_class(
    const NowPlaying& now_playing) noexcept {
  return {.enabled = now_playing.getEnabled()};
}

NowPlaying NowPlayingImpl::to_class() const {
  return {enabled};
}

ConfigImpl ConfigImpl::from_class(const Config& config) noexcept {
  return {
      .general     = config.getGeneral(),
      .now_playing = config.getNowPlaying(),
      .weather     = config.getWeather(),
  };
}

Config ConfigImpl::to_class() const {
  return {general, now_playing, weather};
}

boost::json::object Weather::getWeatherInfo() const {
  using namespace std;
  using namespace cpr;
  using namespace boost;
  using namespace std::chrono;

  const Location loc  = this->m_Location;
  const string apiKey = this->m_ApiKey;
  const string units  = this->m_Units;

  // Define cache file and cache duration
  const string cacheFile          = "/tmp/weather_cache.json";
  constexpr minutes cacheDuration = minutes(10);

  logi("Cache file: {}", cacheFile);
  logi("Cache duration: {} minutes",
       duration_cast<minutes>(cacheDuration).count());

  // Function to read cache from file
  auto readCacheFromFile =
      [&]() -> optional<pair<json::object, system_clock::time_point>> {
    ifstream ifs(cacheFile);

    if (!ifs.is_open()) {
      logi("Cache file not found.");
      return nullopt;
    }

    logi("Reading from cache file...");

    json::object cachedData;
    system_clock::time_point timestamp;

    try {
      json::value val;
      ifs >> val;
      cachedData = val.as_object();

      string tsStr = cachedData["timestamp"].as_string().c_str();
      timestamp    = system_clock::time_point(milliseconds(stoll(tsStr)));

      cachedData.erase("timestamp");
    } catch (...) {
      loge("Failed to read from cache file.");
      return nullopt;
    }

    logi("Successfully read from cache file.");
    return make_pair(cachedData, timestamp);
  };

  // Function to write cache to file
  auto writeCacheToFile = [&](const json::object& data) {
    fmt::println("Writing to cache file...");
    ofstream ofs(cacheFile);

    if (!ofs.is_open()) {
      loge("Failed to open cache file for writing.");
      return;
    }

    json::object dataToWrite = data;
    dataToWrite["timestamp"] = to_string(
        duration_cast<milliseconds>(system_clock::now().time_since_epoch())
            .count());
    ofs << json::serialize(dataToWrite);
    logi("Successfully wrote to cache file.");
  };

  // Check if cache is valid
  if (auto cachedData = readCacheFromFile()) {
    auto [data, timestamp] = *cachedData;
    if (system_clock::now() - timestamp < cacheDuration) {
      logi("Cache is valid. Returning cached data.");
      return data;
    }

    logi("Cache is expired.");
  } else {
    logi("No valid cache found.");
  }

  json::object result;
  if (holds_alternative<string>(loc)) {
    const string city = get<string>(loc);

    const char* location = curl_easy_escape(nullptr, city.c_str(),
                                            static_cast<int>(city.length()));
    logi("City: {}", location);

    logi("Making API request for city: {}", city);

    const Response res =
        Get(Url {fmt::format("https://api.openweathermap.org/data/2.5/"
                             "weather?q={}&appid={}&units={}",
                             location, apiKey, units)});

    logi("Received response from API.");
    json::value json = json::parse(res.text);
    result           = json.as_object();
  } else {
    const auto [lat, lon] = get<Coords>(loc);
    logi("Coordinates: lat = {:.3f}, lon = {:.3f}", lat, lon);

    logi("Making API request for coordinates.");
    const Response res =
        Get(Url {fmt::format("https://api.openweathermap.org/data/2.5/"
                             "weather?lat={:.3f}&lon={:.3f}&appid={}&units={}",
                             lat, lon, apiKey, units)});

    logi("Received response from API.");
    json::value json = json::parse(res.text);
    result           = json.as_object();
  }

  // Update the cache with the new data
  writeCacheToFile(result);

  logi("Returning new data.");

  return result;
}
