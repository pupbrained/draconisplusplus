#include <argparse.hpp>            // argparse::ArgumentParser
#include <cstdlib>                 // EXIT_FAILURE, EXIT_SUCCESS
#include <ftxui/dom/elements.hpp>  // ftxui::{Element, hbox, vbox, text, separator, filler, etc.}
#include <ftxui/dom/node.hpp>      // ftxui::{Render}
#include <ftxui/screen/screen.hpp> // ftxui::{Screen, Dimension::Full}
#include <matchit.hpp>
#include <winrt/base.h>

#ifdef __cpp_lib_print
  #include <print> // std::print
#else
  #include <iostream> // std::cout
#endif

#include "Config/Config.hpp"

#include "Core/System.hpp"

#include "UI/UI.hpp"

#include "Util/Definitions.hpp"
#include "Util/Logging.hpp"
#include "Util/Types.hpp"

using util::types::i32, util::types::Exception;

namespace {
  fn PrintDoctorReport(const os::System& data) -> void {
    using util::types::u8, util::types::Vec, util::types::Pair;

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
    using ftxui::Element, ftxui::Screen, ftxui::Render;
    using ftxui::Dimension::Full, ftxui::Dimension::Fit;
    using os::System;
    using ui::CreateUI;

    const Config& config = Config::getInstance();
    const System  data   = System(config);

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
