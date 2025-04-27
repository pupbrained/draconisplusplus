#include <chrono>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/screen.hpp>
#include <future>
#include <string>
#include <variant>

#include "config/config.h"
#include "core/system_data.h"
#include "os/os.h"

namespace ui {
  using ftxui::Color;

  static constexpr inline bool SHOW_ICONS           = true;
  static constexpr i32         MAX_PARAGRAPH_LENGTH = 30;

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
    StringView desktop;
    StringView window_manager;
  };

  static constexpr Icons EMPTY_ICONS = {
    .user           = "",
    .palette        = "",
    .calendar       = "",
    .host           = "",
    .kernel         = "",
    .os             = "",
    .memory         = "",
    .weather        = "",
    .music          = "",
    .disk           = "",
    .shell          = "",
    .desktop        = "",
    .window_manager = "",
  };

  static constexpr Icons NERD_ICONS = {
    .user           = "   ",
    .palette        = "   ",
    .calendar       = "   ",
    .host           = " 󰌢  ",
    .kernel         = "   ",
    .os             = "   ",
    .memory         = "   ",
    .weather        = "   ",
    .music          = "   ",
    .disk           = " 󰋊  ",
    .shell          = "   ",
    .desktop        = " 󰇄  ",
    .window_manager = "   ",
  };
} // namespace ui

namespace {
  using namespace ftxui;

  fn CreateColorCircles() -> Element {
    return hbox(
      std::views::iota(0, 16) | std::views::transform([](i32 colorIndex) {
        return hbox({ text("◯") | bold | color(static_cast<Color::Palette256>(colorIndex)), text(" ") });
      }) |
      std::ranges::to<Elements>()
    );
  }

  fn SystemInfoBox(const Config& config, const SystemData& data) -> Element {
    const String& name    = config.general.name;
    const Weather weather = config.weather;

    const auto& [userIcon, paletteIcon, calendarIcon, hostIcon, kernelIcon, osIcon, memoryIcon, weatherIcon, musicIcon, diskIcon, shellIcon, deIcon, wmIcon] =
      ui::SHOW_ICONS ? ui::NERD_ICONS : ui::EMPTY_ICONS;

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
    if (weather.enabled && data.weather_info.has_value()) {
      const WeatherOutput& weatherInfo = data.weather_info.value();

      if (weather.show_town_name)
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
    }

    content.push_back(separator() | color(ui::DEFAULT_THEME.border));

    if (data.host)
      content.push_back(createRow(hostIcon, "Host", *data.host));
    else
      ERROR_LOG_LOC(data.host.error());

    if (data.kernel_version)
      content.push_back(createRow(kernelIcon, "Kernel", *data.kernel_version));
    else
      ERROR_LOG_LOC(data.kernel_version.error());

    if (data.os_version)
      content.push_back(createRow(String(osIcon), "OS", *data.os_version));
    else
      ERROR_LOG_LOC(data.os_version.error());

    if (data.mem_info)
      content.push_back(createRow(memoryIcon, "RAM", std::format("{}", BytesToGiB { *data.mem_info })));
    else
      ERROR_LOG_LOC(data.mem_info.error());

    if (data.disk_usage)
      content.push_back(createRow(
        diskIcon,
        "Disk",
        std::format("{}/{}", BytesToGiB { data.disk_usage->used_bytes }, BytesToGiB { data.disk_usage->total_bytes })
      ));
    else
      ERROR_LOG_LOC(data.disk_usage.error());

    if (data.shell)
      content.push_back(createRow(shellIcon, "Shell", *data.shell));

    content.push_back(separator() | color(ui::DEFAULT_THEME.border));

    if (data.desktop_environment && *data.desktop_environment != data.window_manager)
      content.push_back(createRow(deIcon, "DE", *data.desktop_environment));

    if (data.window_manager)
      content.push_back(createRow(wmIcon, "WM", *data.window_manager));

    if (config.now_playing.enabled && data.now_playing) {
      if (const Result<MediaInfo, NowPlayingError>& nowPlayingResult = *data.now_playing) {
        const MediaInfo& info = *nowPlayingResult;

        const String title  = info.title.value_or("Unknown Title");
        const String artist = info.artist.value_or("Unknown Artist");
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
      } else
        DEBUG_LOG_LOC(nowPlayingResult.error());
    }

    return vbox(content) | borderRounded | color(Color::White);
  }
} // namespace

fn main() -> i32 {
  const Config&    config = Config::getInstance();
  const SystemData data   = SystemData::fetchSystemData(config);

  Element document = vbox({ hbox({ SystemInfoBox(config, data), filler() }), text("") });

  Screen screen = Screen::Create(Dimension::Full(), Dimension::Fit(document));
  Render(screen, document);
  screen.Print();

  return 0;
}
