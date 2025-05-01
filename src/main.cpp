#include <cmath>                   // std::lround
#include <format>                  // std::format
#include <ftxui/dom/elements.hpp>  // ftxui::{Element, hbox, vbox, text, separator, filler, etc.}
#include <ftxui/dom/node.hpp>      // ftxui::{Render}
#include <ftxui/screen/color.hpp>  // ftxui::Color
#include <ftxui/screen/screen.hpp> // ftxui::{Screen, Dimension::Full}
#include <ranges>                  // std::ranges::{iota, to, transform}

#include "src/config/config.hpp"
#include "src/config/weather.hpp"
#include "src/core/system_data.hpp"
#include "src/core/util/defs.hpp"
#include "src/core/util/logging.hpp"
#include "src/core/util/types.hpp"

namespace ui {
  using ftxui::Color;
  using util::types::StringView, util::types::i32;

  static constexpr i32 MAX_PARAGRAPH_LENGTH = 30;

  // Color themes
  struct Theme {
    Color::Palette16 icon;
    Color::Palette16 label;
    Color::Palette16 value;
    Color::Palette16 border;
  };

  static constexpr Theme DEFAULT_THEME = {
    .icon   = Color::Cyan,
    .label  = Color::Yellow,
    .value  = Color::White,
    .border = Color::GrayLight,
  };

  struct Icons {
    StringView user;
    StringView palette;
    StringView calendar;
    StringView host;
    StringView kernel;
    StringView os;
    StringView memory;
    StringView weather;
    StringView music;
    StringView disk;
    StringView shell;
    StringView package;
    StringView desktop;
    StringView windowManager;
  };

  [[maybe_unused]] static constexpr Icons NONE = {
    .user          = "",
    .palette       = "",
    .calendar      = "",
    .host          = "",
    .kernel        = "",
    .os            = "",
    .memory        = "",
    .weather       = "",
    .music         = "",
    .disk          = "",
    .shell         = "",
    .package       = "",
    .desktop       = "",
    .windowManager = "",
  };

  [[maybe_unused]] static constexpr Icons NERD = {
    .user          = "   ",
    .palette       = "   ",
    .calendar      = "   ",
    .host          = " 󰌢  ",
    .kernel        = "   ",
    .os            = "   ",
    .memory        = "   ",
    .weather       = "   ",
    .music         = "   ",
    .disk          = " 󰋊  ",
    .shell         = "   ",
    .package       = " 󰏖  ",
    .desktop       = " 󰇄  ",
    .windowManager = "   ",
  };

  [[maybe_unused]] static constexpr Icons EMOJI = {
    .user          = " 👤 ",
    .palette       = " 🎨 ",
    .calendar      = " 📅 ",
    .host          = " 💻 ",
    .kernel        = " 🫀 ",
    .os            = " 🤖 ",
    .memory        = " 🧠 ",
    .weather       = " 🌈 ",
    .music         = " 🎵 ",
    .disk          = " 💾 ",
    .shell         = " 💲 ",
    .package       = " 📦 ",
    .desktop       = " 🖥️ ",
    .windowManager = " 🪟 ",
  };

  static constexpr inline Icons ICON_TYPE = NERD;
} // namespace ui

namespace {
  using namespace util::logging;
  using namespace ftxui;

  fn CreateColorCircles() -> Element {
    return hbox(
      std::views::iota(0, 16) | std::views::transform([](i32 colorIndex) {
        return hbox({ text("◯") | bold | color(static_cast<Color::Palette256>(colorIndex)), text(" ") });
      }) |
      std::ranges::to<Elements>()
    );
  }

  fn SystemInfoBox(const Config& config, const os::SystemData& data) -> Element {
    const String& name    = config.general.name;
    const Weather weather = config.weather;

    const auto& [userIcon, paletteIcon, calendarIcon, hostIcon, kernelIcon, osIcon, memoryIcon, weatherIcon, musicIcon, diskIcon, shellIcon, packageIcon, deIcon, wmIcon] =
      ui::ICON_TYPE;

    Elements content;

    content.push_back(text(String(userIcon) + "Hello " + name + "! ") | bold | color(Color::Cyan));
    content.push_back(separator() | color(ui::DEFAULT_THEME.border));
    content.push_back(hbox(
      {
        text(String(paletteIcon)) | color(ui::DEFAULT_THEME.icon),
        CreateColorCircles(),
      }
    ));
    content.push_back(separator() | color(ui::DEFAULT_THEME.border));

    // Helper function for aligned rows
    fn createRow = [&](const StringView& icon, const StringView& label, const StringView& value) { // NEW
      return hbox(
        {
          text(String(icon)) | color(ui::DEFAULT_THEME.icon),
          text(String(label)) | color(ui::DEFAULT_THEME.label),
          filler(),
          text(String(value)) | color(ui::DEFAULT_THEME.value),
          text(" "),
        }
      );
    };

    // System info rows
    content.push_back(createRow(calendarIcon, "Date", data.date));

    // Weather row
    if (weather.enabled && data.weather) {
      const weather::Output& weatherInfo = *data.weather;

      if (weather.showTownName)
        content.push_back(hbox(
          {
            text(String(weatherIcon)) | color(ui::DEFAULT_THEME.icon),
            text("Weather") | color(ui::DEFAULT_THEME.label),
            filler(),

            hbox(
              {
                text(std::format("{}°F ", std::lround(weatherInfo.main.temp))),
                text("in "),
                text(weatherInfo.name),
                text(" "),
              }
            ) |
              color(ui::DEFAULT_THEME.value),
          }
        ));
      else
        content.push_back(hbox(
          {
            text(String(weatherIcon)) | color(ui::DEFAULT_THEME.icon),
            text("Weather") | color(ui::DEFAULT_THEME.label),
            filler(),

            hbox(
              {
                text(std::format("{}°F, {}", std::lround(weatherInfo.main.temp), weatherInfo.weather[0].description)),
                text(" "),
              }
            ) |
              color(ui::DEFAULT_THEME.value),
          }
        ));
    } else if (weather.enabled)
      error_at(data.weather.error());

    content.push_back(separator() | color(ui::DEFAULT_THEME.border));

    if (data.host && !data.host->empty())
      content.push_back(createRow(hostIcon, "Host", *data.host));
    else
      error_at(data.host.error());

    if (data.kernelVersion)
      content.push_back(createRow(kernelIcon, "Kernel", *data.kernelVersion));
    else
      error_at(data.kernelVersion.error());

    if (data.osVersion)
      content.push_back(createRow(String(osIcon), "OS", *data.osVersion));
    else
      error_at(data.osVersion.error());

    if (data.memInfo)
      content.push_back(createRow(memoryIcon, "RAM", std::format("{}", BytesToGiB { *data.memInfo })));
    else
      error_at(data.memInfo.error());

    if (data.diskUsage)
      content.push_back(createRow(
        diskIcon,
        "Disk",
        std::format("{}/{}", BytesToGiB { data.diskUsage->used_bytes }, BytesToGiB { data.diskUsage->total_bytes })
      ));
    else
      error_at(data.diskUsage.error());

    if (data.shell)
      content.push_back(createRow(shellIcon, "Shell", *data.shell));
    else
      error_at(data.shell.error());

    if (data.packageCount)
      content.push_back(createRow(packageIcon, "Packages", std::format("{}", *data.packageCount)));
    else
      error_at(data.packageCount.error());

    content.push_back(separator() | color(ui::DEFAULT_THEME.border));

    if (data.desktopEnv && *data.desktopEnv != data.windowMgr)
      content.push_back(createRow(deIcon, "DE", *data.desktopEnv));

    if (data.windowMgr)
      content.push_back(createRow(wmIcon, "WM", *data.windowMgr));
    else
      error_at(data.windowMgr.error());

    if (config.nowPlaying.enabled && data.nowPlaying) {
      const String title  = data.nowPlaying->title.value_or("Unknown Title");
      const String artist = data.nowPlaying->artist.value_or("Unknown Artist");
      const String npText = artist + " - " + title;

      content.push_back(separator() | color(ui::DEFAULT_THEME.border));
      content.push_back(hbox(
        {
          text(String(musicIcon)) | color(ui::DEFAULT_THEME.icon),
          text("Playing") | color(ui::DEFAULT_THEME.label),
          text(" "),
          filler(),
          paragraph(npText) | color(Color::Magenta) | size(WIDTH, LESS_THAN, ui::MAX_PARAGRAPH_LENGTH),
          text(" "),
        }
      ));
    }

    return vbox(content) | borderRounded | color(Color::White);
  }
} // namespace

fn main() -> i32 {
  using os::SystemData;

#ifdef _WIN32
  winrt::init_apartment();
#endif

  const Config&    config = Config::getInstance();
  const SystemData data   = SystemData(config);

  Element document = vbox({ hbox({ SystemInfoBox(config, data), filler() }) });

  Screen screen = Screen::Create(Dimension::Full(), Dimension::Fit(document));
  Render(screen, document);
  screen.Print();

  std::println();

  return 0;
}
