#include <ctime>
#include <fmt/chrono.h>
#include <fmt/color.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <string>

#include "config/config.h"
#include "os/os.h"

struct BytesToGiB {
  u64 value;
};

// 1024^3 (size of 1 GiB)
constexpr u64 GIB = 1'073'741'824;

template <>
struct fmt::formatter<BytesToGiB> : fmt::formatter<double> {
  template <typename FmtCtx>
  constexpr auto format(const BytesToGiB& BTG, FmtCtx& ctx) const -> typename FmtCtx::iterator {
    auto out = fmt::formatter<double>::format(static_cast<double>(BTG.value) / GIB, ctx);
    *out++   = 'G';
    *out++   = 'i';
    *out++   = 'B';
    return out;
  }
};

namespace {
  fn GetDate() -> std::string {
    // Get current local time
    std::time_t now       = std::time(nullptr);
    std::tm     localTime = *std::localtime(&now);

    // Format the date using fmt::format
    std::string date = fmt::format("{:%e}", localTime);

    // Remove leading whitespace
    if (!date.empty() && std::isspace(date.front()))
      date.erase(date.begin());

    // Append appropriate suffix for the date
    if (date.ends_with("1") && date != "11")
      date += "st";
    else if (date.ends_with("2") && date != "12")
      date += "nd";
    else if (date.ends_with("3") && date != "13")
      date += "rd";
    else
      date += "th";

    return fmt::format("{:%B} {}", localTime, date);
  }

  using namespace ftxui;

  fn CreateColorCircles() -> Element {
    Elements circles;

    for (int i = 0; i < 16; ++i)
      circles.push_back(hbox({
        text("◯") | bold | color(Color::Palette256(i)),
        text(" "),
      }));

    return hbox(circles);
  }

  fn SystemInfoBox(const Config& config) -> Element {
    // Fetch data
    const std::string& name               = config.general.get().name.get();
    const std::string& date               = GetDate();
    const Weather      weather            = config.weather.get();
    const std::string& host               = GetHost();
    const std::string& kernelVersion      = GetKernelVersion();
    const std::string& osVersion          = GetOSVersion();
    const u64          memInfo            = GetMemInfo();
    const std::string& desktopEnvironment = GetDesktopEnvironment();
    const std::string& windowManager      = GetWindowManager();
    const bool         nowPlayingEnabled  = config.now_playing.get().enabled;
    const std::string& nowPlaying         = nowPlayingEnabled ? GetNowPlaying() : "";

    // Icon constants (using Nerd Font v3)
    constexpr const char*  calendarIcon = "   ";
    constexpr const char*  hostIcon     = " 󰌢  ";
    constexpr const char*  kernelIcon   = "   ";
    constexpr const char*  osIcon       = "   ";
    constexpr const char*  memoryIcon   = "   ";
    constexpr const char*  weatherIcon  = "   ";
    constexpr const char*  musicIcon    = "   ";
    const Color::Palette16 labelColor   = Color::Yellow;
    const Color::Palette16 valueColor   = Color::White;
    const Color::Palette16 borderColor  = Color::GrayLight;
    const Color::Palette16 iconColor    = Color::Cyan;

    Elements content;

    content.push_back(text("   Hello " + name + "! ") | bold | color(Color::Cyan));
    content.push_back(separator() | color(borderColor));
    content.push_back(hbox({
      text("   ") | color(iconColor), // Palette icon
      CreateColorCircles(),
    }));
    content.push_back(separator() | color(borderColor));

    // Helper function for aligned rows
    auto createRow = [&](const std::string& icon, const std::string& label, const std::string& value) {
      return hbox({
        text(icon) | color(iconColor),
        text(label) | color(labelColor),
        text(" "),
        filler(),
        text(value) | color(valueColor),
        text(" "),
      });
    };

    // System info rows
    content.push_back(createRow(calendarIcon, "Date", date));

    // Weather row
    if (weather.enabled) {
      WeatherOutput weatherInfo = weather.getWeatherInfo();

      if (weather.show_town_name)
        content.push_back(hbox({
          text(weatherIcon) | color(iconColor),
          text("Weather") | color(labelColor),
          filler(),

          hbox({
            text(fmt::format("{}°F ", std::lround(weatherInfo.main.temp))),
            text("in "),
            text(weatherInfo.name),
            text(" "),
          }) |
            color(valueColor),
        }));
      else
        content.push_back(hbox({
          text(weatherIcon) | color(iconColor),
          text("Weather") | color(labelColor),
          filler(),

          hbox({
            text(fmt::format("{}°F, {}", std::lround(weatherInfo.main.temp), weatherInfo.weather[0].description)),
            text(" "),
          }) |
            color(valueColor),
        }));
    }

    content.push_back(separator() | color(borderColor));

    if (!host.empty())
      content.push_back(createRow(hostIcon, "Host", host));

    if (!kernelVersion.empty())
      content.push_back(createRow(kernelIcon, "Kernel", kernelVersion));

    if (!osVersion.empty())
      content.push_back(createRow(osIcon, "OS", osVersion));

    if (memInfo > 0)
      content.push_back(createRow(memoryIcon, "RAM", fmt::format("{:.2f}", BytesToGiB { memInfo })));

    content.push_back(separator() | color(borderColor));

    if (!desktopEnvironment.empty() && desktopEnvironment != windowManager)
      content.push_back(createRow(" 󰇄  ", "DE", desktopEnvironment));

    if (!windowManager.empty())
      content.push_back(createRow("   ", "WM", windowManager));

    // Now Playing row
    if (nowPlayingEnabled && !nowPlaying.empty()) {
      content.push_back(separator() | color(borderColor));
      content.push_back(hbox({
        text(musicIcon) | color(iconColor),
        text("Music") | color(labelColor),
        text(" "),
        filler(),
        text(nowPlaying.length() > 30 ? nowPlaying.substr(0, 30) + "..." : nowPlaying) | color(Color::Magenta),
        text(" "),
      }));
    }

    return vbox(content) | borderRounded | color(Color::White);
  }
}

fn main() -> i32 {
  const Config& config = Config::getInstance();

  Element document = hbox({ SystemInfoBox(config), filler() });

  Screen screen = Screen::Create(Dimension::Full(), Dimension::Fit(document));
  Render(screen, document);
  screen.Print();

  return 0;
}
