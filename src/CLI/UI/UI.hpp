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
    drac::types::StringView calendar;
    drac::types::StringView desktopEnvironment;
    drac::types::StringView disk;
    drac::types::StringView host;
    drac::types::StringView kernel;
    drac::types::StringView memory;
    drac::types::StringView cpu;
    drac::types::StringView gpu;
#if DRAC_ENABLE_NOWPLAYING
    drac::types::StringView music;
#endif
    drac::types::StringView os;
#if DRAC_ENABLE_PACKAGECOUNT
    drac::types::StringView package;
#endif
    drac::types::StringView palette;
    drac::types::StringView shell;
    drac::types::StringView user;
#if DRAC_ENABLE_WEATHER
    drac::types::StringView weather;
#endif
    drac::types::StringView windowManager;
  };

  extern const Icons ICON_TYPE;

  /**
   * @brief Creates the main UI element based on system data and configuration.
   * @param config The application configuration.
   * @param data The collected system data. @return The root ftxui::Element for rendering.
   */
  fn CreateUI(const Config& config, const os::System& data) -> ftxui::Element;
} // namespace ui
