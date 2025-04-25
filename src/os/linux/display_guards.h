#pragma once

#ifdef __linux__

#include <wayland-client.h>
#include <xcb/xcb.h>

#include "src/util/macros.h"

namespace os::linux {
  /**
   * RAII wrapper for X11 Display connections
   * Automatically handles resource acquisition and cleanup
   */
  class XorgDisplayGuard {
    xcb_connection_t* m_Connection = nullptr;

   public:
    /**
     * Opens an XCB connection
     * @param name Display name (nullptr for default)
     */
    explicit XorgDisplayGuard(CStr name = nullptr);
    ~XorgDisplayGuard();

    // Non-copyable
    XorgDisplayGuard(const XorgDisplayGuard&)                = delete;
    fn operator=(const XorgDisplayGuard&)->XorgDisplayGuard& = delete;

    // Movable
    XorgDisplayGuard(XorgDisplayGuard&& other) noexcept;
    fn operator=(XorgDisplayGuard&& other) noexcept -> XorgDisplayGuard&;

    [[nodiscard]] explicit operator bool() const;

    [[nodiscard]] fn get() const -> xcb_connection_t*;
    [[nodiscard]] fn setup() const -> const xcb_setup_t*;
    [[nodiscard]] fn rootScreen() const -> xcb_screen_t*;
  };

  /**
   * RAII wrapper for XCB replies
   * Handles automatic cleanup of various XCB reply objects
   */
  template <typename T>
  class XcbReplyGuard {
    T* m_Reply = nullptr;

   public:
    XcbReplyGuard() = default;
    explicit XcbReplyGuard(T* reply) : m_Reply(reply) {}

    ~XcbReplyGuard() {
      if (m_Reply)
        free(m_Reply);
    }

    // Non-copyable
    XcbReplyGuard(const XcbReplyGuard&)                = delete;
    fn operator=(const XcbReplyGuard&)->XcbReplyGuard& = delete;

    // Movable
    XcbReplyGuard(XcbReplyGuard&& other) noexcept : m_Reply(std::exchange(other.m_Reply, nullptr)) {}
    fn operator=(XcbReplyGuard&& other) noexcept -> XcbReplyGuard& {
      if (this != &other) {
        if (m_Reply)
          free(m_Reply);

        m_Reply = std::exchange(other.m_Reply, nullptr);
      }
      return *this;
    }

    [[nodiscard]] explicit operator bool() const { return m_Reply != nullptr; }

    [[nodiscard]] fn get() const -> T* { return m_Reply; }
    [[nodiscard]] fn operator->() const->T* { return m_Reply; }
    [[nodiscard]] fn operator*() const->T& { return *m_Reply; }
  };

  /**
   * RAII wrapper for Wayland display connections
   * Automatically handles resource acquisition and cleanup
   */
  class WaylandDisplayGuard {
    wl_display* m_Display;

   public:
    /**
     * Opens a Wayland display connection
     */
    WaylandDisplayGuard();
    ~WaylandDisplayGuard();

    // Non-copyable
    WaylandDisplayGuard(const WaylandDisplayGuard&)                = delete;
    fn operator=(const WaylandDisplayGuard&)->WaylandDisplayGuard& = delete;

    // Movable
    WaylandDisplayGuard(WaylandDisplayGuard&& other) noexcept;
    fn operator=(WaylandDisplayGuard&& other) noexcept -> WaylandDisplayGuard&;

    [[nodiscard]] explicit operator bool() const;

    [[nodiscard]] fn get() const -> wl_display*;
    [[nodiscard]] fn fd() const -> i32;
  };
}

#endif
