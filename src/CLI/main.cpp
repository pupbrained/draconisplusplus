#ifdef _WIN32
  #include <fcntl.h>
  #include <io.h>
  #include <windows.h>
#endif

#include <Drac++/Core/System.hpp>
#include <Drac++/Services/Packages.hpp>

#if DRAC_ENABLE_WEATHER
  #include <Drac++/Services/Weather.hpp>
#endif

#include <glaze/glaze.hpp>

#include <Drac++/Utils/ArgumentParser.hpp>
#include <Drac++/Utils/CacheManager.hpp>
#include <Drac++/Utils/Error.hpp>
#include <Drac++/Utils/Localization.hpp>
#include <Drac++/Utils/Logging.hpp>
#include <Drac++/Utils/Types.hpp>

#include "Config/Config.hpp"
#include "Core/SystemInfo.hpp"
#include "UI/UI.hpp"

using namespace draconis::utils::types;
using namespace draconis::utils::logging;
using namespace draconis::utils::localization;
using namespace draconis::core::system;
using namespace draconis::config;
using namespace draconis::ui;

namespace {
  fn WriteToConsole(const String& document) -> Unit {
#ifdef _WIN32
    if (HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE); hConsole != INVALID_HANDLE_VALUE)
      WriteConsoleA(hConsole, document.c_str(), static_cast<DWORD>(document.length()), nullptr, nullptr);
#else
    Println(document);
#endif
  }

  fn PrintDoctorReport(
#if DRAC_ENABLE_WEATHER
    const Result<Report>& weather,
#endif
    const SystemInfo& data
  ) -> Unit {
    using draconis::utils::error::DracError;

    Array<Option<Pair<String, DracError>>, 10 + DRAC_ENABLE_PACKAGECOUNT + DRAC_ENABLE_NOWPLAYING + DRAC_ENABLE_WEATHER>
      failures {};

    usize failureCount = 0;

#define DRAC_CHECK(expr, label) \
  if (!(expr))                  \
  failures.at(failureCount++) = { label, (expr).error() }

    DRAC_CHECK(data.date, "Date");
    DRAC_CHECK(data.host, "Host");
    DRAC_CHECK(data.kernelVersion, "KernelVersion");
    DRAC_CHECK(data.operatingSystem, "OperatingSystem");
    DRAC_CHECK(data.memInfo, "MemoryInfo");
    DRAC_CHECK(data.desktopEnv, "DesktopEnvironment");
    DRAC_CHECK(data.windowMgr, "WindowManager");
    DRAC_CHECK(data.diskUsage, "DiskUsage");
    DRAC_CHECK(data.shell, "Shell");
    DRAC_CHECK(data.uptime, "Uptime");

    if constexpr (DRAC_ENABLE_PACKAGECOUNT)
      DRAC_CHECK(data.packageCount, "PackageCount");

    if constexpr (DRAC_ENABLE_NOWPLAYING)
      DRAC_CHECK(data.nowPlaying, "NowPlaying");

    if constexpr (DRAC_ENABLE_WEATHER)
      DRAC_CHECK(weather, "Weather");

#undef DRAC_CHECK

    if (failureCount == 0)
      Println("All readouts were successful!");
    else {
      Println(
        "Out of {} readouts, {} failed.\n",
        failures.size(),
        failureCount
      );

      for (const Option<Pair<String, DracError>>& failure : failures)
        if (failure)
          Println(
            R"(Readout "{}" failed: {} ({}))",
            failure->first,
            failure->second.message,
            magic_enum::enum_name(failure->second.code)
          );
    }
  }

  fn PrintJsonOutput(
#if DRAC_ENABLE_WEATHER
    const Result<Report>& weather,
#endif
    const SystemInfo& data,
    bool              prettyJson
  ) -> Unit {
    using draconis::core::system::JsonInfo;

    JsonInfo output;

#define DRAC_SET_OPTIONAL(field) \
  if (data.field)                \
  output.field = *data.field

    DRAC_SET_OPTIONAL(date);
    DRAC_SET_OPTIONAL(host);
    DRAC_SET_OPTIONAL(kernelVersion);
    DRAC_SET_OPTIONAL(operatingSystem);
    DRAC_SET_OPTIONAL(memInfo);
    DRAC_SET_OPTIONAL(desktopEnv);
    DRAC_SET_OPTIONAL(windowMgr);
    DRAC_SET_OPTIONAL(diskUsage);
    DRAC_SET_OPTIONAL(shell);
    DRAC_SET_OPTIONAL(cpuModel);
    DRAC_SET_OPTIONAL(cpuCores);
    DRAC_SET_OPTIONAL(gpuModel);

    if (data.uptime)
      output.uptimeSeconds = data.uptime->count();

    if constexpr (DRAC_ENABLE_PACKAGECOUNT)
      DRAC_SET_OPTIONAL(packageCount);

    if constexpr (DRAC_ENABLE_NOWPLAYING)
      DRAC_SET_OPTIONAL(nowPlaying);

    if constexpr (DRAC_ENABLE_WEATHER)
      if (weather)
        output.weather = *weather;

#undef DRAC_SET_OPTIONAL

    String jsonStr;

    glz::error_ctx errorContext =
      prettyJson
      ? glz::write<glz::opts { .prettify = true }>(output, jsonStr)
      : glz::write_json(output, jsonStr);

    if (errorContext)
      WriteToConsole(std::format("Failed to write JSON output: {}", glz::format_error(errorContext, jsonStr)));
    else
      WriteToConsole(jsonStr);
  }
} // namespace

fn main(const i32 argc, CStr* argv[]) -> i32 try {
#ifdef _WIN32
  winrt::init_apartment();
#endif

  // clang-format off
  auto [
    doctorMode,
    clearCache,
    ignoreCacheRun,
    noAscii,
    jsonOutput,
    prettyJson,
    language
  ] = Tuple(false, false, false, false, false, false, String(""));
  // clang-format on

  {
    using draconis::utils::argparse::ArgumentParser;

    ArgumentParser parser(DRAC_VERSION);

    parser
      .addArguments("-V", "--verbose")
      .help("Enable verbose logging. Overrides --log-level.")
      .flag();

    parser
      .addArguments("-d", "--doctor")
      .help("Reports any failed readouts and their error messages.")
      .flag();

    parser
      .addArguments("-l", "--log-level")
      .help("Set the minimum log level.")
      .defaultValue(LogLevel::Info);

    parser
      .addArguments("--clear-cache")
      .help("Clears the cache. This will remove all cached data, including in-memory and on-disk copies.")
      .flag();

    parser
      .addArguments("--lang")
      .help("Set the language for localization (e.g., 'en', 'es', 'fr', 'de').")
      .defaultValue(String(""));

    parser
      .addArguments("--ignore-cache")
      .help("Ignore cache for this run (fetch fresh data without reading/writing on-disk cache).")
      .flag();

    parser
      .addArguments("--no-ascii")
      .help("Disable ASCII art display.")
      .flag();

    parser
      .addArguments("--json")
      .help("Output system information in JSON format. Overrides --no-ascii.")
      .flag();

    parser
      .addArguments("--pretty")
      .help("Pretty-print JSON output. Only valid when --json is used.")
      .flag();

    if (Result result = parser.parseArgs({ argv, static_cast<usize>(argc) }); !result) {
      error_at(result.error());
      return EXIT_FAILURE;
    }

    doctorMode     = parser.get<bool>("-d") || parser.get<bool>("--doctor");
    clearCache     = parser.get<bool>("--clear-cache");
    ignoreCacheRun = parser.get<bool>("--ignore-cache");
    noAscii        = parser.get<bool>("--no-ascii");
    jsonOutput     = parser.get<bool>("--json");
    prettyJson     = parser.get<bool>("--pretty");
    language       = parser.get<String>("--lang");

    SetRuntimeLogLevel(
      parser.get<bool>("-V") || parser.get<bool>("--verbose")
        ? LogLevel::Debug
        : parser.getEnum<LogLevel>("--log-level")
    );
  }

  using draconis::utils::cache::CacheManager, draconis::utils::cache::CachePolicy;

  CacheManager cache;

  if (ignoreCacheRun)
    CacheManager::ignoreCache = true;

  cache.setGlobalPolicy(CachePolicy::tempDirectory());

  if (clearCache) {
    const u8 removedCount = cache.invalidateAll(true);

    if (removedCount > 0)
      Println("Removed {} files.", removedCount);
    else
      Println("No cache files were found to clear.");

    return EXIT_SUCCESS;
  }

#ifndef NDEBUG
  if (Result<CPUCores> cpuCores = GetCPUCores(cache))
    debug_log("CPU cores: {} physical, {} logical", cpuCores->physical, cpuCores->logical);
  else
    debug_at(cpuCores.error());

  if (Result<NetworkInterface> networkInterface = GetPrimaryNetworkInterface(cache)) {
    debug_log("Network interface: {}", networkInterface->name);
    debug_log("Network interface IPv4 address: {}", networkInterface->ipv4Address.value_or("N/A"));
    debug_log("Network interface MAC address: {}", networkInterface->macAddress.value_or("N/A"));
    debug_log("Network interface is up: {}", networkInterface->isUp);
    debug_log("Network interface is loopback: {}", networkInterface->isLoopback);
  } else
    debug_at(networkInterface.error());

  if (Result<Battery> battery = GetBatteryInfo(cache)) {
    debug_log("Battery status: {}", magic_enum::enum_name(battery->status));

    debug_log("Battery percentage: {}%", battery->percentage.value_or(0));

    if (battery->timeRemaining.has_value())
      debug_log("Battery time remaining: {}", SecondsToFormattedDuration(battery->timeRemaining.value()));
    else
      debug_log("Battery time remaining: N/A");
  } else
    debug_at(battery.error());

  if (Result<DisplayInfo> primaryOutput = GetPrimaryOutput(cache)) {
    debug_log("Primary display ID: {}", primaryOutput->id);
    debug_log("Primary display resolution: {}x{}", primaryOutput->resolution.width, primaryOutput->resolution.height);
    debug_log("Primary display refresh rate: {:.2f}Hz", primaryOutput->refreshRate);
    debug_log("Primary display is primary: {}", primaryOutput->isPrimary);
  } else
    debug_at(primaryOutput.error());
#endif

  {
    const Config& config = Config::getInstance();

    // Initialize translation manager with language from command line or config
    if (language.empty() && config.general.language)
      language = *config.general.language;

    // Initialize translation manager (this will auto-detect system language)
    TranslationManager& translationManager = GetTranslationManager();

    if (!language.empty())
      translationManager.setLanguage(language);

    debug_log("Current language: {}", translationManager.getCurrentLanguage());
    debug_log("Selected language: {}", language.empty() ? "auto" : language);

    SystemInfo data(cache, config);

#if DRAC_ENABLE_WEATHER
    using enum draconis::utils::error::DracErrorCode;

    Result<Report> weatherReport;

    if (config.weather.enabled && config.weather.service == nullptr)
      weatherReport = Err({ Other, "Weather service is not configured" });
    else if (config.weather.enabled)
      weatherReport = config.weather.service->getWeatherInfo();
    else
      weatherReport = Err({ ApiUnavailable, "Weather is disabled" });
#endif

    if (doctorMode) {
      PrintDoctorReport(
#if DRAC_ENABLE_WEATHER
        weatherReport,
#endif
        data
      );

      return EXIT_SUCCESS;
    }

    if (jsonOutput)
      PrintJsonOutput(
#if DRAC_ENABLE_WEATHER
        weatherReport,
#endif
        data,
        prettyJson
      );
    else
      WriteToConsole(CreateUI(
        config,
        data,
#if DRAC_ENABLE_WEATHER
        weatherReport,
#endif
        noAscii
      ));
  }

  return EXIT_SUCCESS;
} catch (const Exception& e) {
  error_at(e);
  return EXIT_FAILURE;
}
