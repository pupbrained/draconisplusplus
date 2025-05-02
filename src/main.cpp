#include <cmath>                   // std::lround
#include <format>                  // std::format
#include <ftxui/dom/elements.hpp>  // ftxui::{Element, hbox, vbox, text, separator, filler, etc.}
#include <ftxui/dom/node.hpp>      // ftxui::{Render}
#include <ftxui/screen/color.hpp>  // ftxui::Color
#include <ftxui/screen/screen.hpp> // ftxui::{Screen, Dimension::Full}
#include <ftxui/screen/string.hpp> // ftxui::string_width
#include <print>                   // std::println
#include <ranges>                  // std::ranges::{iota, to, transform}

#include "src/config/config.hpp"
#include "src/config/weather.hpp"
#include "src/core/system_data.hpp"
#include "src/util/defs.hpp"
#include "src/util/logging.hpp"
#include "src/util/types.hpp"

namespace ui {
  using ftxui::Color;
  using util::types::StringView, util::types::i32;

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

  struct RowInfo {
    StringView icon;
    StringView label;
    String     value;
  };

  fn CreateColorCircles() -> Element {
    return hbox(
      std::views::iota(0, 16) | std::views::transform([](i32 colorIndex) {
        return hbox({ text("◯") | bold | color(static_cast<Color::Palette256>(colorIndex)), text(" ") });
      }) |
      std::ranges::to<Elements>()
    );
  }

  fn get_visual_width(const String& str) -> usize { return ftxui::string_width(str); }
  fn get_visual_width_sv(const StringView& sview) -> usize { return ftxui::string_width(String(sview)); }

  fn find_max_label_len(const std::vector<RowInfo>& rows) -> usize {
    usize maxWidth = 0;
    for (const RowInfo& row : rows) maxWidth = std::max(maxWidth, get_visual_width_sv(row.label));

    return maxWidth;
  };

  fn SystemInfoBox(const Config& config, const os::SystemData& data) -> Element {
    const String&  name    = config.general.name;
    const Weather& weather = config.weather;

    const auto& [userIcon, paletteIcon, calendarIcon, hostIcon, kernelIcon, osIcon, memoryIcon, weatherIcon, musicIcon, diskIcon, shellIcon, packageIcon, deIcon, wmIcon] =
      ui::ICON_TYPE;

    std::vector<RowInfo> initialRows;    // Date, Weather
    std::vector<RowInfo> systemInfoRows; // Host, Kernel, OS, RAM, Disk, Shell, Packages
    std::vector<RowInfo> envInfoRows;    // DE, WM

    if (data.date)
      initialRows.push_back({ calendarIcon, "Date", *data.date });
    else
      debug_at(data.date.error());

    if (weather.enabled && data.weather) {
      const weather::Output& weatherInfo  = *data.weather;
      String                 weatherValue = weather.showTownName
                        ? std::format("{}°F in {}", std::lround(weatherInfo.main.temp), weatherInfo.name)
                        : std::format("{}°F, {}", std::lround(weatherInfo.main.temp), weatherInfo.weather[0].description);
      initialRows.push_back({ weatherIcon, "Weather", std::move(weatherValue) });
    } else if (weather.enabled)
      debug_at(data.weather.error());

    if (data.host && !data.host->empty())
      systemInfoRows.push_back({ hostIcon, "Host", *data.host });
    else
      debug_at(data.host.error());

    if (data.kernelVersion)
      systemInfoRows.push_back({ kernelIcon, "Kernel", *data.kernelVersion });
    else
      debug_at(data.kernelVersion.error());

    if (data.osVersion)
      systemInfoRows.push_back({ osIcon, "OS", *data.osVersion });
    else
      debug_at(data.osVersion.error());

    if (data.memInfo)
      systemInfoRows.push_back({ memoryIcon, "RAM", std::format("{}", BytesToGiB { *data.memInfo }) });
    else
      debug_at(data.memInfo.error());

    if (data.diskUsage)
      systemInfoRows.push_back(
        { diskIcon,
          "Disk",
          std::format("{}/{}", BytesToGiB { data.diskUsage->used_bytes }, BytesToGiB { data.diskUsage->total_bytes }) }
      );
    else
      debug_at(data.diskUsage.error());

    if (data.shell)
      systemInfoRows.push_back({ shellIcon, "Shell", *data.shell });
    else
      debug_at(data.shell.error());

    if (data.packageCount) {
      if (*data.packageCount > 0)
        systemInfoRows.push_back({ packageIcon, "Packages", std::format("{}", *data.packageCount) });
      else
        debug_log("Package count is 0, skipping");
    } else
      debug_at(data.packageCount.error());

    bool addedDe = false;
    if (data.desktopEnv && (!data.windowMgr || *data.desktopEnv != *data.windowMgr)) {
      envInfoRows.push_back({ deIcon, "DE", *data.desktopEnv });
      addedDe = true;
    } else if (!data.desktopEnv)
      debug_at(data.desktopEnv.error());

    if (data.windowMgr) {
      if (!addedDe || (data.desktopEnv && *data.desktopEnv != *data.windowMgr))
        envInfoRows.push_back({ wmIcon, "WM", *data.windowMgr });
    } else
      debug_at(data.windowMgr.error());

    bool   nowPlayingActive = false;
    String npText;

    if (config.nowPlaying.enabled && data.nowPlaying) {
      const String title  = data.nowPlaying->title.value_or("Unknown Title");
      const String artist = data.nowPlaying->artist.value_or("Unknown Artist");
      npText              = artist + " - " + title;
      nowPlayingActive    = true;
    } else if (config.nowPlaying.enabled)
      debug_at(data.nowPlaying.error());

    usize maxContentWidth = 0;

    usize greetingWidth = get_visual_width_sv(userIcon) + get_visual_width_sv("Hello ") + get_visual_width(name) +
      get_visual_width_sv("! ");
    maxContentWidth = std::max(maxContentWidth, greetingWidth);

    usize paletteWidth = get_visual_width_sv(userIcon) + (16 * (get_visual_width_sv("◯") + get_visual_width_sv(" ")));
    maxContentWidth    = std::max(maxContentWidth, paletteWidth);

    usize iconActualWidth = get_visual_width_sv(userIcon);

    usize maxLabelWidthInitial = find_max_label_len(initialRows);
    usize maxLabelWidthSystem  = find_max_label_len(systemInfoRows);
    usize maxLabelWidthEnv     = find_max_label_len(envInfoRows);

    usize requiredWidthInitialW = iconActualWidth + maxLabelWidthInitial;
    usize requiredWidthSystemW  = iconActualWidth + maxLabelWidthSystem;
    usize requiredWidthEnvW     = iconActualWidth + maxLabelWidthEnv;

    fn calculateRowVisualWidth = [&](const RowInfo& row, const usize requiredLabelVisualWidth) -> usize {
      return requiredLabelVisualWidth + get_visual_width(row.value) + get_visual_width_sv(" ");
    };

    for (const RowInfo& row : initialRows)
      maxContentWidth = std::max(maxContentWidth, calculateRowVisualWidth(row, requiredWidthInitialW));

    for (const RowInfo& row : systemInfoRows)
      maxContentWidth = std::max(maxContentWidth, calculateRowVisualWidth(row, requiredWidthSystemW));

    for (const RowInfo& row : envInfoRows)
      maxContentWidth = std::max(maxContentWidth, calculateRowVisualWidth(row, requiredWidthEnvW));

    usize targetBoxWidth = maxContentWidth + 2;

    usize npFixedWidthLeft  = 0;
    usize npFixedWidthRight = 0;

    if (nowPlayingActive) {
      npFixedWidthLeft  = get_visual_width_sv(musicIcon) + get_visual_width_sv("Playing") + get_visual_width_sv(" ");
      npFixedWidthRight = get_visual_width_sv(" ");
    }

    i32 paragraphLimit = 1;

    if (nowPlayingActive) {
      i32 availableForParagraph =
        static_cast<i32>(targetBoxWidth) - static_cast<i32>(npFixedWidthLeft) - static_cast<i32>(npFixedWidthRight);

      availableForParagraph -= 2;

      paragraphLimit = std::max(1, availableForParagraph);
    }

    fn createStandardRow = [&](const RowInfo& row, const usize sectionRequiredVisualWidth) {
      return hbox(
        {
          hbox(
            {
              text(String(row.icon)) | color(ui::DEFAULT_THEME.icon),
              text(String(row.label)) | color(ui::DEFAULT_THEME.label),
            }
          ) |
            size(WIDTH, EQUAL, static_cast<int>(sectionRequiredVisualWidth)),
          filler(),
          text(row.value) | color(ui::DEFAULT_THEME.value),
          text(" "),
        }
      );
    };

    Elements content;

    content.push_back(text(String(userIcon) + "Hello " + name + "! ") | bold | color(Color::Cyan));
    content.push_back(separator() | color(ui::DEFAULT_THEME.border));
    content.push_back(hbox({ text(String(paletteIcon)) | color(ui::DEFAULT_THEME.icon), CreateColorCircles() }));

    bool section1Present = !initialRows.empty();
    bool section2Present = !systemInfoRows.empty();
    bool section3Present = !envInfoRows.empty();

    if (section1Present)
      content.push_back(separator() | color(ui::DEFAULT_THEME.border));

    for (const RowInfo& row : initialRows) content.push_back(createStandardRow(row, requiredWidthInitialW));

    if ((section1Present && (section2Present || section3Present)) || (!section1Present && section2Present))
      content.push_back(separator() | color(ui::DEFAULT_THEME.border));

    for (const RowInfo& row : systemInfoRows) content.push_back(createStandardRow(row, requiredWidthSystemW));

    if (section2Present && section3Present)
      content.push_back(separator() | color(ui::DEFAULT_THEME.border));

    for (const RowInfo& row : envInfoRows) content.push_back(createStandardRow(row, requiredWidthEnvW));

    if ((section1Present || section2Present || section3Present) && nowPlayingActive)
      content.push_back(separator() | color(ui::DEFAULT_THEME.border));

    if (nowPlayingActive) {
      content.push_back(hbox(
        { text(String(musicIcon)) | color(ui::DEFAULT_THEME.icon),
          text("Playing") | color(ui::DEFAULT_THEME.label),
          text(" "),
          filler(),
          paragraphAlignRight(npText) | color(Color::Magenta) | size(WIDTH, LESS_THAN, paragraphLimit),
          text(" ") }
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
