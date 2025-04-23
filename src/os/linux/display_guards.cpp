#ifdef __linux__

#include <utility> // for std::exchange

#include "display_guards.h"

#include "src/util/macros.h"

namespace os::linux {
  DisplayGuard::DisplayGuard(CStr name) : m_Display(XOpenDisplay(name)) {}

  DisplayGuard::~DisplayGuard() {
    if (m_Display)
      XCloseDisplay(m_Display);
  }

  DisplayGuard::DisplayGuard(DisplayGuard&& other) noexcept : m_Display(std::exchange(other.m_Display, nullptr)) {}

  fn DisplayGuard::operator=(DisplayGuard&& other) noexcept -> DisplayGuard& {
    if (this != &other) {
      if (m_Display)
        XCloseDisplay(m_Display);

      m_Display = std::exchange(other.m_Display, nullptr);
    }

    return *this;
  }

  DisplayGuard::operator bool() const { return m_Display != nullptr; }

  fn DisplayGuard::get() const -> Display* { return m_Display; }

  fn DisplayGuard::defaultRootWindow() const -> Window {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
    return DefaultRootWindow(m_Display);
#pragma clang diagnostic pop
  }

  WaylandDisplayGuard::WaylandDisplayGuard() : m_Display(wl_display_connect(nullptr)) {}

  WaylandDisplayGuard::~WaylandDisplayGuard() {
    if (m_Display)
      wl_display_disconnect(m_Display);
  }

  WaylandDisplayGuard::WaylandDisplayGuard(WaylandDisplayGuard&& other) noexcept
    : m_Display(std::exchange(other.m_Display, nullptr)) {}

  fn WaylandDisplayGuard::operator=(WaylandDisplayGuard&& other) noexcept -> WaylandDisplayGuard& {
    if (this != &other) {
      if (m_Display)
        wl_display_disconnect(m_Display);

      m_Display = std::exchange(other.m_Display, nullptr);
    }

    return *this;
  }

  WaylandDisplayGuard::operator bool() const { return m_Display != nullptr; }

  fn WaylandDisplayGuard::get() const -> wl_display* { return m_Display; }

  fn WaylandDisplayGuard::fd() const -> i32 { return m_Display ? wl_display_get_fd(m_Display) : -1; }
}

#endif
