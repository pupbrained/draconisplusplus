#include <format>                  // std::format
#include <ftxui/dom/elements.hpp>  // ftxui::{Element, hbox, vbox, text, separator, filler, etc.}
#include <ftxui/dom/node.hpp>      // ftxui::{Render}
#include <ftxui/screen/screen.hpp> // ftxui::{Screen, Dimension::Full}

#include "src/ui/ui.hpp"

#ifdef __cpp_lib_print
  #include <print> // std::print
#else
  #include <iostream> // std::cout
#endif

#include "src/config/config.hpp"
#include "src/core/system_data.hpp"
#include "src/util/defs.hpp"
#include "src/util/logging.hpp"
#include "src/util/types.hpp"

#include "include/argparse.hpp"

using util::types::i32;

fn main(const i32 argc, char* argv[]) -> i32 {
  using namespace ftxui;
  using os::SystemData;
  using util::types::Exception;

  // TODO: don't wrap all of this in a try-catch
  try {
#ifdef _WIN32
    winrt::init_apartment();
#endif

    argparse::ArgumentParser parser("draconis", "0.1.0");

    parser
      .add_argument("--log-level")
      .help("Set the log level")
      .default_value("info")
      .choices("trace", "debug", "info", "warn", "error", "fatal");

    parser
      .add_argument("-V", "--verbose")
      .help("Enable verbose logging. Alias for --log-level=debug")
      .flag();

    parser.parse_args(argc, argv);

    if (parser["--verbose"] == true || parser["-v"] == true)
      info_log("Verbose logging enabled");

    const Config&    config = Config::getInstance();
    const SystemData data   = SystemData(config);

    Element document = ui::CreateUI(config, data);

    Screen screen = Screen::Create(Dimension::Full(), Dimension::Fit(document));
    Render(screen, document);
    screen.Print();

#ifdef __cpp_lib_print
    std::println();
#else
    std::cout << '\n';
#endif
  } catch (const Exception& e) {
    error_log("Exception: {}", e.what());
    return 1;
  } catch (...) {
    error_log("Unknown exception");
    return 1;
  }

  return 0;
}
