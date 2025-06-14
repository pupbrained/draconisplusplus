#include <DracUtils/Error.hpp>
#include <argparse.hpp>            // argparse::ArgumentParser
#include <cstdlib>                 // EXIT_FAILURE, EXIT_SUCCESS
#include <ftxui/dom/elements.hpp>  // ftxui::{Element, hbox, vbox, text, separator, filler, etc.}
#include <ftxui/dom/node.hpp>      // ftxui::{Render}
#include <ftxui/screen/screen.hpp> // ftxui::{Screen, Dimension::Full}
#include <matchit.hpp>             // matchit::{match, is, _}

#ifdef __cpp_lib_print
  #include <print> // std::print
#else
  #include <iostream> // std::cout
#endif

#include <Drac++/Core/System.hpp>
#include <DracUtils/Definitions.hpp>
#include <DracUtils/Logging.hpp>
#include <DracUtils/Types.hpp>

#include "Config/Config.hpp"

#include "UI/UI.hpp"

using namespace util::types;
using util::error::DracError;

namespace {
  fn PrintDoctorReport(const os::System& data) -> void {
    Vec<Pair<String, DracError>> failures;

    constexpr u8 totalPossibleReadouts = 9
#if DRAC_ENABLE_PACKAGECOUNT
      + 1
#endif
#if DRAC_ENABLE_NOWPLAYING
      + 1
#endif
#if DRAC_ENABLE_WEATHER
      + 1
#endif
      ;

    failures.reserve(totalPossibleReadouts);

    if (!data.date.has_value())
      failures.emplace_back("Date", data.date.error());
    if (!data.host.has_value())
      failures.emplace_back("Host", data.host.error());
    if (!data.kernelVersion.has_value())
      failures.emplace_back("KernelVersion", data.kernelVersion.error());
    if (!data.osVersion.has_value())
      failures.emplace_back("OSVersion", data.osVersion.error());
    if (!data.memInfo.has_value())
      failures.emplace_back("MemoryInfo", data.memInfo.error());
    if (!data.desktopEnv.has_value())
      failures.emplace_back("DesktopEnvironment", data.desktopEnv.error());
    if (!data.windowMgr.has_value())
      failures.emplace_back("WindowManager", data.windowMgr.error());
    if (!data.diskUsage.has_value())
      failures.emplace_back("DiskUsage", data.diskUsage.error());
    if (!data.shell.has_value())
      failures.emplace_back("Shell", data.shell.error());
#if DRAC_ENABLE_PACKAGECOUNT
    if (!data.packageCount.has_value())
      failures.emplace_back("PackageCount", data.packageCount.error());
#endif
#if DRAC_ENABLE_NOWPLAYING
    if (!data.nowPlaying.has_value())
      failures.emplace_back("NowPlaying", data.nowPlaying.error());
#endif
#if DRAC_ENABLE_WEATHER
    if (!data.weather.has_value())
      failures.emplace_back("Weather", data.weather.error());
#endif

#ifdef __cpp_lib_print
    std::println("We've collected a total of {} readouts including {} failed read{}.\n", totalPossibleReadouts, failures.size(), failures.size() == 1 ? "" : "s");

    for (const auto& [readout, err] : failures)
      std::println("Readout \"{}\" failed: {} (code: {})", readout, err.message, err.code);
#else
    std::cout << std::format("We've collected a total of {} readouts including {} failed read{}.\n\n", totalPossibleReadouts, failures.size(), failures.size() == 1 ? "" : "s");

    for (const auto& fail : failures)
      std::cout << std::format("Readout \"{}\" failed: {} (code: {})\n", fail.first, fail.second.message, fail.second.code);

    std::cout << std::flush;
#endif
  }

  fn InitializeSystem(const Config& config) -> os::System {
    using enum std::launch;
    using enum util::error::DracErrorCode;

    os::System system;

    // Use batch operations for related information
    Future<Result<SZString>>      osFut     = std::async(async, &os::System::getOSVersion);
    Future<Result<SZString>>      kernelFut = std::async(async, &os::System::getKernelVersion);
    Future<Result<SZString>>      hostFut   = std::async(async, &os::System::getHost);
    Future<Result<SZString>>      cpuFut    = std::async(async, &os::System::getCPUModel);
    Future<Result<SZString>>      gpuFut    = std::async(async, &os::System::getGPUModel);
    Future<Result<SZString>>      deFut     = std::async(async, &os::System::getDesktopEnvironment);
    Future<Result<SZString>>      wmFut     = std::async(async, &os::System::getWindowManager);
    Future<Result<SZString>>      shellFut  = std::async(async, &os::System::getShell);
    Future<Result<ResourceUsage>> memFut    = std::async(async, &os::System::getMemInfo);
    Future<Result<ResourceUsage>> diskFut   = std::async(async, &os::System::getDiskUsage);
    Future<Result<SZString>>      dateFut   = std::async(async, &os::System::getDate);

#if DRAC_ENABLE_PACKAGECOUNT
    Future<Result<u64>> pkgFut = std::async(async, package::GetTotalCount, config.enabledPackageManagers);
#endif

#if DRAC_ENABLE_NOWPLAYING
    Future<Result<MediaInfo>> npFut = std::async(config.nowPlaying.enabled ? async : deferred, &os::System::getNowPlaying);
#endif

#if DRAC_ENABLE_WEATHER
    Future<Result<weather::WeatherReport>> wthrFut = std::async(
      config.weather.enabled ? std::launch::async : std::launch::deferred,
      [&service = config.weather.service]() { return service->getWeatherInfo(); }
    );
#endif

    system.osVersion     = osFut.get();
    system.kernelVersion = kernelFut.get();
    system.host          = hostFut.get();
    system.cpuModel      = cpuFut.get();
    system.gpuModel      = gpuFut.get();
    system.desktopEnv    = deFut.get();
    system.windowMgr     = wmFut.get();
    system.shell         = shellFut.get();
    system.memInfo       = memFut.get();
    system.diskUsage     = diskFut.get();
    system.date          = dateFut.get();

#if DRAC_ENABLE_PACKAGECOUNT
    system.packageCount = pkgFut.get();
#endif

#if DRAC_ENABLE_WEATHER
    system.weather = config.weather.enabled ? wthrFut.get() : Err(DracError(ApiUnavailable, "Weather API disabled"));
#endif

#if DRAC_ENABLE_NOWPLAYING
    system.nowPlaying = config.nowPlaying.enabled ? npFut.get() : Err(DracError(ApiUnavailable, "Now Playing API disabled"));
#endif

    return system;
  }
} // namespace

fn main(const i32 argc, char* argv[]) -> i32 try {
#ifdef _WIN32
  winrt::init_apartment();
#endif

  bool doctorMode = false;

  {
    using argparse::ArgumentParser;

    ArgumentParser parser("draconis", DRACONISPLUSPLUS_VERSION);

    parser
      .add_argument("--log-level")
      .help("Set the log level")
      .default_value("info")
      .choices("debug", "info", "warn", "error");

    parser
      .add_argument("-V", "--verbose")
      .help("Enable verbose logging. Overrides --log-level.")
      .flag();

    parser
      .add_argument("-d", "--doctor")
      .help("Reports any failed readouts and their error messages.")
      .flag();

    if (Result result = parser.parse_args(argc, argv); !result) {
      error_at(result.error());
      return EXIT_FAILURE;
    }

    doctorMode = parser.get<bool>("-d").value_or(false) || parser.get<bool>("--doctor").value_or(false);

    {
      using matchit::match, matchit::is, matchit::_;
      using util::logging::LogLevel;
      using enum util::logging::LogLevel;

      const bool     verbose     = parser.get<bool>("-V").value_or(false) || parser.get<bool>("--verbose").value_or(false);
      Result<String> logLevelStr = verbose ? "debug" : parser.get<String>("--log-level");

      const LogLevel minLevel = match(logLevelStr)(
        is | "debug" = Debug,
        is | "info"  = Info,
        is | "warn"  = Warn,
        is | "error" = Error,
#ifndef NDEBUG
        is | _ = Debug
#else
        is | _ = Info
#endif
      );

      SetRuntimeLogLevel(minLevel);
    }
  }

  {
    using namespace ftxui;
    using namespace ftxui::Dimension;
    using os::System;
    using ui::CreateUI;

    const Config& config = Config::getInstance();
    const System  data   = InitializeSystem(config);

    if (doctorMode) {
      PrintDoctorReport(data);
      return EXIT_SUCCESS;
    }

    Element document = CreateUI(config, data);

    Screen screen = Screen::Create(Full(), Fit(document));
    Render(screen, document);
    screen.Print();
  }

  // Running the program as part of the shell's startup will cut
  // off the last line of output, so we need to add a newline here.
#ifdef __cpp_lib_print
  std::println();
#else
  std::cout << '\n';
#endif
  return EXIT_SUCCESS;
} catch (const DracError& e) {
  error_at(e);
  return EXIT_FAILURE;
} catch (const Exception& e) {
  error_at(e);
  return EXIT_FAILURE;
}
