#pragma once

#if defined(__linux__) || defined(__FreeBSD__) || defined(__DragonFly__) || defined(__NetBSD__)

// clang-format off
#include <xcb/xcb.h> // XCB library

#include "Util/Definitions.hpp"
#include "Util/Types.hpp"
// clang-format on

namespace XCB {
  using util::types::u8, util::types::u16, util::types::i32, util::types::u32, util::types::CStr, util::types::None;

  using Connection = xcb_connection_t;
  using Setup      = xcb_setup_t;
  using Screen     = xcb_screen_t;
  using Window     = xcb_window_t;
  using Atom       = xcb_atom_t;

  using GenericError  = xcb_generic_error_t;
  using IntAtomCookie = xcb_intern_atom_cookie_t;
  using IntAtomReply  = xcb_intern_atom_reply_t;
  using GetPropCookie = xcb_get_property_cookie_t;
  using GetPropReply  = xcb_get_property_reply_t;

  constexpr Atom ATOM_WINDOW = XCB_ATOM_WINDOW; ///< Window atom

  /**
   * @brief Enum representing different types of connection errors
   *
   * This enum defines the possible types of errors that can occur when
   * establishing or maintaining an XCB connection. Each error type
   * corresponds to a specific error code defined in the XCB library.
   */
  enum ConnError : u8 {
    Generic         = XCB_CONN_ERROR,                   ///< Generic connection error
    ExtNotSupported = XCB_CONN_CLOSED_EXT_NOTSUPPORTED, ///< Extension not supported
    MemInsufficient = XCB_CONN_CLOSED_MEM_INSUFFICIENT, ///< Memory insufficient
    ReqLenExceed    = XCB_CONN_CLOSED_REQ_LEN_EXCEED,   ///< Request length exceed
    ParseErr        = XCB_CONN_CLOSED_PARSE_ERR,        ///< Parse error
    InvalidScreen   = XCB_CONN_CLOSED_INVALID_SCREEN,   ///< Invalid screen
    FdPassingFailed = XCB_CONN_CLOSED_FDPASSING_FAILED, ///< FD passing failed
  };

  /**
   * @brief Connect to an XCB display
   *
   * This function establishes a connection to an XCB display. It takes a
   * display name and a pointer to an integer that will store the screen
   * number.
   *
   * @param displayname The name of the display to connect to
   * @param screenp Pointer to an integer that will store the screen number
   * @return A pointer to the connection object
   */
  inline fn Connect(CStr displayname, i32* screenp) -> Connection* {
    return xcb_connect(displayname, screenp);
  }

  /**
   * @brief Disconnect from an XCB display
   *
   * This function disconnects from an XCB display. It takes a pointer to
   * the connection object.
   *
   * @param conn The connection object to disconnect from
   */
  inline fn Disconnect(Connection* conn) -> void {
    xcb_disconnect(conn);
  }

  /**
   * @brief Check if a connection has an error
   *
   * This function checks if a connection has an error. It takes a pointer
   * to the connection object.
   *
   * @param conn The connection object to check
   * @return 1 if the connection has an error, 0 otherwise
   */
  inline fn ConnectionHasError(Connection* conn) -> i32 {
    return xcb_connection_has_error(conn);
  }

  /**
   * @brief Intern an atom
   *
   * This function interns an atom. It takes a connection object, a flag
   *
   * @param conn The connection object to intern the atom on
   * @param only_if_exists The flag to check if the atom exists
   * @param name_len The length of the atom name
   * @param name The name of the atom
   * @return The cookie for the atom
   */
  inline fn InternAtom(Connection* conn, const u8 only_if_exists, const u16 name_len, CStr name) -> IntAtomCookie {
    return xcb_intern_atom(conn, only_if_exists, name_len, name);
  }

  /**
   * @brief Get the reply for an interned atom
   *
   * This function gets the reply for an interned atom. It takes a connection
   * object, a cookie, and a pointer to a generic error.
   *
   * @param conn The connection object
   * @param cookie The cookie for the atom
   * @param err The pointer to the generic error
   * @return The reply for the atom
   */
  inline fn InternAtomReply(Connection* conn, const IntAtomCookie cookie, GenericError** err) -> IntAtomReply* {
    return xcb_intern_atom_reply(conn, cookie, err);
  }

  /**
   * @brief Get a property
   *
   * This function gets a property. It takes a connection object, a flag,
   * a window, a property, a type, a long offset, and a long length.
   *
   * @param conn The connection object
   * @param _delete The flag
   * @param window The window
   * @param property The property
   * @param type The type
   */
  inline fn GetProperty(
    Connection*  conn,
    const u8     _delete,
    const Window window,
    const Atom   property,
    const Atom   type,
    const u32    long_offset,
    const u32    long_length
  ) -> GetPropCookie {
    return xcb_get_property(conn, _delete, window, property, type, long_offset, long_length);
  }

  /**
   * @brief Get the reply for a property
   *
   * This function gets the reply for a property. It takes a connection
   * object, a cookie, and a pointer to a generic error.
   *
   * @param conn The connection object
   * @param cookie The cookie for the property
   * @param err The pointer to the generic error
   * @return The reply for the property
   */
  inline fn GetPropertyReply(Connection* conn, const GetPropCookie cookie, GenericError** err) -> GetPropReply* {
    return xcb_get_property_reply(conn, cookie, err);
  }

  /**
   * @brief Get the value length for a property
   *
   * @param reply The reply for the property
   * @return The value length for the property
   */
  inline fn GetPropertyValueLength(const GetPropReply* reply) -> i32 {
    return xcb_get_property_value_length(reply);
  }

  /**
   * @brief Get the value for a property
   *
   * @param reply The reply for the property
   * @return The value for the property
   */
  inline fn GetPropertyValue(const GetPropReply* reply) -> void* {
    return xcb_get_property_value(reply);
  }

  /**
   * RAII wrapper for X11 Display connections
   * Automatically handles resource acquisition and cleanup
   */
  class DisplayGuard {
    Connection* m_connection = nullptr; ///< The connection to the display

   public:
    /**
     * Opens an XCB connection
     * @param name Display name (nullptr for default)
     */
    explicit DisplayGuard(const CStr name = nullptr)
      : m_connection(Connect(name, nullptr)) {}

    ~DisplayGuard() {
      if (m_connection)
        Disconnect(m_connection);
    }

    // Non-copyable
    DisplayGuard(const DisplayGuard&)                = delete;
    fn operator=(const DisplayGuard&)->DisplayGuard& = delete;

    // Movable
    DisplayGuard(DisplayGuard&& other) noexcept
      : m_connection(std::exchange(other.m_connection, nullptr)) {}

    /**
     * Move assignment operator
     * @param other The other display guard
     * @return The moved display guard
     */
    fn operator=(DisplayGuard&& other) noexcept -> DisplayGuard& {
      if (this != &other) {
        if (m_connection)
          Disconnect(m_connection);

        m_connection = std::exchange(other.m_connection, nullptr);
      }
      return *this;
    }

    /**
     * @brief Check if the display guard is valid
     * @return True if the display guard is valid, false otherwise
     */
    [[nodiscard]] explicit operator bool() const {
      return m_connection && !ConnectionHasError(m_connection);
    }

    /**
     * @brief Get the connection to the display
     * @return The connection to the display
     */
    [[nodiscard]] fn get() const -> Connection* {
      return m_connection;
    }

    /**
     * @brief Get the setup for the display
     * @return The setup for the display
     */
    [[nodiscard]] fn setup() const -> const Setup* {
      return m_connection ? xcb_get_setup(m_connection) : nullptr;
    }

    /**
     * @brief Get the root screen for the display
     * @return The root screen for the display
     */
    [[nodiscard]] fn rootScreen() const -> Screen* {
      const Setup* setup = this->setup();

      return setup ? xcb_setup_roots_iterator(setup).data : nullptr;
    }
  };

  /**
   * RAII wrapper for XCB replies
   * Handles automatic cleanup of various XCB reply objects
   */
  template <typename T>
  class ReplyGuard {
    T* m_reply = nullptr; ///< The reply to the XCB request

   public:
    /**
     * @brief Default constructor
     */
    ReplyGuard() = default;

    /**
     * @brief Constructor with a reply
     * @param reply The reply to the XCB request
     */
    explicit ReplyGuard(T* reply)
      : m_reply(reply) {}

    /**
     * @brief Destructor
     */
    ~ReplyGuard() {
      if (m_reply)
        free(m_reply);
    }

    // Non-copyable
    ReplyGuard(const ReplyGuard&)                = delete;
    fn operator=(const ReplyGuard&)->ReplyGuard& = delete;

    // Movable
    ReplyGuard(ReplyGuard&& other) noexcept
      : m_reply(std::exchange(other.m_reply, nullptr)) {}

    /**
     * @brief Move assignment operator
     * @param other The other reply guard
     * @return The moved reply guard
     */
    fn operator=(ReplyGuard&& other) noexcept -> ReplyGuard& {
      if (this != &other) {
        if (m_reply)
          free(m_reply);

        m_reply = std::exchange(other.m_reply, nullptr);
      }
      return *this;
    }

    /**
     * @brief Check if the reply guard is valid
     * @return True if the reply guard is valid, false otherwise
     */
    [[nodiscard]] explicit operator bool() const {
      return m_reply != nullptr;
    }

    /**
     * @brief Get the reply
     * @return The reply
     */
    [[nodiscard]] fn get() const -> T* {
      return m_reply;
    }

    /**
     * @brief Get the reply
     * @return The reply
     */
    [[nodiscard]] fn operator->() const->T* {
      return m_reply;
    }

    /**
     * @brief Dereference the reply
     * @return The reply
     */
    [[nodiscard]] fn operator*() const->T& {
      return *m_reply;
    }
  };
} // namespace XCB

#endif // __linux__ || __FreeBSD__ || __DragonFly__ || __NetBSD__
