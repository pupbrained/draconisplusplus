#include <cmath>                   // std::lround
#include <format>                  // std::format
#include <ftxui/dom/elements.hpp>  // ftxui::{Element, hbox, vbox, text, separator, filler, etc.}
#include <ftxui/dom/node.hpp>      // ftxui::{Render}
#include <ftxui/screen/color.hpp>  // ftxui::Color
#include <ftxui/screen/screen.hpp> // ftxui::{Screen, Dimension::Full}
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

  // Helper struct to hold row data before calculating max width
  struct RowInfo {
    StringView icon;
    StringView label;
    String     value; // Store the final formatted value as String
  };

  fn CreateColorCircles() -> Element {
    return hbox(
      std::views::iota(0, 16) | std::views::transform([](ui::i32 colorIndex) {
        return hbox({ text("◯") | bold | color(static_cast<Color::Palette256>(colorIndex)), text(" ") });
      }) |
      std::ranges::to<Elements>()
    );
  }

  fn find_max_label_len(const std::vector<RowInfo>& rows) -> usize {
    usize max_len = 0;
    for (const auto& row : rows) { max_len = std::max(max_len, row.label.length()); }
    return max_len;
  };

  fn SystemInfoBox(const Config& config, const os::SystemData& data) -> Element {
    const String&  name    = config.general.name;
    const Weather& weather = config.weather;

    const auto& [userIcon, paletteIcon, calendarIcon, hostIcon, kernelIcon, osIcon, memoryIcon, weatherIcon, musicIcon, diskIcon, shellIcon, packageIcon, deIcon, wmIcon] =
      ui::ICON_TYPE;

    // --- Stage 1: Collect data for rows into logical sections ---
    std::vector<RowInfo> initial_rows;     // Date, Weather
    std::vector<RowInfo> system_info_rows; // Host, Kernel, OS, RAM, Disk, Shell, Packages
    std::vector<RowInfo> env_info_rows;    // DE, WM

    // --- Section 1: Date and Weather ---
    if (data.date) {
      initial_rows.push_back({ calendarIcon, "Date", *data.date });
    } else {
      debug_at(data.date.error());
    }
    if (weather.enabled && data.weather) {
      const weather::Output& weatherInfo  = *data.weather;
      String                 weatherValue = weather.showTownName
                        ? std::format("{}°F in {}", std::lround(weatherInfo.main.temp), weatherInfo.name)
                        : std::format("{}°F, {}", std::lround(weatherInfo.main.temp), weatherInfo.weather[0].description);
      initial_rows.push_back({ weatherIcon, "Weather", std::move(weatherValue) });
    } else if (weather.enabled) {
      debug_at(data.weather.error());
    }

    // --- Section 2: Core System Info ---
    if (data.host && !data.host->empty()) {
      system_info_rows.push_back({ hostIcon, "Host", *data.host });
    } else {
      debug_at(data.host.error());
    }
    if (data.kernelVersion) {
      system_info_rows.push_back({ kernelIcon, "Kernel", *data.kernelVersion });
    } else {
      debug_at(data.kernelVersion.error());
    }
    if (data.osVersion) {
      system_info_rows.push_back({ osIcon, "OS", *data.osVersion });
    } else {
      debug_at(data.osVersion.error());
    }
    if (data.memInfo) {
      system_info_rows.push_back({ memoryIcon, "RAM", std::format("{}", BytesToGiB { *data.memInfo }) });
    } else {
      debug_at(data.memInfo.error());
    }
    if (data.diskUsage) {
      system_info_rows.push_back(
        { diskIcon,
          "Disk",
          std::format("{}/{}", BytesToGiB { data.diskUsage->used_bytes }, BytesToGiB { data.diskUsage->total_bytes }) }
      );
    } else {
      debug_at(data.diskUsage.error());
    }
    if (data.shell) {
      system_info_rows.push_back({ shellIcon, "Shell", *data.shell });
    } else {
      debug_at(data.shell.error());
    }
    if (data.packageCount) {
      if (*data.packageCount > 0) {
        system_info_rows.push_back({ packageIcon, "Packages", std::format("{}", *data.packageCount) });
      } else {
        debug_log("Package count is 0, skipping");
      }
    } else {
      debug_at(data.packageCount.error());
    }

    // --- Section 3: Desktop Env / Window Manager ---
    bool added_de = false;
    if (data.desktopEnv && (!data.windowMgr || *data.desktopEnv != *data.windowMgr)) {
      env_info_rows.push_back({ deIcon, "DE", *data.desktopEnv });
      added_de = true;
    } else if (!data.desktopEnv) { /* Optional debug */
    }
    if (data.windowMgr) {
      if (!added_de || (data.desktopEnv && *data.desktopEnv != *data.windowMgr)) {
        env_info_rows.push_back({ wmIcon, "WM", *data.windowMgr });
      }
    } else {
      debug_at(data.windowMgr.error());
    }

    // --- Section 4: Now Playing (Handled separately) ---
    bool   now_playing_active = false;
    String np_text;
    if (config.nowPlaying.enabled && data.nowPlaying) {
      const String title  = data.nowPlaying->title.value_or("Unknown Title");
      const String artist = data.nowPlaying->artist.value_or("Unknown Artist");
      np_text             = artist + " - " + title;
      now_playing_active  = true;
    } else if (config.nowPlaying.enabled) { /* Optional debug */
    }

    // --- Stage 2: Calculate max width needed for Icon + Label across relevant sections ---
    usize maxActualLabelLen = 0;
    auto  find_max_label    = [&](const std::vector<RowInfo>& rows) {
      usize max_len = 0;
      for (const auto& row : rows) { max_len = std::max(max_len, row.label.length()); }
      return max_len;
    };

    maxActualLabelLen =
      std::max({ find_max_label(initial_rows), find_max_label(system_info_rows), find_max_label(env_info_rows) });
    // Note: We don't include "Playing" from Now Playing in this calculation
    // as it's handled differently, but we could if we wanted perfect alignment.

    // --- Stage 2: Calculate max width needed PER SECTION ---
    // Assume consistent icon width for simplicity (adjust if icons vary significantly)
    usize iconLen = ui::ICON_TYPE.user.length() - 1;
    // Optionally refine iconLen based on actual icons used, if needed

    usize maxLabelLen_initial = find_max_label_len(initial_rows);
    usize maxLabelLen_system  = find_max_label_len(system_info_rows);
    usize maxLabelLen_env     = find_max_label_len(env_info_rows);

    usize requiredWidth_initial = iconLen + maxLabelLen_initial;
    usize requiredWidth_system  = iconLen + maxLabelLen_system;
    usize requiredWidth_env     = iconLen + maxLabelLen_env;

    // --- Stage 3: Define the row creation function ---
    auto createStandardRow = [&](const RowInfo& row, usize sectionRequiredWidth) {
      Element leftPart = hbox(
        {
          text(String(row.icon)) | color(ui::DEFAULT_THEME.icon),
          text(String(row.label)) | color(ui::DEFAULT_THEME.label),
        }
      );
      return hbox(
        {
          leftPart | size(WIDTH, EQUAL, static_cast<int>(sectionRequiredWidth)),
          filler(),
          text(row.value) | color(ui::DEFAULT_THEME.value),
          text(" "),
        }
      );
    };

    // --- Stage 4: Build the final Elements list with explicit separators and section-specific widths ---
    Elements content;

    // Greeting and Palette
    content.push_back(text(String(userIcon) + "Hello " + name + "! ") | bold | color(Color::Cyan));
    content.push_back(separator() | color(ui::DEFAULT_THEME.border)); // Separator after greeting
    content.push_back(hbox({ text(String(paletteIcon)) | color(ui::DEFAULT_THEME.icon), CreateColorCircles() }));
    content.push_back(separator() | color(ui::DEFAULT_THEME.border)); // Separator after palette

    // Determine section presence
    bool section1_present = !initial_rows.empty();
    bool section2_present = !system_info_rows.empty();
    bool section3_present = !env_info_rows.empty();
    bool section4_present = now_playing_active;

    // Add Section 1 (Date/Weather) - Use initial width
    for (const auto& row : initial_rows) { content.push_back(createStandardRow(row, requiredWidth_initial)); }

    // Separator before Section 2?
    if (section1_present && (section2_present || section3_present || section4_present)) {
      content.push_back(separator() | color(ui::DEFAULT_THEME.border));
    }

    // Add Section 2 (System Info) - Use system width
    for (const auto& row : system_info_rows) { content.push_back(createStandardRow(row, requiredWidth_system)); }

    // Separator before Section 3?
    if (section2_present && (section3_present || section4_present)) {
      content.push_back(separator() | color(ui::DEFAULT_THEME.border));
    }

    // Add Section 3 (DE/WM) - Use env width
    for (const auto& row : env_info_rows) { content.push_back(createStandardRow(row, requiredWidth_env)); }

    // Separator before Section 4?
    if (section3_present && section4_present) {
      content.push_back(separator() | color(ui::DEFAULT_THEME.border));
    } else if (!section3_present && (section1_present || section2_present) && section4_present) {
      content.push_back(separator() | color(ui::DEFAULT_THEME.border));
    }

    // Add Section 4 (Now Playing)
    if (section4_present) {
      // Pad "Playing" label based on the max label length of the preceding section (Env)
      usize playingLabelPadding = maxLabelLen_env;
      content.push_back(hbox(
        {
          text(String(musicIcon)) | color(ui::DEFAULT_THEME.icon),
          // Pad only the label part
          hbox({ text("Playing") | color(ui::DEFAULT_THEME.label) }) |
            size(WIDTH, EQUAL, static_cast<int>(playingLabelPadding)),
          text(" "), // Space after label
          filler(),
          paragraph(np_text) | color(Color::Magenta) | size(WIDTH, LESS_THAN, ui::MAX_PARAGRAPH_LENGTH),
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
