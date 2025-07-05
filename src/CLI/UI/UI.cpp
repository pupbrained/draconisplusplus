#include "UI.hpp"

#include <sstream>

#include <Drac++/Utils/Logging.hpp>
#include <Drac++/Utils/Types.hpp>

using namespace draconis::utils::types;
using namespace draconis::utils::logging;

namespace draconis::ui {
  using config::Config;

  using core::system::SystemInfo;

  using services::weather::Report;

  constexpr Theme DEFAULT_THEME = {
    .icon   = LogColor::Cyan,
    .label  = LogColor::Yellow,
    .value  = LogColor::White,
    .border = LogColor::Gray,
  };

  [[maybe_unused]] static constexpr Icons NONE = {
    .calendar           = "",
    .desktopEnvironment = "",
    .disk               = "",
    .host               = "",
    .kernel             = "",
    .memory             = "",
    .cpu                = "",
    .gpu                = "",
    .uptime             = "",
#if DRAC_ENABLE_NOWPLAYING
    .music = "",
#endif
    .os = "",
#if DRAC_ENABLE_PACKAGECOUNT
    .package = "",
#endif
    .palette = "",
    .shell   = "",
    .user    = "",
#if DRAC_ENABLE_WEATHER
    .weather = "",
#endif
    .windowManager = "",
  };

  [[maybe_unused]] static constexpr Icons NERD = {
    .calendar           = "   ",
    .desktopEnvironment = " 󰇄  ",
    .disk               = " 󰋊  ",
    .host               = " 󰌢  ",
    .kernel             = "   ",
    .memory             = "   ",
#if DRAC_ARCH_64BIT
    .cpu = " 󰻠  ",
#else
    .cpu = " 󰻟  ",
#endif
    .gpu    = "   ",
    .uptime = "   ",
#if DRAC_ENABLE_NOWPLAYING
    .music = "   ",
#endif
#ifdef __linux__
    .os = " 󰌽  ",
#elifdef __APPLE__
    .os = "   ",
#elifdef _WIN32
    .os = "   ",
#elifdef __FreeBSD__
    .os = "   ",
#else
    .os = "   ",
#endif
#if DRAC_ENABLE_PACKAGECOUNT
    .package = " 󰏖  ",
#endif
    .palette = "   ",
    .shell   = "   ",
    .user    = "   ",
#if DRAC_ENABLE_WEATHER
    .weather = "   ",
#endif
    .windowManager = "   ",
  };

  [[maybe_unused]] static constexpr Icons EMOJI = {
    .calendar           = " 📅 ",
    .desktopEnvironment = " 🖥️ ",
    .disk               = " 💾 ",
    .host               = " 💻 ",
    .kernel             = " 🫀 ",
    .memory             = " 🧠 ",
    .cpu                = " 💻 ",
    .gpu                = " 🎨 ",
    .uptime             = " ⏰ ",
#if DRAC_ENABLE_NOWPLAYING
    .music = " 🎵 ",
#endif
    .os = " 🤖 ",
#if DRAC_ENABLE_PACKAGECOUNT
    .package = " 📦 ",
#endif
    .palette = " 🎨 ",
    .shell   = " 💲 ",
    .user    = " 👤 ",
#if DRAC_ENABLE_WEATHER
    .weather = " 🌈 ",
#endif
    .windowManager = " 🪟 ",
  };

  constexpr inline Icons ICON_TYPE = NERD;

  struct RowInfo {
    StringView icon;
    StringView label;
    String     value;
  };

  namespace {
#ifdef __linux__
    // clang-format off
    constexpr Array<Pair<StringView, StringView>, 13> distro_icons {{
      {        "NixOS", "   " },
      {        "Zorin", "   " },
      {       "Debian", "   " },
      {       "Fedora", "   " },
      {       "Gentoo", "   " },
      {       "Ubuntu", "   " },
      {      "Manjaro", "   " },
      {      "Pop!_OS", "   " },
      {   "Arch Linux", "   " },
      {   "Linux Mint", "   " },
      {   "Void Linux", "   " },
      { "Alpine Linux", "   " },
    }};
    // clang-format on

    fn GetDistroIcon(StringView distro) -> Option<StringView> {
      for (const auto& [distroName, distroIcon] : distro_icons)
        if (distro.contains(distroName))
          return distroIcon;

      return None;
    }
#endif // __linux__

    fn CreateColorCircles() -> String {
      std::stringstream stream;

      for (usize i = 0; i < 16; ++i)
        stream << Colorize("◯", static_cast<LogColor>(i)) << (((i == 15) ? "" : " "));

      return stream.str();
    }

    fn get_visual_width(const String& str) -> usize {
      usize width    = 0;
      bool  inEscape = false;

      for (const char& character : str)
        if (inEscape)
          inEscape = (character != 'm');
        else if (character == '\033')
          inEscape = true;
        else if ((character & 0xC0) != 0x80)
          width++;

      return width;
    }

    fn get_visual_width_sv(const StringView& sview) -> usize {
      return get_visual_width(String(sview));
    }

    fn find_max_label_len(const Vec<RowInfo>& rows) -> usize {
      usize maxWidth = 0;

      for (const RowInfo& row : rows)
        maxWidth = std::max(maxWidth, get_visual_width_sv(row.label));

      return maxWidth;
    }
  } // namespace

#if DRAC_ENABLE_WEATHER
  fn CreateUI(const Config& config, const SystemInfo& data, Option<Report> weather) -> String {
#else
  fn CreateUI(const Config& config, const SystemInfo& data) -> String {
#endif
    const String& name     = config.general.name;
    const Icons&  iconType = ICON_TYPE;

    Vec<RowInfo> initialRows;
    Vec<RowInfo> systemInfoRows;
    Vec<RowInfo> hardwareRows;
    Vec<RowInfo> softwareRows;
    Vec<RowInfo> envInfoRows;

    {
      if (data.date)
        initialRows.push_back({ iconType.calendar, "Date", *data.date });

#if DRAC_ENABLE_WEATHER
      if (weather) {
        const auto& [temperature, townName, description] = *weather;

        PCStr tempUnit =
          config.weather.units == services::weather::UnitSystem::METRIC
          ? "C"
          : "F";

        initialRows.push_back(
          {
            iconType.weather,
            "Weather",
            config.weather.showTownName && townName
              ? std::format("{}°{} in {}", std::lround(temperature), tempUnit, *townName)
              : std::format("{}°{}, {}", std::lround(temperature), tempUnit, description),
          }
        );
      }
#endif
    }

    {
      if (data.host && !data.host->empty())
        systemInfoRows.push_back({ iconType.host, "Host", *data.host });

      if (data.osVersion)
        systemInfoRows.push_back({
#ifdef __linux__
          GetDistroIcon(*data.osVersion).value_or(iconType.os),
#else
          iconType.os,
#endif
          "OS",
          *data.osVersion,
        });

      if (data.kernelVersion)
        systemInfoRows.push_back({ iconType.kernel, "Kernel", *data.kernelVersion });
    }

    {
      if (data.memInfo)
        hardwareRows.push_back({ iconType.memory, "RAM", std::format("{}/{}", BytesToGiB(data.memInfo->usedBytes), BytesToGiB(data.memInfo->totalBytes)) });

      if (data.diskUsage)
        hardwareRows.push_back({ iconType.disk, "Disk", std::format("{}/{}", BytesToGiB(data.diskUsage->usedBytes), BytesToGiB(data.diskUsage->totalBytes)) });

      if (data.cpuModel)
        hardwareRows.push_back({ iconType.cpu, "CPU", *data.cpuModel });

      if (data.gpuModel)
        hardwareRows.push_back({ iconType.gpu, "GPU", *data.gpuModel });

      if (data.uptime)
        hardwareRows.push_back({ iconType.uptime, "Uptime", std::format("{}", SecondsToFormattedDuration { *data.uptime }) });
    }

    {
      if (data.shell)
        softwareRows.push_back({ iconType.shell, "Shell", *data.shell });

#if DRAC_ENABLE_PACKAGECOUNT
      if (data.packageCount && *data.packageCount > 0)
        softwareRows.push_back({ iconType.package, "Packages", std::format("{}", *data.packageCount) });
#endif
    }

    {
      const bool deExists = data.desktopEnv.has_value();
      const bool wmExists = data.windowMgr.has_value();

      if (deExists && wmExists) {
        if (*data.desktopEnv == *data.windowMgr)
          envInfoRows.push_back({ iconType.windowManager, "WM", *data.windowMgr });
        else {
          envInfoRows.push_back({ iconType.desktopEnvironment, "DE", *data.desktopEnv });
          envInfoRows.push_back({ iconType.windowManager, "WM", *data.windowMgr });
        }
      } else if (deExists)
        envInfoRows.push_back({ iconType.desktopEnvironment, "DE", *data.desktopEnv });
      else if (wmExists)
        envInfoRows.push_back({ iconType.windowManager, "WM", *data.windowMgr });
    }

    usize maxContentWidth = 0;

    fn getGroupMaxWidth = [&](const Vec<RowInfo>& group) -> usize {
      if (group.empty())
        return 0;

      usize groupMaxLabelWidth = find_max_label_len(group);
      usize groupMaxWidth      = 0;

      for (const RowInfo& row : group)
        groupMaxWidth = std::max(groupMaxWidth, get_visual_width_sv(row.icon) + groupMaxLabelWidth + 1 + get_visual_width(row.value));

      return groupMaxWidth;
    };

    maxContentWidth = std::max({
      getGroupMaxWidth(initialRows),
      getGroupMaxWidth(systemInfoRows),
      getGroupMaxWidth(hardwareRows),
      getGroupMaxWidth(softwareRows),
      getGroupMaxWidth(envInfoRows),
    });

    String greetingLine = String(iconType.user) + "Hello " + name + "!";
    maxContentWidth     = std::max(maxContentWidth, get_visual_width(greetingLine));

    String paletteLine = String(iconType.palette) + CreateColorCircles();
    maxContentWidth    = std::max(maxContentWidth, get_visual_width(paletteLine));

#if DRAC_ENABLE_NOWPLAYING
    bool   nowPlayingActive = false;
    String npText;

    if (config.nowPlaying.enabled && data.nowPlaying) {
      npText           = data.nowPlaying->artist.value_or("Unknown Artist") + " - " + data.nowPlaying->title.value_or("Unknown Title");
      nowPlayingActive = true;

      String playingLeft = String(iconType.music) + "Playing";

      maxContentWidth = std::max(maxContentWidth, get_visual_width(playingLeft) + 1 + get_visual_width(npText));
    }
#endif

    std::stringstream stream;

    const usize innerWidth = maxContentWidth + 1;

    String hBorder;

    for (usize i = 0; i < innerWidth; ++i) hBorder += "─";

    fn createLine = [&](const String& left, const String& right = "") {
      usize leftWidth  = get_visual_width(left);
      usize rightWidth = get_visual_width(right);
      usize padding    = (maxContentWidth >= leftWidth + rightWidth) ? maxContentWidth - (leftWidth + rightWidth) : 0;

      stream << "│" << left << String(padding, ' ') << right << " │\n";
    };

    fn createLeftAlignedLine = [&](const String& content) { createLine(content, ""); };

    stream << "╭" << hBorder << "╮\n";
    createLeftAlignedLine(Colorize(greetingLine, DEFAULT_THEME.icon));

    stream << "├" << hBorder << "┤\n";
    createLeftAlignedLine(Colorize(String(iconType.palette), DEFAULT_THEME.icon) + CreateColorCircles());

    bool hasRenderedContent = false;

    fn renderGroup = [&](const Vec<RowInfo>& group) {
      if (group.empty())
        return;

      const usize groupMaxLabelWidth = find_max_label_len(group);

      if (hasRenderedContent)
        stream << "├" << hBorder << "┤\n";

      for (const RowInfo& row : group)
        createLine(
          Colorize(String(row.icon), DEFAULT_THEME.icon) + Colorize(String(row.label), DEFAULT_THEME.label) + String(groupMaxLabelWidth - get_visual_width_sv(row.label), ' '),
          Colorize(row.value, DEFAULT_THEME.value)
        );

      hasRenderedContent = true;
    };

    hasRenderedContent = true;

    renderGroup(initialRows);
    renderGroup(systemInfoRows);
    renderGroup(hardwareRows);
    renderGroup(softwareRows);
    renderGroup(envInfoRows);

#if DRAC_ENABLE_NOWPLAYING
    if (nowPlayingActive) {
      if (hasRenderedContent)
        stream << "├" << hBorder << "┤\n";

      createLine(
        Colorize(String(iconType.music), DEFAULT_THEME.icon) + Colorize("Playing", DEFAULT_THEME.label),
        Colorize(npText, LogColor::Magenta)
      );
    }
#endif

    stream << "╰" << hBorder << "╯\n";
    return stream.str();
  }
} // namespace draconis::ui
