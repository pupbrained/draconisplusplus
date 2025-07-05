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

  struct UIGroup {
    Vec<RowInfo> rows;
    Vec<usize>   iconWidths;
    Vec<usize>   labelWidths;
    Vec<usize>   valueWidths;
    usize        maxLabelWidth = 0;
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

    constexpr StringView COLOR_CIRCLES =
      "\033[38;5;0m◯\033[0m "
      "\033[38;5;1m◯\033[0m "
      "\033[38;5;2m◯\033[0m "
      "\033[38;5;3m◯\033[0m "
      "\033[38;5;4m◯\033[0m "
      "\033[38;5;5m◯\033[0m "
      "\033[38;5;6m◯\033[0m "
      "\033[38;5;7m◯\033[0m "
      "\033[38;5;8m◯\033[0m "
      "\033[38;5;9m◯\033[0m "
      "\033[38;5;10m◯\033[0m "
      "\033[38;5;11m◯\033[0m "
      "\033[38;5;12m◯\033[0m "
      "\033[38;5;13m◯\033[0m "
      "\033[38;5;14m◯\033[0m "
      "\033[38;5;15m◯\033[0m";

    fn GetVisualWidth(const StringView& str) -> usize {
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

    fn ProcessGroup(UIGroup& group) -> usize {
      if (group.rows.empty())
        return 0;

      group.iconWidths.reserve(group.rows.size());
      group.labelWidths.reserve(group.rows.size());
      group.valueWidths.reserve(group.rows.size());

      for (const auto& row : group.rows) {
        const usize labelWidth = GetVisualWidth(row.label);
        group.maxLabelWidth    = std::max(group.maxLabelWidth, labelWidth);
        group.iconWidths.push_back(GetVisualWidth(row.icon));
        group.labelWidths.push_back(labelWidth);
        group.valueWidths.push_back(GetVisualWidth(row.value));
      }

      const auto zippedWidths = std::views::zip(group.iconWidths, group.valueWidths);

      const usize groupMaxWidth =
        std::ranges::max(
          zippedWidths | std::views::transform([&](const auto& widths) {
            const auto& [iconW, valueW] = widths;
            return iconW + group.maxLabelWidth + 1 + valueW;
          })
        );

      return groupMaxWidth;
    }

    fn RenderGroup(std::stringstream& stream, const UIGroup& group, const usize maxContentWidth, const String& hBorder, bool& hasRenderedContent) {
      if (group.rows.empty())
        return;

      if (hasRenderedContent)
        stream << "├" << hBorder << "┤\n";

      auto createLine = [&](const String& left, const String& right = "") {
        const usize leftWidth  = GetVisualWidth(left);
        const usize rightWidth = GetVisualWidth(right);
        const usize padding    = (maxContentWidth >= leftWidth + rightWidth) ? maxContentWidth - (leftWidth + rightWidth) : 0;

        stream << "│" << left << String(padding, ' ') << right << " │\n";
      };

      const auto zippedRows = std::views::zip(group.rows, group.labelWidths);

      for (const auto& [row, labelWidth] : zippedRows)
        createLine(
          Colorize(row.icon, DEFAULT_THEME.icon) + Colorize(row.label, DEFAULT_THEME.label) + String(group.maxLabelWidth - labelWidth, ' '),
          Colorize(row.value, DEFAULT_THEME.value)
        );

      hasRenderedContent = true;
    }

    fn WordWrap(const StringView& text, const usize wrapWidth) -> Vec<String> {
      Vec<String> lines;

      if (wrapWidth == 0) {
        lines.emplace_back(text);
        return lines;
      }

      std::stringstream textStream((String(text)));
      String            word;
      String            currentLine;

      while (textStream >> word) {
        if (!currentLine.empty() && GetVisualWidth(currentLine) + GetVisualWidth(word) + 1 > wrapWidth) {
          lines.emplace_back(currentLine);
          currentLine.clear();
        }

        if (!currentLine.empty())
          currentLine += " ";

        currentLine += word;
      }

      if (!currentLine.empty())
        lines.emplace_back(currentLine);

      return lines;
    }
  } // namespace

#if DRAC_ENABLE_WEATHER
  fn CreateUI(const Config& config, const SystemInfo& data, Option<Report> weather) -> String {
#else
  fn CreateUI(const Config& config, const SystemInfo& data) -> String {
#endif
    const String& name     = config.general.name;
    const Icons&  iconType = ICON_TYPE;

    UIGroup initialGroup;
    UIGroup systemInfoGroup;
    UIGroup hardwareGroup;
    UIGroup softwareGroup;
    UIGroup envInfoGroup;

    {
      if (data.date)
        initialGroup.rows.push_back({ iconType.calendar, "Date", *data.date });

#if DRAC_ENABLE_WEATHER
      if (weather) {
        const auto& [temperature, townName, description] = *weather;

        PCStr tempUnit =
          config.weather.units == services::weather::UnitSystem::METRIC
          ? "C"
          : "F";

        initialGroup.rows.push_back(
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
        systemInfoGroup.rows.push_back({ iconType.host, "Host", *data.host });

      if (data.osVersion)
        systemInfoGroup.rows.push_back({
#ifdef __linux__
          GetDistroIcon(*data.osVersion).value_or(iconType.os),
#else
          iconType.os,
#endif
          "OS",
          *data.osVersion,
        });

      if (data.kernelVersion)
        systemInfoGroup.rows.push_back({ iconType.kernel, "Kernel", *data.kernelVersion });
    }

    {
      if (data.memInfo)
        hardwareGroup.rows.push_back({ iconType.memory, "RAM", std::format("{}/{}", BytesToGiB(data.memInfo->usedBytes), BytesToGiB(data.memInfo->totalBytes)) });

      if (data.diskUsage)
        hardwareGroup.rows.push_back({ iconType.disk, "Disk", std::format("{}/{}", BytesToGiB(data.diskUsage->usedBytes), BytesToGiB(data.diskUsage->totalBytes)) });

      if (data.cpuModel)
        hardwareGroup.rows.push_back({ iconType.cpu, "CPU", *data.cpuModel });

      if (data.gpuModel)
        hardwareGroup.rows.push_back({ iconType.gpu, "GPU", *data.gpuModel });

      if (data.uptime)
        hardwareGroup.rows.push_back({ iconType.uptime, "Uptime", std::format("{}", SecondsToFormattedDuration { *data.uptime }) });
    }

    {
      if (data.shell)
        softwareGroup.rows.push_back({ iconType.shell, "Shell", *data.shell });

#if DRAC_ENABLE_PACKAGECOUNT
      if (data.packageCount && *data.packageCount > 0)
        softwareGroup.rows.push_back({ iconType.package, "Packages", std::format("{}", *data.packageCount) });
#endif
    }

    {
      const bool deExists = data.desktopEnv.has_value();
      const bool wmExists = data.windowMgr.has_value();

      if (deExists && wmExists) {
        if (*data.desktopEnv == *data.windowMgr)
          envInfoGroup.rows.push_back({ iconType.windowManager, "WM", *data.windowMgr });
        else {
          envInfoGroup.rows.push_back({ iconType.desktopEnvironment, "DE", *data.desktopEnv });
          envInfoGroup.rows.push_back({ iconType.windowManager, "WM", *data.windowMgr });
        }
      } else if (deExists)
        envInfoGroup.rows.push_back({ iconType.desktopEnvironment, "DE", *data.desktopEnv });
      else if (wmExists)
        envInfoGroup.rows.push_back({ iconType.windowManager, "WM", *data.windowMgr });
    }

    Vec<UIGroup*> groups = { &initialGroup, &systemInfoGroup, &hardwareGroup, &softwareGroup, &envInfoGroup };

    usize maxContentWidth = 0;

    for (UIGroup* group : groups) {
      if (group->rows.empty())
        continue;

      maxContentWidth = std::max(maxContentWidth, ProcessGroup(*group));
    }

    String greetingLine = std::format("{}Hello {}!", iconType.user, name);
    maxContentWidth     = std::max(maxContentWidth, GetVisualWidth(greetingLine));

    maxContentWidth = std::max(maxContentWidth, GetVisualWidth(iconType.palette) + GetVisualWidth(COLOR_CIRCLES));

#if DRAC_ENABLE_NOWPLAYING
    bool   nowPlayingActive = false;
    String npText;

    if (config.nowPlaying.enabled && data.nowPlaying) {
      npText           = std::format("{} - {}", data.nowPlaying->artist.value_or("Unknown Artist"), data.nowPlaying->title.value_or("Unknown Title"));
      nowPlayingActive = true;
    }
#endif

    std::stringstream stream;

    const usize innerWidth = maxContentWidth + 1;

    String hBorder;
    hBorder.reserve(innerWidth * 3);
    for (usize i = 0; i < innerWidth; ++i) hBorder += "─";

    auto createLine = [&](const String& left, const String& right = "") {
      const usize leftWidth  = GetVisualWidth(left);
      const usize rightWidth = GetVisualWidth(right);
      const usize padding    = (maxContentWidth >= leftWidth + rightWidth) ? maxContentWidth - (leftWidth + rightWidth) : 0;

      stream << "│" << left << String(padding, ' ') << right << " │\n";
    };

    auto createLeftAlignedLine = [&](const String& content) { createLine(content, ""); };

    stream << "╭" << hBorder << "╮\n";
    createLeftAlignedLine(Colorize(greetingLine, DEFAULT_THEME.icon));

    stream << "├" << hBorder << "┤\n";
    createLeftAlignedLine(Colorize(iconType.palette, DEFAULT_THEME.icon) + String(COLOR_CIRCLES));
    bool hasRenderedContent = true;

    for (const UIGroup* group : groups)
      RenderGroup(stream, *group, maxContentWidth, hBorder, hasRenderedContent);

#if DRAC_ENABLE_NOWPLAYING
    if (nowPlayingActive) {
      if (hasRenderedContent)
        stream << "├" << hBorder << "┤\n";

      const String leftPart      = Colorize(iconType.music, DEFAULT_THEME.icon) + Colorize("Playing", DEFAULT_THEME.label);
      const usize  leftPartWidth = GetVisualWidth(leftPart);

      const usize availableWidth = maxContentWidth - leftPartWidth;

      const Vec<String> wrappedLines = WordWrap(npText, availableWidth);

      if (!wrappedLines.empty()) {
        createLine(leftPart, Colorize(wrappedLines[0], LogColor::Magenta));

        const String indent(leftPartWidth, ' ');

        for (usize i = 1; i < wrappedLines.size(); ++i) {
          String rightPart      = Colorize(wrappedLines[i], LogColor::Magenta);
          usize  rightPartWidth = GetVisualWidth(rightPart);

          usize padding = (maxContentWidth > leftPartWidth + rightPartWidth)
            ? maxContentWidth - leftPartWidth - rightPartWidth
            : 0;

          String lineContent = indent;
          lineContent.append(padding, ' ');
          lineContent.append(rightPart);
          createLine(lineContent);
        }
      }
    }
#endif

    stream << "╰" << hBorder << "╯\n";
    return stream.str();
  }
} // namespace draconis::ui
