#ifdef __linux__

#include <utility>

#include "display_guards.h"

#include "src/util/macros.h"

namespace os::linux {
  XorgDisplayGuard::XorgDisplayGuard(const CStr name) : m_Connection(xcb_connect(name, nullptr)) {}

  XorgDisplayGuard::~XorgDisplayGuard() {
    if (m_Connection)
      xcb_disconnect(m_Connection);
  }

  XorgDisplayGuard::XorgDisplayGuard(XorgDisplayGuard&& other) noexcept
    : m_Connection(std::exchange(other.m_Connection, nullptr)) {}

  fn XorgDisplayGuard::operator=(XorgDisplayGuard&& other) noexcept -> XorgDisplayGuard& {
    if (this != &other) {
      if (m_Connection)
        xcb_disconnect(m_Connection);
      m_Connection = std::exchange(other.m_Connection, nullptr);
    }
    return *this;
  }

  XorgDisplayGuard::operator bool() const { return m_Connection && !xcb_connection_has_error(m_Connection); }

  fn XorgDisplayGuard::get() const -> xcb_connection_t* { return m_Connection; }

  fn XorgDisplayGuard::setup() const -> const xcb_setup_t* {
    return m_Connection ? xcb_get_setup(m_Connection) : nullptr;
  }

  fn XorgDisplayGuard::rootScreen() const -> xcb_screen_t* {
    const xcb_setup_t* setup = this->setup();
    return setup ? xcb_setup_roots_iterator(setup).data : nullptr;
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
