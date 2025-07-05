#pragma once

#include <Drac++/Services/Weather.hpp>

#include <Drac++/Utils/Logging.hpp>
#include <Drac++/Utils/Types.hpp>

#include "Config/Config.hpp"
#include "Core/SystemInfo.hpp"

namespace draconis::ui {
  namespace {
    using config::Config;

    using core::system::SystemInfo;

    using services::weather::Report;

    using utils::logging::LogColor;
    using utils::types::Option;
    using utils::types::String;
    using utils::types::StringView;
  } // namespace

  struct Theme {
    LogColor icon;
    LogColor label;
    LogColor value;
    LogColor border;
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
    StringView uptime;
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
   * @return A string containing the formatted UI.
   */
  fn CreateUI(const Config& config, const SystemInfo& data, Option<Report> weather) -> String;
#else
  /**
   * @brief Creates the main UI element based on system data and configuration.
   * @param config The application configuration.
   * @param data The collected system data. @return A string containing the formatted UI.
   */
  fn CreateUI(const Config& config, const SystemInfo& data) -> String;
#endif // DRAC_ENABLE_WEATHER
} // namespace draconis::ui
