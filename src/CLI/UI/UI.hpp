#pragma once

#include <ftxui/dom/elements.hpp> // ftxui::Element
#include <ftxui/screen/color.hpp> // ftxui::Color

#include <Drac++/Core/System.hpp>

#include <DracUtils/Types.hpp>

#include "Config/Config.hpp"

namespace ui {
  struct Theme {
    ftxui::Color::Palette16 icon;
    ftxui::Color::Palette16 label;
    ftxui::Color::Palette16 value;
    ftxui::Color::Palette16 border;
  };

  extern const Theme DEFAULT_THEME;

  struct Icons {
    util::types::StringView calendar;
    util::types::StringView desktopEnvironment;
    util::types::StringView disk;
    util::types::StringView host;
    util::types::StringView kernel;
    util::types::StringView memory;
    util::types::StringView cpu;
    util::types::StringView gpu;
#if DRAC_ENABLE_NOWPLAYING
    util::types::StringView music;
#endif
    util::types::StringView os;
#if DRAC_ENABLE_PACKAGECOUNT
    util::types::StringView package;
#endif
    util::types::StringView palette;
    util::types::StringView shell;
    util::types::StringView user;
#if DRAC_ENABLE_WEATHER
    util::types::StringView weather;
#endif
    util::types::StringView windowManager;
  };

  extern const Icons ICON_TYPE;

  /**
   * @brief Creates the main UI element based on system data and configuration.
   * @param config The application configuration.
   * @param data The collected system data. @return The root ftxui::Element for rendering.
   */
  fn CreateUI(const Config& config, const os::System& data) -> ftxui::Element;
} // namespace ui
