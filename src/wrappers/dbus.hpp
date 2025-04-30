#pragma once

#ifdef __linux__

// clang-format off
#include <dbus/dbus.h> // DBus Library
#include <utility>     // std::exchange
#include <format>      // std::format
#include <cstdarg>     // va_list, va_start, va_end

#include "src/core/util/defs.hpp"
#include "src/core/util/error.hpp"
#include "src/core/util/types.hpp"
// clang-format on

namespace dbus {
  using util::error::DraconisError, util::error::DraconisErrorCode;
  using util::types::Option, util::types::Result, util::types::Err, util::types::String, util::types::i32,
    util::types::None;

  /**
   * @brief RAII wrapper for DBusError. Automatically initializes and frees.
   */
  class ErrorGuard {
    DBusError m_Err {};
    bool      m_IsInitialized = false;

   public:
    ErrorGuard() : m_IsInitialized(true) { dbus_error_init(&m_Err); }

    ~ErrorGuard() {
      if (m_IsInitialized)
        dbus_error_free(&m_Err);
    }

    ErrorGuard(const ErrorGuard&)                = delete;
    fn operator=(const ErrorGuard&)->ErrorGuard& = delete;

    ErrorGuard(ErrorGuard&& other) noexcept : m_Err(other.m_Err), m_IsInitialized(other.m_IsInitialized) {
      other.m_IsInitialized = false;
      dbus_error_init(&other.m_Err);
    }

    fn operator=(ErrorGuard&& other) noexcept -> ErrorGuard& {
      if (this != &other) {
        if (m_IsInitialized) {
          dbus_error_free(&m_Err);
        }
        m_Err           = other.m_Err;
        m_IsInitialized = other.m_IsInitialized;

        other.m_IsInitialized = false;
        dbus_error_init(&other.m_Err);
      }
      return *this;
    }

    [[nodiscard]] fn isSet() const -> bool { return m_IsInitialized && dbus_error_is_set(&m_Err); }

    [[nodiscard]] fn message() const -> const char* { return isSet() ? m_Err.message : ""; }

    [[nodiscard]] fn name() const -> const char* { return isSet() ? m_Err.name : ""; }

    [[nodiscard]] fn get() -> DBusError* { return &m_Err; }
    [[nodiscard]] fn get() const -> const DBusError* { return &m_Err; }

    [[nodiscard]] fn toDraconisError(const DraconisErrorCode code = DraconisErrorCode::PlatformSpecific) const
      -> DraconisError {
      if (isSet())
        return { code, std::format("D-Bus Error: {} ({})", message(), name()) };

      return { DraconisErrorCode::InternalError, "Attempted to convert non-set DBusErrorGuard" };
    }
  };

  /**
   * @brief RAII wrapper for DBusConnection. Automatically unrefs.
   */
  class ConnectionGuard {
    DBusConnection* m_Conn = nullptr;

   public:
    explicit ConnectionGuard(DBusConnection* conn = nullptr) : m_Conn(conn) {}

    ~ConnectionGuard() {
      if (m_Conn)
        dbus_connection_unref(m_Conn);
    }

    ConnectionGuard(const ConnectionGuard&)                = delete;
    fn operator=(const ConnectionGuard&)->ConnectionGuard& = delete;

    ConnectionGuard(ConnectionGuard&& other) noexcept : m_Conn(std::exchange(other.m_Conn, nullptr)) {}
    fn operator=(ConnectionGuard&& other) noexcept -> ConnectionGuard& {
      if (this != &other) {
        if (m_Conn)
          dbus_connection_unref(m_Conn);
        m_Conn = std::exchange(other.m_Conn, nullptr);
      }

      return *this;
    }

    [[nodiscard]] fn get() const -> DBusConnection* { return m_Conn; }
    explicit         operator bool() const { return m_Conn != nullptr; }

    [[nodiscard]] fn release() -> DBusConnection* { return std::exchange(m_Conn, nullptr); }
  };

  /**
   * @brief RAII wrapper for DBusMessage. Automatically unrefs.
   */
  class MessageGuard {
    DBusMessage* m_Msg = nullptr;

   public:
    explicit MessageGuard(DBusMessage* msg = nullptr) : m_Msg(msg) {}

    ~MessageGuard() {
      if (m_Msg)
        dbus_message_unref(m_Msg);
    }

    MessageGuard(const MessageGuard&)                = delete;
    fn operator=(const MessageGuard&)->MessageGuard& = delete;

    MessageGuard(MessageGuard&& other) noexcept : m_Msg(std::exchange(other.m_Msg, nullptr)) {}
    fn operator=(MessageGuard&& other) noexcept -> MessageGuard& {
      if (this != &other) {
        if (m_Msg)
          dbus_message_unref(m_Msg);
        m_Msg = std::exchange(other.m_Msg, nullptr);
      }

      return *this;
    }

    [[nodiscard]] fn get() const -> DBusMessage* { return m_Msg; }
    explicit         operator bool() const { return m_Msg != nullptr; }

    [[nodiscard]] fn release() -> DBusMessage* { return std::exchange(m_Msg, nullptr); }
  };

  /**
   * @brief Connects to a D-Bus bus type.
   * @param bus_type The type of bus (e.g., DBUS_BUS_SESSION, DBUS_BUS_SYSTEM).
   * @return Result containing a DBusConnectionGuard on success, or DraconisError on failure.
   */
  inline fn BusGet(const DBusBusType bus_type) -> Result<ConnectionGuard, DraconisError> {
    ErrorGuard      err;
    DBusConnection* rawConn = dbus_bus_get(bus_type, err.get());

    if (err.isSet())
      return Err(err.toDraconisError(DraconisErrorCode::ApiUnavailable));

    if (!rawConn)
      return Err(DraconisError(DraconisErrorCode::ApiUnavailable, "dbus_bus_get returned null without setting error"));

    return ConnectionGuard(rawConn);
  }

  /**
   * @brief Creates a new D-Bus method call message.
   * @param destination Service name (e.g., "org.freedesktop.Notifications").
   * @param path Object path (e.g., "/org/freedesktop/Notifications").
   * @param interface Interface name (e.g., "org.freedesktop.Notifications").
   * @param method Method name (e.g., "Notify").
   * @return Result containing a DBusMessageGuard on success, or DraconisError on failure.
   */
  inline fn MessageNewMethodCall(const char* destination, const char* path, const char* interface, const char* method)
    -> Result<MessageGuard, DraconisError> {
    DBusMessage* rawMsg = dbus_message_new_method_call(destination, path, interface, method);
    if (!rawMsg)
      return Err(DraconisError(DraconisErrorCode::InternalError, "dbus_message_new_method_call failed (allocation?)"));

    return MessageGuard(rawMsg);
  }

  /**
   * @brief Sends a message and waits for a reply.
   * @param connection The D-Bus connection guard.
   * @param message The D-Bus message guard to send.
   * @param timeout_milliseconds Timeout duration.
   * @return Result containing the reply DBusMessageGuard on success, or DraconisError on failure.
   */
  inline fn ConnectionSendWithReplyAndBlock(
    const ConnectionGuard& connection,
    const MessageGuard&    message,
    const i32              timeout_milliseconds = 100
  ) -> Result<MessageGuard, DraconisError> {
    ErrorGuard   err;
    DBusMessage* rawReply =
      dbus_connection_send_with_reply_and_block(connection.get(), message.get(), timeout_milliseconds, err.get());

    if (err.isSet())
      return Err(err.toDraconisError(DraconisErrorCode::ApiUnavailable));

    if (!rawReply)
      return Err(DraconisError(
        DraconisErrorCode::ApiUnavailable,
        "dbus_connection_send_with_reply_and_block returned null without setting error"
      ));

    return MessageGuard(rawReply);
  }

  /**
   * @brief Appends arguments to a D-Bus message using varargs.
   * @param message The message guard.
   * @param first_arg_type The D-Bus type code of the first argument.
   * @param ... Subsequent arguments (type code, value pointer, type code, value pointer...).
   * End with DBUS_TYPE_INVALID.
   * @return True on success, false on failure (e.g., allocation error).
   */
  inline fn MessageAppendArgs(const MessageGuard& message, const int first_arg_type, ...) -> bool {
    va_list args;
    va_start(args, first_arg_type);
    const bool result = dbus_message_append_args_valist(message.get(), first_arg_type, args);
    va_end(args);
    return result;
  }

  using MessageIter = DBusMessageIter;

  /**
   * @brief Initializes a message iterator.
   * @param message The message guard.
   * @param iter Pointer to the iterator to initialize.
   * @return True if iterator is valid, false otherwise.
   */
  inline fn MessageIterInit(const MessageGuard& message, MessageIter* iter) -> bool {
    return dbus_message_iter_init(message.get(), iter);
  }

  /**
   * @brief Recurses into a container-type argument.
   * @param iter The current iterator.
   * @param sub_iter Pointer to the sub-iterator to initialize.
   */
  inline fn MessageIterRecurse(MessageIter* iter, MessageIter* sub_iter) -> void {
    dbus_message_iter_recurse(iter, sub_iter);
  }

  /**
   * @brief Gets the D-Bus type code of the current argument.
   * @param iter The iterator.
   * @return The type code (e.g., DBUS_TYPE_STRING, DBUS_TYPE_INVALID).
   */
  inline fn MessageIterGetArgType(MessageIter* iter) -> int { return dbus_message_iter_get_arg_type(iter); }

  /**
   * @brief Gets the value of a basic-typed argument.
   * @param iter The iterator.
   * @param value Pointer to store the retrieved value. Must match the argument type.
   */
  inline fn MessageIterGetBasic(MessageIter* iter, void* value) -> void { dbus_message_iter_get_basic(iter, value); }

  /**
   * @brief Advances the iterator to the next argument.
   * @param iter The iterator.
   * @return True if successful, false if at the end.
   */
  inline fn MessageIterNext(MessageIter* iter) -> bool { return dbus_message_iter_next(iter); }

  /**
   * @brief Helper to safely get a string argument from an iterator.
   * @param iter The iterator positioned at a DBUS_TYPE_STRING argument.
   * @return An Option containing the string value, or None if the type is wrong or value is null.
   */
  inline fn MessageIterGetString(MessageIter* iter) -> Option<String> {
    if (MessageIterGetArgType(iter) == DBUS_TYPE_STRING) {
      const char* strPtr = nullptr;
      MessageIterGetBasic(iter, static_cast<void*>(&strPtr));
      if (strPtr)
        return String(strPtr);
    }

    return None;
  }
} // namespace dbus

#endif // __linux__
