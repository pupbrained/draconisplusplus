#pragma once

#include <ftxui/dom/elements.hpp> // ftxui::Element
#include <ftxui/screen/color.hpp> // ftxui::Color

#include <Drac++/Services/Weather.hpp>

#include <Drac++/Utils/Types.hpp>

#include "Config/Config.hpp"
#include "Core/SystemInfo.hpp"

namespace draconis::ui {
  namespace {
    using config::Config;
    using core::system::SystemInfo;
    using ftxui::Element;
    using services::weather::Report;
    using utils::types::Option;
    using utils::types::StringView;

    using Palette16 = ftxui::Color::Palette16;
  } // namespace

  struct Theme {
    Palette16 icon;
    Palette16 label;
    Palette16 value;
    Palette16 border;
  };

  extern const Theme DEFAULT_THEME;

  struct Icons {
    StringView calendar;
    StringView desktopEnvironment;
    StringView disk;
    StringView host;
    StringView kernel;
    StringView memory;
    StringView cpu;
    StringView gpu;
#if DRAC_ENABLE_NOWPLAYING
    StringView music;
#endif
    StringView os;
#if DRAC_ENABLE_PACKAGECOUNT
    StringView package;
#endif
    StringView palette;
    StringView shell;
    StringView user;
#if DRAC_ENABLE_WEATHER
    StringView weather;
#endif
    StringView windowManager;
  };

  extern const Icons ICON_TYPE;

#if DRAC_ENABLE_WEATHER
  /**
   * @brief Creates the main UI element based on system data and configuration.
   * @param config The application configuration.
   * @param data The collected system data.
   * @param weather The weather report, if enabled/available.
   * @return The root ftxui::Element for rendering.
   */
  fn CreateUI(const Config& config, const SystemInfo& data, Option<Report> weather) -> Element;
#else
  /**
   * @brief Creates the main UI element based on system data and configuration.
   * @param config The application configuration.
   * @param data The collected system data. @return The root ftxui::Element for rendering.
   */
  fn CreateUI(const Config& config, const SystemInfo& data) -> Element;
#endif // DRAC_ENABLE_WEATHER
} // namespace draconis::ui
