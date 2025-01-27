#include <ctime>
#include <fmt/chrono.h>
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
    if (date == "1" || date == "21" || date == "31")
      date += "st";
    else if (date == "2" || date == "22")
      date += "nd";
    else if (date == "3" || date == "23")
      date += "rd";
    else
      date += "th";

    return fmt::format("{:%B} {}", localTime, date);
  }

  using namespace ftxui;

  fn CreateColorCircles() -> Element {
    Elements circles;
    for (int i = 0; i < 16; ++i) {
      circles.push_back(text("◍") | color(Color::Palette256(i)));
      circles.push_back(text(" "));
    }
    return hbox(circles);
  }

  fn SystemInfoBox(const Config& config) -> Element {
    // Fetch data
    std::string        date              = GetDate();
    u64                memInfo           = GetMemInfo();
    std::string        osVersion         = GetOSVersion();
    Weather            weather           = config.weather.get();
    bool               nowPlayingEnabled = config.now_playing.get().enabled;
    std::string        nowPlaying        = nowPlayingEnabled ? GetNowPlaying() : "";
    const std::string& name              = config.general.get().name.get();

    // Icon constants (using Nerd Font v3)
    constexpr const char* calendarIcon = "   ";
    constexpr const char* memoryIcon   = "   ";
    constexpr const char* osIcon       = "   ";
    constexpr const char* weatherIcon  = " 󰖐  ";
    constexpr const char* musicIcon    = "   ";
    const auto            labelColor   = Color::Yellow;
    const auto            valueColor   = Color::White;
    const auto            borderColor  = Color::GrayLight;
    const auto            iconColor    = Color::RGB(100, 200, 255); // Bright cyan

    Elements content;
    content.push_back(text("   Hello " + name + "! ") | bold | color(Color::Cyan));
    content.push_back(separator() | color(borderColor));

    // Helper function for aligned rows
    auto createRow =
      [&](const std::string& emoji, const std::string& label, const std::string& value) {
        return hbox({ text(emoji),
                      text(label) | color(labelColor),
                      filler(),
                      text(value),
                      text(" ") | color(valueColor) });
      };

    // System info rows
    content.push_back(createRow(calendarIcon, "Date ", date));
    content.push_back(createRow(memoryIcon, "RAM ", fmt::format("{:.2f}", BytesToGiB { memInfo })));
    content.push_back(createRow(osIcon, "OS ", osVersion));

    // Weather row
    if (weather.enabled) {
      auto weatherInfo = weather.getWeatherInfo();
      content.push_back(separator() | color(borderColor));
      content.push_back(hbox(
        { text(weatherIcon),
          text("Weather ") | color(labelColor),
          filler(),
          hbox({ text(fmt::format("{}°F ", std::lround(weatherInfo.main.temp))) | color(Color::Red),
                 text("in "),
                 text(weatherInfo.name),
                 text(" ") }) |
            color(valueColor) }
      ));
    }

    // Now Playing row
    if (nowPlayingEnabled) {
      content.push_back(separator() | color(borderColor));
      content.push_back(hbox({ text(musicIcon),
                               text("Now Playing ") | color(labelColor),
                               filler(),
                               text(!nowPlaying.empty() ? nowPlaying : "No song playing"),
                               text(" ") | color(Color::Magenta) }));
    }

    // Color circles section
    content.push_back(filler());
    content.push_back(separator() | color(borderColor));
    content.push_back(hbox({ text("   ") | color(iconColor), // Palette icon
                             CreateColorCircles() }));
    return vbox(content) | borderRounded | color(Color::White);
  }
}

fn main() -> i32 {
  const Config& config = Config::getInstance();

  auto document = hbox({ SystemInfoBox(config), filler() });

  auto screen = Screen::Create(Dimension::Full(), Dimension::Fit(document));
  Render(screen, document);
  screen.Print();

  return 0;
}
