#pragma once

#include <ftxui/dom/elements.hpp> // ftxui::Element
#include <ftxui/screen/color.hpp> // ftxui::Color

#include <Drac++/Core/System.hpp>

#include <DracUtils/Types.hpp>

#include "Config/Config.hpp"

namespace draconis::ui {
  struct Theme {
    ftxui::Color::Palette16 icon;
    ftxui::Color::Palette16 label;
    ftxui::Color::Palette16 value;
    ftxui::Color::Palette16 border;
  };

  extern const Theme DEFAULT_THEME;

  struct Icons {
    draconis::utils::types::StringView calendar;
    draconis::utils::types::StringView desktopEnvironment;
    draconis::utils::types::StringView disk;
    draconis::utils::types::StringView host;
    draconis::utils::types::StringView kernel;
    draconis::utils::types::StringView memory;
    draconis::utils::types::StringView cpu;
    draconis::utils::types::StringView gpu;
#if DRAC_ENABLE_NOWPLAYING
    draconis::utils::types::StringView music;
#endif
    draconis::utils::types::StringView os;
#if DRAC_ENABLE_PACKAGECOUNT
    draconis::utils::types::StringView package;
#endif
    draconis::utils::types::StringView palette;
    draconis::utils::types::StringView shell;
    draconis::utils::types::StringView user;
#if DRAC_ENABLE_WEATHER
    draconis::utils::types::StringView weather;
#endif
    draconis::utils::types::StringView windowManager;
  };

  extern const Icons ICON_TYPE;

  /**
   * @brief Creates the main UI element based on system data and configuration.
   * @param config The application configuration.
   * @param data The collected system data. @return The root ftxui::Element for rendering.
   */
  fn CreateUI(const config::Config& config, const draconis::core::system::System& data) -> ftxui::Element;
} // namespace draconis::ui
