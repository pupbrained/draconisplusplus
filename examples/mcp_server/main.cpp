/**
 * @file main.cpp
 * @brief Draconis++ MCP server example
 *
 * This example demonstrates how to create an MCP server that exposes
 * Draconis++ library functionality via standard input/output, making it
 * compatible with stdio-based MCP clients.
 */

#include <chrono>
#include <cstdlib>
#include <glaze/core/meta.hpp>
#include <glaze/core/read.hpp>
#include <glaze/core/write.hpp>
#include <glaze/glaze.hpp>
#include <glaze/json/json_t.hpp>
#include <glaze/json/read.hpp>
#include <glaze/json/write.hpp>
#include <iostream>
#include <magic_enum/magic_enum.hpp>
#include <matchit.hpp>
#include <utility>

#include <Drac++/Core/System.hpp>
#include <Drac++/Services/Packages.hpp>
#include <Drac++/Services/Weather.hpp>

#include <Drac++/Utils/CacheManager.hpp>
#include <Drac++/Utils/DataTypes.hpp>
#include <Drac++/Utils/Error.hpp>
#include <Drac++/Utils/Types.hpp>

using namespace draconis::utils::types;
using namespace draconis::core::system;
using namespace draconis::services::weather;
using namespace draconis::services::packages;
using enum draconis::utils::error::DracErrorCode;

using GlzArray   = glz::json_t::array_t;
using GlzJson    = glz::json_t;
using GlzObject  = glz::json_t::object_t;
using GlzRawJson = glz::raw_json;
using GlzVal     = glz::json_t::val_t;

struct ToolResponse {
  GlzJson result;
  bool    isError = false;

  ToolResponse() = default;
  ToolResponse(GlzJson result) : result(std::move(result)) {}
  ToolResponse(GlzJson result, bool isError) : result(std::move(result)), isError(isError) {}
};

struct ToolParam {
  String name;
  String description;
  String type     = "string";
  bool   required = false;
};

struct Tool {
  String         name;
  String         description;
  Vec<ToolParam> parameters;

  Tool() = default;
  Tool(String name, String description, Vec<ToolParam> parameters)
    : name(std::move(name)), description(std::move(description)), parameters(std::move(parameters)) {}
  Tool(String name, String description)
    : name(std::move(name)), description(std::move(description)) {}
  Tool(String name, String description, ToolParam parameter)
    : name(std::move(name)), description(std::move(description)), parameters({ std::move(parameter) }) {}
};

struct SystemInfoResponse {
  Option<OSInfo>   operatingSystem;
  Option<String>   kernelVersion;
  Option<String>   host;
  Option<String>   shell;
  Option<String>   desktopEnv;
  Option<String>   windowMgr;
  Option<String>   cpuModel;
  Option<CPUCores> cpuCores;
};

struct HardwareInfoResponse {
  Option<String>        cpuModel;
  Option<CPUCores>      cpuCores;
  Option<String>        gpuModel;
  Option<ResourceUsage> memInfo;
  Option<ResourceUsage> diskUsage;
};

struct NetworkInfoResponse {
  Option<Vec<NetworkInterface>> interfaces;
  Option<NetworkInterface>      primaryInterface;
};

struct DisplayInfoResponse {
  Option<Vec<DisplayInfo>> displays;
  Option<DisplayInfo>      primaryDisplay;
};

struct UptimeInfoResponse {
  u32    seconds;
  String formatted;
};

struct ComprehensiveInfo {
  SystemInfoResponse       system;
  HardwareInfoResponse     hardware;
  NetworkInfoResponse      network;
  DisplayInfoResponse      display;
  UptimeInfoResponse       uptime;
  Option<Report>           weather;
  Option<Map<String, u64>> packages;
  Option<MediaInfo>        nowPlaying;
};

namespace glz {
  template <>
  struct meta<ToolResponse> {
    using T = ToolResponse;

    // clang-format off
    static constexpr detail::Object value = object(
      "result",  &T::result,
      "isError", &T::isError
    );
    // clang-format on
  };

  template <>
  struct meta<ToolParam> {
    using T = ToolParam;

    // clang-format off
    static constexpr detail::Object value = object(
      "name",        &T::name,
      "description", &T::description,
      "required",    &T::required,
      "type",        &T::type
    );
    // clang-format on
  };

  template <>
  struct meta<Tool> {
    using T = Tool;

    // clang-format off
    static constexpr detail::Object value = object(
      "name",        &T::name,
      "description", &T::description,
      "parameters",  &T::parameters
    );
    // clang-format on
  };

  template <>
  struct meta<SystemInfoResponse> {
    using T = SystemInfoResponse;

    // clang-format off
    static constexpr detail::Object value = object(
      "operatingSystem", &T::operatingSystem,
      "kernelVersion",   &T::kernelVersion,
      "host",            &T::host,
      "shell",           &T::shell,
      "desktopEnv",      &T::desktopEnv,
      "windowMgr",       &T::windowMgr,
      "cpuModel",        &T::cpuModel,
      "cpuCores",        &T::cpuCores
    );
    // clang-format on
  };

  template <>
  struct meta<HardwareInfoResponse> {
    using T = HardwareInfoResponse;

    // clang-format off
    static constexpr detail::Object value = object(
      "cpuModel",  &T::cpuModel,
      "cpuCores",  &T::cpuCores,
      "gpuModel",  &T::gpuModel,
      "memInfo",   &T::memInfo,
      "diskUsage", &T::diskUsage
    );
    // clang-format on
  };

  template <>
  struct meta<NetworkInfoResponse> {
    using T = NetworkInfoResponse;

    // clang-format off
    static constexpr detail::Object value = object(
      "interfaces",       &T::interfaces,
      "primaryInterface", &T::primaryInterface
    );
    // clang-format on
  };

  template <>
  struct meta<DisplayInfoResponse> {
    using T = DisplayInfoResponse;

    // clang-format off
    static constexpr detail::Object value = object(
      "displays",       &T::displays,
      "primaryDisplay", &T::primaryDisplay
    );
    // clang-format on
  };

  template <>
  struct meta<UptimeInfoResponse> {
    using T = UptimeInfoResponse;

    // clang-format off
    static constexpr detail::Object value = object(
      "seconds",   &T::seconds,
      "formatted", &T::formatted
    );
    // clang-format on
  };

  template <>
  struct meta<ComprehensiveInfo> {
    using T = ComprehensiveInfo;

    // clang-format off
    static constexpr detail::Object value = object(
      "system",     &T::system,
      "hardware",   &T::hardware,
      "network",    &T::network,
      "display",    &T::display,
      "uptime",     &T::uptime,
      "weather",    &T::weather,
      "packages",   &T::packages,
      "nowPlaying", &T::nowPlaying
    );
    // clang-format on
  };

  template <>
  struct meta<MediaInfo> {
    using T = MediaInfo;

    // clang-format off
    static constexpr detail::Object value = object(
      "title",  &T::title,
      "artist", &T::artist
    );
    // clang-format on
  };

  template <>
  struct meta<ResourceUsage> {
    using T = ResourceUsage;

    // clang-format off
    static constexpr detail::Object value = object(
      "usedBytes",  &T::usedBytes,
      "totalBytes", &T::totalBytes
    );
    // clang-format on
  };
} // namespace glz

namespace {
  using ToolHandler        = Fn<ToolResponse(const Map<String, String>&)>;
  using NoParamToolHandler = Fn<ToolResponse()>;

  fn GetCacheManager() -> CacheManager& {
    static CacheManager CacheManager;
    return CacheManager;
  }

  template <typename T>
  fn resultToJson(const Result<T>& result) -> GlzJson {
    if (result)
      return *result;

    return {
      { "error", result.error().message }
    };
  }

  template <typename T>
  fn serializeToJson(const T& obj) -> GlzJson {
    String jsonStr;

    if (glz::error_ctx errc = glz::write_json(obj, jsonStr); !errc) {
      GlzJson jsonVal;

      if (!glz::read_json(jsonVal, jsonStr))
        return jsonVal;
    }

    return {
      { "error", "Failed to serialize result" }
    };
  }

  fn makeErrorResult(StringView message, i32 code = -1) -> GlzJson {
    return {
      { "error", { { "message", String(message) }, { "code", code } } }
    };
  }

  template <typename T>
  fn makeSuccessResult(const T& data) -> GlzJson {
    return {
      { "data", serializeToJson(data) }
    };
  }

  fn SystemInfoHandler() -> ToolResponse {
    CacheManager& cacheManager = GetCacheManager();

    SystemInfoResponse info;

    if (Result res = GetOperatingSystem(cacheManager); res)
      info.operatingSystem = *res;
    if (Result res = GetKernelVersion(cacheManager); res)
      info.kernelVersion = *res;
    if (Result res = GetHost(cacheManager); res)
      info.host = *res;
    if (Result res = GetShell(cacheManager); res)
      info.shell = *res;
    if (Result res = GetDesktopEnvironment(cacheManager); res)
      info.desktopEnv = *res;
    if (Result res = GetWindowManager(cacheManager); res)
      info.windowMgr = *res;
    if (Result res = GetCPUModel(cacheManager); res)
      info.cpuModel = *res;
    if (Result res = GetCPUCores(cacheManager); res)
      info.cpuCores = *res;

    return { makeSuccessResult(info) };
  }

  fn HardwareInfoHandler() -> ToolResponse {
    CacheManager& cacheManager = GetCacheManager();

    HardwareInfoResponse info;

    if (Result res = GetCPUModel(cacheManager); res)
      info.cpuModel = *res;
    if (Result res = GetCPUCores(cacheManager); res)
      info.cpuCores = *res;
    if (Result res = GetGPUModel(cacheManager); res)
      info.gpuModel = *res;
    if (Result res = GetMemInfo(cacheManager); res)
      info.memInfo = *res;
    if (Result res = GetDiskUsage(cacheManager); res)
      info.diskUsage = *res;

    return { makeSuccessResult(info) };
  }

  fn WeatherHandler(const Map<String, String>& params) -> ToolResponse {
#if DRAC_ENABLE_WEATHER
    Result<Coords> coordsResult;
    String         location;

    auto iter = params.find("location");

    if (iter != params.end() && !iter->second.empty()) {
      location     = iter->second;
      coordsResult = Geocode(location);
      if (!coordsResult)
        return { makeErrorResult("Failed to geocode location '" + location + "': " + coordsResult.error().message), true };
    } else {
      Result locationInfoResult = GetCurrentLocationInfoFromIP();
      if (!locationInfoResult)
        return { makeErrorResult("Failed to get current location from IP: " + locationInfoResult.error().message), true };

      const IPLocationInfo& locationInfo = *locationInfoResult;
      coordsResult                       = locationInfo.coords;
      location                           = locationInfo.locationName;
    }

    UniquePointer<IWeatherService> weatherService = CreateWeatherService(
      Provider::MetNo, *coordsResult, UnitSystem::Imperial
    );

    if (!weatherService)
      return { makeErrorResult("Failed to create weather service"), true };

    String         weatherCacheKey = "weather_" + location;
    Result<Report> weatherResult   = GetCacheManager().getOrSet<Report>(
      weatherCacheKey, [&]() -> Result<Report> { return weatherService->getWeatherInfo(); }
    );

    if (!weatherResult)
      return { makeErrorResult("Failed to fetch weather data: " + weatherResult.error().message), true };

    return { makeSuccessResult(*weatherResult) };
#else
    return { makeErrorResult("Weather service not enabled in this build"), true };
#endif
  }

  fn PackageCountHandler(const Map<String, String>& params) -> ToolResponse {
#if DRAC_ENABLE_PACKAGECOUNT
    using enum Manager;

    Manager enabledManagers = NONE;

    static const UnorderedMap<String, Manager> MANAGER_MAP = {
      {  "cargo",  CARGO },
  #if defined(__linux__) || defined(__APPLE__)
      {    "nix",    NIX },
  #endif
  #ifdef __linux__
      {    "apk",    APK },
      {   "dpkg",   DPKG },
      {   "moss",   MOSS },
      { "pacman", PACMAN },
      {    "rpm",    RPM },
      {   "xbps",   XBPS },
  #elifdef __APPLE__
      { "homebrew", HOMEBREW },
      { "macports", MACPORTS },
  #elifdef _WIN32
      { "winget", WINGET },
      { "chocolatey", CHOCOLATEY },
      { "scoop", SCOOP },
  #elif defined(__FreeBSD__) || defined(__DragonFly__)
      { "pkgng", PKGNG },
  #elifdef __NetBSD__
      { "pkgsrc", PKGSRC },
  #elifdef __HAIKU__
      { "haikupkg", HAIKUPKG },
  #endif
    };

    auto mgrIter = params.find("managers");

    if (mgrIter != params.end() && !mgrIter->second.empty()) {
      String      managersStr = mgrIter->second;
      Vec<String> managersList;

      managersList.reserve(std::count(managersStr.begin(), managersStr.end(), ',') + 1);
      usize pos = 0;

      while ((pos = managersStr.find(',')) != String::npos) {
        managersList.emplace_back(managersStr.substr(0, pos));
        managersStr.erase(0, pos + 1);
      }

      managersList.emplace_back(managersStr);

      for (const String& mgr : managersList) {
        auto iter = MANAGER_MAP.find(mgr);

        if (iter != MANAGER_MAP.end())
          enabledManagers |= iter->second;
      }
    } else {
      for (const auto& [_, value] : MANAGER_MAP)
        enabledManagers |= value;
    }

    if (enabledManagers == NONE)
      return { makeErrorResult("No valid package managers specified or available"), true };

    Result<Map<String, u64>> countResult = GetIndividualCounts(GetCacheManager(), enabledManagers);
    if (!countResult)
      return { makeErrorResult("Failed to get package count: " + countResult.error().message), true };

    return { makeSuccessResult(*countResult) };
#else
    return { makeErrorResult("Package counting not enabled in this build"), true };
#endif
  }

  fn NetworkInfoHandler() -> ToolResponse {
    CacheManager& cacheManager = GetCacheManager();

    NetworkInfoResponse info;
    if (Result res = GetNetworkInterfaces(cacheManager); res)
      info.interfaces = *res;
    if (Result res = GetPrimaryNetworkInterface(cacheManager); res)
      info.primaryInterface = *res;

    return { makeSuccessResult(info) };
  }

  fn DisplayInfoHandler() -> ToolResponse {
    CacheManager& cacheManager = GetCacheManager();

    DisplayInfoResponse info;
    if (Result<Vec<DisplayInfo>> res = GetOutputs(cacheManager); res)
      info.displays = *res;
    if (Result<DisplayInfo> res = GetPrimaryOutput(cacheManager); res)
      info.primaryDisplay = *res;

    if (info.displays->empty())
      return { makeErrorResult("No displays found"), true };

    return { makeSuccessResult(info) };
  }

  fn UptimeHandler() -> ToolResponse {
    Result<std::chrono::seconds> uptimeResult = GetUptime();
    if (!uptimeResult)
      return { makeErrorResult("Failed to get uptime: " + uptimeResult.error().message), true };

    u32 seconds          = uptimeResult->count();
    u32 hours            = seconds / 3600;
    u32 minutes          = (seconds % 3600) / 60;
    u32 remainingSeconds = seconds % 60;

    UptimeInfoResponse info { .seconds = seconds, .formatted = std::format("{}h {}m {}s", hours, minutes, remainingSeconds) };
    return { makeSuccessResult(info) };
  }

  fn NowPlayingHandler() -> ToolResponse {
#if DRAC_ENABLE_NOWPLAYING
    Result nowPlayingResult = GetNowPlaying();
    if (!nowPlayingResult)
      return { makeErrorResult("Failed to get now playing info: " + nowPlayingResult.error().message), true };

    return { makeSuccessResult(*nowPlayingResult) };
#else
    return { makeErrorResult("Now playing functionality not enabled in this build"), true };
#endif
  }

  fn ComprehensiveInfoHandler(const Map<String, String>& params) -> ToolResponse {
    CacheManager& cacheManager = GetCacheManager();

    ComprehensiveInfo info;

    if (Result res = GetOperatingSystem(cacheManager); res)
      info.system.operatingSystem = *res;
    if (Result res = GetKernelVersion(cacheManager); res)
      info.system.kernelVersion = *res;
    if (Result res = GetHost(cacheManager); res)
      info.system.host = *res;
    if (Result res = GetShell(cacheManager); res)
      info.system.shell = *res;
    if (Result res = GetDesktopEnvironment(cacheManager); res)
      info.system.desktopEnv = *res;
    if (Result res = GetWindowManager(cacheManager); res)
      info.system.windowMgr = *res;

    if (Result res = GetCPUModel(cacheManager); res)
      info.hardware.cpuModel = *res;
    if (Result res = GetCPUCores(cacheManager); res)
      info.hardware.cpuCores = *res;
    if (Result res = GetGPUModel(cacheManager); res)
      info.hardware.gpuModel = *res;
    if (Result res = GetMemInfo(cacheManager); res)
      info.hardware.memInfo = *res;
    if (Result res = GetDiskUsage(cacheManager); res)
      info.hardware.diskUsage = *res;

    if (Result res = GetNetworkInterfaces(cacheManager); res)
      info.network.interfaces = *res;
    if (Result res = GetPrimaryNetworkInterface(cacheManager); res)
      info.network.primaryInterface = *res;

    if (Result res = GetOutputs(cacheManager); res)
      info.display.displays = *res;
    if (Result res = GetPrimaryOutput(cacheManager); res)
      info.display.primaryDisplay = *res;

    if (Result res = GetUptime(); res) {
      u32 seconds          = res->count();
      u32 hours            = seconds / 3600;
      u32 minutes          = (seconds % 3600) / 60;
      u32 remainingSeconds = seconds % 60;
      info.uptime          = { .seconds = seconds, .formatted = std::format("{}h {}m {}s", hours, minutes, remainingSeconds) };
    }

#if DRAC_ENABLE_WEATHER
    auto locIter = params.find("location");
    if (locIter != params.end() && !locIter->second.empty()) {
      if (Result coords = Geocode(locIter->second); coords)
        if (UniquePointer<IWeatherService> weatherService = CreateWeatherService(Provider::MetNo, *coords, UnitSystem::Imperial)) {
          String weatherCacheKey = "weather_" + locIter->second;
          if (Result weather = cacheManager.getOrSet<Report>(weatherCacheKey, [&]() -> Result<Report> { return weatherService->getWeatherInfo(); }); weather)
            info.weather = *weather;
        }
    } else if (Result locationInfo = GetCurrentLocationInfoFromIP(); locationInfo)
      if (UniquePointer<IWeatherService> weatherService = CreateWeatherService(Provider::MetNo, locationInfo->coords, UnitSystem::Imperial)) {
        String weatherCacheKey = "weather_" + locationInfo->locationName;
        if (
          Result weather = cacheManager.getOrSet<Report>(
            weatherCacheKey,
            [&]() -> Result<Report> {
              return weatherService->getWeatherInfo();
            }
          );
          weather
        )
          info.weather = *weather;
      }
#endif

#if DRAC_ENABLE_PACKAGECOUNT
    using enum Manager;

    Manager enabledManagers = CARGO;
  #if defined(__linux__) || defined(__APPLE__)
    enabledManagers |= NIX;
  #endif
  #ifdef __linux__
    enabledManagers |= APK | DPKG | MOSS | PACMAN | RPM | XBPS;
  #elifdef __APPLE__
    enabledManagers |= HOMEBREW | MACPORTS;
  #elifdef _WIN32
    enabledManagers |= WINGET | CHOCOLATEY | SCOOP;
  #elif defined(__FreeBSD__) || defined(__DragonFly__)
    enabledManagers |= PKGNG;
  #elifdef __NetBSD__
    enabledManagers |= PKGSRC;
  #elifdef __HAIKU__
    enabledManagers |= HAIKUPKG;
  #endif
    if (Result packages = GetIndividualCounts(cacheManager, enabledManagers); packages)
      info.packages = *packages;
#endif

#if DRAC_ENABLE_NOWPLAYING
    if (Result nowPlaying = GetNowPlaying(); nowPlaying)
      info.nowPlaying = *nowPlaying;
#endif

    return { makeSuccessResult(info) };
  }

  fn CacheClearHandler() -> ToolResponse {
    return { makeSuccessResult(std::format("Removed {} files.", GetCacheManager().invalidateAll(false))) };
  }
} // namespace

class DracStdioServer {
 public:
  DracStdioServer(String name, String version)
    : m_name(std::move(name)), m_version(std::move(version)) {}

  fn setCapabilities(const GlzObject& capabilities) -> void {
    m_capabilities = capabilities;
  }

  fn registerTool(const Tool& tool, const ToolHandler& handler) -> void {
    m_tools[tool.name] = { tool, handler };
  }

  fn registerTool(const Tool& tool, const NoParamToolHandler& handler) -> void {
    m_tools[tool.name] = { tool, [handler](const Map<String, String>&) -> ToolResponse { return handler(); } };
  }

  fn run() -> Result<> {
    String line;

    while (std::getline(std::cin, line)) {
      if (line.empty())
        continue;

      GlzObject requestJson;

      glz::error_ctx errc = glz::read_json(requestJson, line);

      if (errc) {
        std::cerr << "Failed to parse input: " << glz::format_error(errc, line) << '\n';
        continue;
      }

      String method = requestJson.contains("method") ? requestJson["method"].get<String>() : "";

      GlzJson params = requestJson.contains("params") ? requestJson["params"] : GlzJson {};

      String jsonrpc = requestJson.contains("jsonrpc") ? requestJson["jsonrpc"].get<String>() : "2.0";

      try {
        Result result = processRequest(method, params);

        if (requestJson.contains("id")) {
          GlzJson idVal = requestJson["id"];

          GlzObject response;
          response["jsonrpc"] = jsonrpc;
          response["id"]      = idVal;

          if (result) {
            response["result"] = *result;
          } else {
            response["error"] = GlzObject {
              {    "code",                                              -32603 },
              { "message", "Internal error: " + String(result.error().message) }
            };
          }

          String responseStr;
          if (glz::error_ctx errc = glz::write_json(response, responseStr); errc)
            ERR_FMT(ParseError, "Failed to serialize response: {}", glz::format_error(errc, responseStr));

          std::cout << responseStr << '\n';
          std::cout.flush();
        }
      } catch (const Exception& e) {
        if (requestJson.contains("id")) {
          GlzJson idVal = requestJson["id"];

          GlzObject response;
          response["jsonrpc"] = jsonrpc;
          response["id"]      = idVal;
          response["error"]   = GlzObject {
              {    "code",                                -32603 },
              { "message", "Internal error: " + String(e.what()) }
          };

          String responseStr;
          if (glz::error_ctx errc = glz::write_json(response, responseStr); errc)
            ERR_FMT(ParseError, "Failed to serialize error response: {}", glz::format_error(errc, responseStr));

          std::cout << responseStr << '\n';
          std::cout.flush();
        } else {
          std::cerr << "Internal error: " << e.what() << '\n';
        }
      }
    }
    return {};
  }

 private:
  using Tools = Map<String, Pair<Tool, ToolHandler>>;

  String    m_name;
  String    m_version;
  GlzObject m_capabilities;
  Tools     m_tools;

  fn processRequest(const String& method, const GlzJson& params) -> Result<GlzJson> {
    if (method == "initialize") {
      return GlzObject {
        { "protocolVersion",                                               "2025-06-18" },
        {    "capabilities",                                             m_capabilities },
        {      "serverInfo", GlzObject { { "name", m_name }, { "version", m_version } } }
      };
    }

    if (method == "tools/list") {
      GlzArray toolsArray;
      toolsArray.reserve(m_tools.size());

      for (const auto& [_, toolPair] : m_tools) {
        GlzObject toolObj;
        toolObj["name"]        = toolPair.first.name;
        toolObj["description"] = toolPair.first.description;

        GlzObject inputSchema;
        inputSchema["type"] = "object";
        GlzObject inputProperties;
        GlzArray  inputRequired;

        for (const ToolParam& param : toolPair.first.parameters) {
          inputProperties[param.name] = GlzObject {
            { "title", param.name },
            {  "type", param.type }
          };

          if (param.required)
            inputRequired.emplace_back(param.name);
        }

        inputSchema["properties"] = inputProperties;
        inputSchema["required"]   = inputRequired;
        inputSchema["title"]      = toolPair.first.name + "Arguments";

        GlzObject outputSchema = {
          {       "type",                                                                                                                   "object" },
          { "properties", { { "data", { { "title", "Data" }, { "type", "object" } } }, { "error", { { "title", "Error" }, { "type", "object" } } } } },
          {      "title",                                                                                             toolPair.first.name + "Output" }
        };

        toolObj["inputSchema"]  = inputSchema;
        toolObj["outputSchema"] = outputSchema;
        toolsArray.emplace_back(toolObj);
      }

      return GlzObject {
        { "tools", toolsArray }
      };
    }

    if (method == "tools/call") {
      if (!params.contains("name"))
        ERR(InvalidArgument, "Missing tool name");

      String toolName = params["name"].get<String>();

      auto iter = m_tools.find(toolName);

      if (iter == m_tools.end())
        ERR_FMT(NotFound, "Tool not found: {}", toolName);

      Map<String, String> arguments;

      if (params.contains("arguments")) {
        const GlzJson& args = params["arguments"];

        if (args.is_object())
          for (const auto& [key, value] : args.get<GlzObject>()) {
            if (value.is_string())
              arguments[key] = value.get<String>();
            else
              ERR_FMT(InvalidArgument, "Argument '{}' must be a string", key);
          }
      }

      ToolResponse result = iter->second.second(arguments);

      GlzObject out {
        { "structuredContent", { { "result", result.result }, { "isError", result.isError } } }
      };

      String outStr;

      if (!result.result.is_string()) {
        if (glz::error_ctx errc = glz::write_json(result.result, outStr); errc)
          ERR_FMT(ParseError, "Failed to serialize result: {}", glz::format_error(errc, outStr));
      } else
        outStr = result.result.get<String>();

      GlzArray contentArr;

      contentArr.emplace_back(GlzObject {
        { "type", "text" },
        { "text", outStr },
      });

      out["content"] = contentArr;

      return out;
    }

    if (method == "resources/list")
      return GlzObject {
        { "resources", GlzArray() }
      };

    if (method == "prompts/list")
      return GlzObject {
        { "prompts", GlzArray() }
      };

    if (method == "ping" || method == "notifications/initialized")
      return GlzObject {};

    ERR_FMT(NotSupported, "Unknown method: {}", method);
  }
};

fn main() -> i32 {
  DracStdioServer server("Draconis++ MCP Server", DRAC_VERSION);

  server.setCapabilities({
    { "tools", { { "listChanged", true } } }
  });

  Tool systemInfoTool("system_info", "Get system information (OS, kernel, host, shell, desktop environment, window manager)");
  Tool hardwareInfoTool("hardware_info", "Get hardware information (CPU, GPU, memory, disk, battery)");
  Tool networkInfoTool("network_info", "Get network interface information");
  Tool displayInfoTool("display_info", "Get display/monitor information");
  Tool uptimeTool("uptime", "Get system uptime");
  Tool nowPlayingTool("now_playing", "Get currently playing media information (title and artist)");

  Tool weatherTool(
    "weather",
    "Get current weather information. If no location is specified, automatically detects your current location from IP address.",
    ToolParam("location", "Location name (e.g., 'New York, NY', 'London, UK', 'Tokyo, Japan'). Omit this parameter to use your current location.")
  );

  Tool packageCountTool(
    "package_count",
    "Get individual package counts from available package managers",
    ToolParam("managers", "Comma-separated list of package managers to check (e.g., 'pacman,dpkg,cargo'). Omit this parameter to check all available package managers.")
  );

  Tool comprehensiveTool(
    "comprehensive_info",
    "Get all system information at once (system, hardware, network, display, uptime, weather, individual package counts)",
    ToolParam("location", "Location name for weather information (e.g., 'New York, NY', 'London, UK'). Omit this parameter to use your current location for weather.")
  );

  Tool cacheClearTool("cache_clear", "Clear all cached data");

  server.registerTool(systemInfoTool, SystemInfoHandler);
  server.registerTool(hardwareInfoTool, HardwareInfoHandler);
  server.registerTool(weatherTool, WeatherHandler);
  server.registerTool(packageCountTool, PackageCountHandler);
  server.registerTool(networkInfoTool, NetworkInfoHandler);
  server.registerTool(displayInfoTool, DisplayInfoHandler);
  server.registerTool(uptimeTool, UptimeHandler);
  server.registerTool(nowPlayingTool, NowPlayingHandler);
  server.registerTool(comprehensiveTool, ComprehensiveInfoHandler);
  server.registerTool(cacheClearTool, CacheClearHandler);

  Result res = server.run();

  if (res)
    return EXIT_SUCCESS;

  std::cerr << "Error: " << res.error().message << '\n';
  return EXIT_FAILURE;
}
