#pragma once

#ifdef __linux__

// clang-format off
#include <xcb/xcb.h> // XCB library

#include "src/core/util/defs.hpp"
#include "src/core/util/types.hpp"
// clang-format on

namespace xcb {
  using util::types::u8, util::types::i32, util::types::CStr, util::types::None;

  using connection_t = xcb_connection_t;
  using setup_t      = xcb_setup_t;
  using screen_t     = xcb_screen_t;
  using window_t     = xcb_window_t;
  using atom_t       = xcb_atom_t;

  using generic_error_t       = xcb_generic_error_t;
  using intern_atom_cookie_t  = xcb_intern_atom_cookie_t;
  using intern_atom_reply_t   = xcb_intern_atom_reply_t;
  using get_property_cookie_t = xcb_get_property_cookie_t;
  using get_property_reply_t  = xcb_get_property_reply_t;

  constexpr atom_t ATOM_WINDOW = XCB_ATOM_WINDOW;

  enum ConnError : u8 {
    Generic         = XCB_CONN_ERROR,
    ExtNotSupported = XCB_CONN_CLOSED_EXT_NOTSUPPORTED,
    MemInsufficient = XCB_CONN_CLOSED_MEM_INSUFFICIENT,
    ReqLenExceed    = XCB_CONN_CLOSED_REQ_LEN_EXCEED,
    ParseErr        = XCB_CONN_CLOSED_PARSE_ERR,
    InvalidScreen   = XCB_CONN_CLOSED_INVALID_SCREEN,
    FdPassingFailed = XCB_CONN_CLOSED_FDPASSING_FAILED
  };

  // NOLINTBEGIN(readability-identifier-naming)
  inline fn getConnError(const util::types::i32 code) -> util::types::Option<ConnError> {
    switch (code) {
      case XCB_CONN_ERROR:                   return Generic;
      case XCB_CONN_CLOSED_EXT_NOTSUPPORTED: return ExtNotSupported;
      case XCB_CONN_CLOSED_MEM_INSUFFICIENT: return MemInsufficient;
      case XCB_CONN_CLOSED_REQ_LEN_EXCEED:   return ReqLenExceed;
      case XCB_CONN_CLOSED_PARSE_ERR:        return ParseErr;
      case XCB_CONN_CLOSED_INVALID_SCREEN:   return InvalidScreen;
      case XCB_CONN_CLOSED_FDPASSING_FAILED: return FdPassingFailed;
      default:                               return None;
    }
  }

  inline fn connect(const char* displayname, int* screenp) -> connection_t* {
    return xcb_connect(displayname, screenp);
  }
  inline fn disconnect(connection_t* conn) -> void { xcb_disconnect(conn); }
  inline fn connection_has_error(connection_t* conn) -> int { return xcb_connection_has_error(conn); }
  inline fn intern_atom(connection_t* conn, const uint8_t only_if_exists, const uint16_t name_len, const char* name)
    -> intern_atom_cookie_t {
    return xcb_intern_atom(conn, only_if_exists, name_len, name);
  }
  inline fn intern_atom_reply(connection_t* conn, const intern_atom_cookie_t cookie, generic_error_t** err)
    -> intern_atom_reply_t* {
    return xcb_intern_atom_reply(conn, cookie, err);
  }
  inline fn get_property(
    connection_t*  conn,
    const uint8_t  _delete,
    const window_t window,
    const atom_t   property,
    const atom_t   type,
    const uint32_t long_offset,
    const uint32_t long_length
  ) -> get_property_cookie_t {
    return xcb_get_property(conn, _delete, window, property, type, long_offset, long_length);
  }
  inline fn get_property_reply(connection_t* conn, const get_property_cookie_t cookie, generic_error_t** err)
    -> get_property_reply_t* {
    return xcb_get_property_reply(conn, cookie, err);
  }
  inline fn get_property_value_length(const get_property_reply_t* reply) -> int {
    return xcb_get_property_value_length(reply);
  }
  inline fn get_property_value(const get_property_reply_t* reply) -> void* { return xcb_get_property_value(reply); }
  // NOLINTEND(readability-identifier-naming)

  /**
   * RAII wrapper for X11 Display connections
   * Automatically handles resource acquisition and cleanup
   */
  class DisplayGuard {
    connection_t* m_Connection = nullptr;

   public:
    /**
     * Opens an XCB connection
     * @param name Display name (nullptr for default)
     */
    explicit DisplayGuard(const util::types::CStr name = nullptr) : m_Connection(connect(name, nullptr)) {}
    ~DisplayGuard() {
      if (m_Connection)
        disconnect(m_Connection);
    }

    // Non-copyable
    DisplayGuard(const DisplayGuard&)                = delete;
    fn operator=(const DisplayGuard&)->DisplayGuard& = delete;

    // Movable
    DisplayGuard(DisplayGuard&& other) noexcept : m_Connection(std::exchange(other.m_Connection, nullptr)) {}
    fn operator=(DisplayGuard&& other) noexcept -> DisplayGuard& {
      if (this != &other) {
        if (m_Connection)
          disconnect(m_Connection);

        m_Connection = std::exchange(other.m_Connection, nullptr);
      }
      return *this;
    }

    [[nodiscard]] explicit operator bool() const { return m_Connection && !connection_has_error(m_Connection); }

    [[nodiscard]] fn get() const -> connection_t* { return m_Connection; }

    [[nodiscard]] fn setup() const -> const setup_t* { return m_Connection ? xcb_get_setup(m_Connection) : nullptr; }

    [[nodiscard]] fn rootScreen() const -> screen_t* {
      const setup_t* setup = this->setup();
      return setup ? xcb_setup_roots_iterator(setup).data : nullptr;
    }
  };

  /**
   * RAII wrapper for XCB replies
   * Handles automatic cleanup of various XCB reply objects
   */
  template <typename T>
  class ReplyGuard {
    T* m_Reply = nullptr;

   public:
    ReplyGuard() = default;
    explicit ReplyGuard(T* reply) : m_Reply(reply) {}

    ~ReplyGuard() {
      if (m_Reply)
        free(m_Reply);
    }

    // Non-copyable
    ReplyGuard(const ReplyGuard&)                = delete;
    fn operator=(const ReplyGuard&)->ReplyGuard& = delete;

    // Movable
    ReplyGuard(ReplyGuard&& other) noexcept : m_Reply(std::exchange(other.m_Reply, nullptr)) {}
    fn operator=(ReplyGuard&& other) noexcept -> ReplyGuard& {
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
} // namespace xcb

#endif // __linux__
