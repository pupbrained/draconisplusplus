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
#include <Drac++/Services/Packages.hpp>
#include <Drac++/Services/Weather.hpp>

#include <DracUtils/Definitions.hpp>
#include <DracUtils/Error.hpp>
#include <DracUtils/Logging.hpp>
#include <DracUtils/Types.hpp>

#include "Config/Config.hpp"
#include "UI/UI.hpp"

using namespace draconis::utils::types;
using namespace draconis::core::system;
using namespace draconis::config;
using namespace draconis::ui;

using draconis::utils::error::DracError;
using enum draconis::utils::error::DracErrorCode;

using draconis::services::weather::Report;

namespace {
  fn getOrdinalSuffix(const i32 day) -> CStr {
    using matchit::match, matchit::is, matchit::_, matchit::in;

    return match(day)(
      is | in(11, 13)    = "th",
      is | (_ % 10 == 1) = "st",
      is | (_ % 10 == 2) = "nd",
      is | (_ % 10 == 3) = "rd",
      is | _             = "th"
    );
  }

  fn getDate() -> Result<String> {
    using std::chrono::system_clock;

    const system_clock::time_point nowTp = system_clock::now();
    const std::time_t              nowTt = system_clock::to_time_t(nowTp);

    std::tm nowTm;

#ifdef _WIN32
    if (localtime_s(&nowTm, &nowTt) == 0) {
#else
    if (localtime_r(&nowTt, &nowTm) != nullptr) {
#endif
      i32 day = nowTm.tm_mday;

      String monthBuffer(32, '\0');

      if (const usize monthLen = std::strftime(monthBuffer.data(), monthBuffer.size(), "%B", &nowTm); monthLen > 0) {
        monthBuffer.resize(monthLen);

        CStr suffix = getOrdinalSuffix(day);

        return std::format("{} {}{}", monthBuffer, day, suffix);
      }

      return Err(DracError(ParseError, "Failed to format date"));
    }

    return Err(DracError(ParseError, "Failed to get local time"));
  }

#if DRAC_ENABLE_WEATHER
  fn PrintDoctorReport(const System& data, const Result<Report>& weather) -> void {
#else
  fn PrintDoctorReport(const System& data) -> void {
#endif
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
    if (!weather.has_value())
      failures.emplace_back("Weather", weather.error());
#endif

    const String summary = std::format(
      "We've collected a total of {} readouts including {} failed read{}.\n\n",
      totalPossibleReadouts,
      failures.size(),
      failures.size() == 1 ? "" : "s"
    );

#ifdef __cpp_lib_print
    std::println("{}", summary);
#else
    std::cout << summary;
#endif

    for (const auto& [readout, err] : failures) {
      const String failureLine = std::format(
        "Readout \"{}\" failed: {} (code: {})\n",
        readout,
        err.message,
        err.code
      );

#ifdef __cpp_lib_print
      std::println("{}", failureLine);
#else
      std::cout << failureLine;
#endif
    }

#ifndef __cpp_lib_print
    std::cout << std::flush;
#endif
  }

  fn InitializeSystem(const draconis::config::Config& config) -> System {
    using enum std::launch;

    System system;

    // Use batch operations for related information
    Future<Result<String>>        osFut     = std::async(async, &System::getOSVersion);
    Future<Result<String>>        kernelFut = std::async(async, &System::getKernelVersion);
    Future<Result<String>>        hostFut   = std::async(async, &System::getHost);
    Future<Result<String>>        cpuFut    = std::async(async, &System::getCPUModel);
    Future<Result<String>>        gpuFut    = std::async(async, &System::getGPUModel);
    Future<Result<String>>        deFut     = std::async(async, &System::getDesktopEnvironment);
    Future<Result<String>>        wmFut     = std::async(async, &System::getWindowManager);
    Future<Result<String>>        shellFut  = std::async(async, &System::getShell);
    Future<Result<ResourceUsage>> memFut    = std::async(async, &System::getMemInfo);
    Future<Result<ResourceUsage>> diskFut   = std::async(async, &System::getDiskUsage);
    Future<Result<String>>        dateFut   = std::async(async, &getDate);

#if DRAC_ENABLE_PACKAGECOUNT
    Future<Result<u64>> pkgFut = std::async(async, draconis::services::packages::GetTotalCount, config.enabledPackageManagers);
#endif

#if DRAC_ENABLE_NOWPLAYING
    Future<Result<MediaInfo>> npFut = std::async(config.nowPlaying.enabled ? async : deferred, &System::getNowPlaying);
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

    ArgumentParser parser("draconis", DRAC_VERSION);

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
      using draconis::utils::logging::LogLevel;
      using matchit::match, matchit::is, matchit::_;
      using enum draconis::utils::logging::LogLevel;

      const bool     verbose     = parser.get<bool>("-V").value_or(false) || parser.get<bool>("--verbose").value_or(false);
      Result<String> logLevelStr = verbose ? "debug" : parser.get<String>("--log-level");

      const LogLevel minLevel = match(logLevelStr)(
        is | "debug" = Debug,
        is | "info"  = Info,
        is | "warn"  = Warn,
        is | "error" = Error
      );

      SetRuntimeLogLevel(minLevel);
    }
  }

  {
    using namespace ftxui;
    using namespace ftxui::Dimension;
    using matchit::match, matchit::is, matchit::_;

    const Config&  config        = Config::getInstance();
    const System   data          = InitializeSystem(config);
    Result<Report> weatherReport = config.weather.service->getWeatherInfo();

    if (doctorMode) {
#if DRAC_ENABLE_WEATHER
      PrintDoctorReport(data, weatherReport);
#else
      PrintDoctorReport(data);
#endif
      return EXIT_SUCCESS;
    }

    Element document;

    if (weatherReport) {
      document = CreateUI(config, data, *weatherReport);
    } else {
      error_at(weatherReport.error());
      document = CreateUI(config, data, None);
    }

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
} catch (const Exception& e) {
  error_at(e);
  return EXIT_FAILURE;
}
