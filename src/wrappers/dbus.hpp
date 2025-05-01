#pragma once

#include <cstring>
#ifdef __linux__

// clang-format off
#include <dbus/dbus.h> // DBus Library
#include <utility>     // std::exchange, std::forward
#include <format>      // std::format
#include <type_traits> // std::is_convertible_v

#include "src/core/util/defs.hpp"
#include "src/core/util/error.hpp"
#include "src/core/util/types.hpp"
// clang-format on

namespace dbus {
  using util::error::DracError, util::error::DracErrorCode;
  using util::types::Option, util::types::Result, util::types::Err, util::types::String, util::types::i32,
    util::types::u32, util::types::None;

  /**
   * @brief RAII wrapper for DBusError. Automatically initializes and frees.
   */
  class Error {
    DBusError m_err {};
    bool      m_isInitialized = false;

   public:
    Error() : m_isInitialized(true) { dbus_error_init(&m_err); }

    ~Error() {
      if (m_isInitialized)
        dbus_error_free(&m_err);
    }

    Error(const Error&)                = delete;
    fn operator=(const Error&)->Error& = delete;

    Error(Error&& other) noexcept : m_err(other.m_err), m_isInitialized(other.m_isInitialized) {
      other.m_isInitialized = false;
      dbus_error_init(&other.m_err);
    }

    fn operator=(Error&& other) noexcept -> Error& {
      if (this != &other) {
        if (m_isInitialized)
          dbus_error_free(&m_err);

        m_err           = other.m_err;
        m_isInitialized = other.m_isInitialized;

        other.m_isInitialized = false;
        dbus_error_init(&other.m_err);
      }
      return *this;
    }

    /**
     * @brief Checks if the D-Bus error is set.
     * @return True if an error is set, false otherwise.
     */
    [[nodiscard]] fn isSet() const -> bool { return m_isInitialized && dbus_error_is_set(&m_err); }

    /**
     * @brief Gets the error message.
     * @return The error message string, or "" if not set or not initialized.
     */
    [[nodiscard]] fn message() const -> const char* { return isSet() ? m_err.message : ""; }

    /**
     * @brief Gets the error name.
     * @return The error name string (e.g., "org.freedesktop.DBus.Error.Failed"), or "" if not set or not initialized.
     */
    [[nodiscard]] fn name() const -> const char* { return isSet() ? m_err.name : ""; }

    /**
     * @brief Gets a pointer to the underlying DBusError. Use with caution.
     * @return Pointer to the DBusError struct.
     */
    [[nodiscard]] fn get() -> DBusError* { return &m_err; }
    /**
     * @brief Gets a const pointer to the underlying DBusError.
     * @return Const pointer to the DBusError struct.
     */
    [[nodiscard]] fn get() const -> const DBusError* { return &m_err; }

    /**
     * @brief Converts the D-Bus error to a DraconisError.
     * @param code The DraconisError code to use if the D-Bus error is set.
     * @return A DraconisError representing the D-Bus error, or an internal error if called when no D-Bus error is set.
     */
    [[nodiscard]] fn toDraconisError(const DracErrorCode code = DracErrorCode::PlatformSpecific) const -> DracError {
      if (isSet())
        return { code, std::format("D-Bus Error: {} ({})", message(), name()) };

      return { DracErrorCode::InternalError, "Attempted to convert non-set ErrorGuard" };
    }
  };

  /**
   * @brief RAII wrapper for DBusMessageIter. Encapsulates iterator operations.
   * Note: This wrapper does *not* own the message, only the iterator state.
   * It's designed to be used within the scope where the MessageGuard is valid.
   */
  class MessageIter {
    DBusMessageIter m_iter {};
    bool            m_isValid = false;

    explicit MessageIter(const DBusMessageIter& iter, const bool isValid) : m_iter(iter), m_isValid(isValid) {}

    friend class Message;

    /**
     * @brief Gets the value of a basic-typed argument.
     * Unsafe: Caller must ensure 'value' points to memory suitable for the actual argument type.
     * @param value Pointer to store the retrieved value.
     */
    fn getBasic(void* value) -> void {
      if (m_isValid)
        dbus_message_iter_get_basic(&m_iter, value);
    }

   public:
    MessageIter(const MessageIter&)                = delete;
    fn operator=(const MessageIter&)->MessageIter& = delete;
    MessageIter(MessageIter&&)                     = delete;
    fn operator=(MessageIter&&)->MessageIter&      = delete;
    ~MessageIter()                                 = default;

    /**
     * @brief Checks if the iterator is validly initialized.
     */
    [[nodiscard]] fn isValid() const -> bool { return m_isValid; }

    /**
     * @brief Gets the D-Bus type code of the current argument.
     * @return The D-Bus type code, or DBUS_TYPE_INVALID otherwise.
     */
    [[nodiscard]] fn getArgType() -> int {
      return m_isValid ? dbus_message_iter_get_arg_type(&m_iter) : DBUS_TYPE_INVALID;
    }

    /**
     * @brief Gets the element type of the container pointed to by the iterator.
     * Only valid if the iterator points to an ARRAY or VARIANT.
     * @return The D-Bus type code of the elements, or DBUS_TYPE_INVALID otherwise.
     */
    [[nodiscard]] fn getElementType() -> int {
      return m_isValid ? dbus_message_iter_get_element_type(&m_iter) : DBUS_TYPE_INVALID;
    }

    /**
     * @brief Advances the iterator to the next argument.
     * @return True if successful (moved to a next element), false if at the end or iterator is invalid.
     */
    fn next() -> bool { return m_isValid && dbus_message_iter_next(&m_iter); }

    /**
     * @brief Recurses into a container-type argument (e.g., array, struct, variant).
     * @return A new MessageIterGuard for the sub-container. The returned iterator might be invalid
     * if the current element is not a container or the main iterator is invalid.
     */
    [[nodiscard]] fn recurse() -> MessageIter {
      if (!m_isValid)
        return MessageIter({}, false);

      DBusMessageIter subIter;
      dbus_message_iter_recurse(&m_iter, &subIter);

      return MessageIter(subIter, true);
    }

    /**
     * @brief Helper to safely get a string argument from the iterator.
     * @return An Option containing the string value if the current arg is a valid string, or None otherwise.
     */
    [[nodiscard]] fn getString() -> Option<String> {
      if (m_isValid && getArgType() == DBUS_TYPE_STRING) {
        const char* strPtr = nullptr;

        // ReSharper disable once CppRedundantCastExpression
        getBasic(static_cast<void*>(&strPtr));

        if (strPtr)
          return String(strPtr);
      }

      return None;
    }
  };

  /**
   * @brief RAII wrapper for DBusMessage. Automatically unrefs.
   */
  class Message {
    DBusMessage* m_msg = nullptr;

   public:
    explicit Message(DBusMessage* msg = nullptr) : m_msg(msg) {}

    ~Message() {
      if (m_msg)
        dbus_message_unref(m_msg);
    }

    Message(const Message&)                = delete;
    fn operator=(const Message&)->Message& = delete;

    Message(Message&& other) noexcept : m_msg(std::exchange(other.m_msg, nullptr)) {}

    fn operator=(Message&& other) noexcept -> Message& {
      if (this != &other) {
        if (m_msg)
          dbus_message_unref(m_msg);
        m_msg = std::exchange(other.m_msg, nullptr);
      }
      return *this;
    }

    /**
     * @brief Gets the underlying DBusMessage pointer. Use with caution.
     * @return The raw DBusMessage pointer, or nullptr if not holding a message.
     */
    [[nodiscard]] fn get() const -> DBusMessage* { return m_msg; }

    /**
     * @brief Initializes a message iterator for reading arguments from this message.
     * @return A MessageIterGuard. Check iter.isValid() before use.
     */
    [[nodiscard]] fn iterInit() const -> MessageIter {
      if (!m_msg)
        return MessageIter({}, false);

      DBusMessageIter iter;
      const bool      isValid = dbus_message_iter_init(m_msg, &iter);
      return MessageIter(iter, isValid);
    }

    /**
     * @brief Appends arguments of basic types to the message.
     * @tparam Args Types of the arguments to append.
     * @param args The arguments to append.
     * @return True if all arguments were appended successfully, false otherwise (e.g., allocation error).
     */
    template <typename... Args>
    [[nodiscard]] fn appendArgs(Args&&... args) -> bool {
      if (!m_msg)
        return false;

      DBusMessageIter iter;
      dbus_message_iter_init_append(m_msg, &iter);

      bool success = true;
      ((success = success && appendArgInternal(iter, std::forward<Args>(args))), ...); // NOLINT
      return success;
    }

    /**
     * @brief Creates a new D-Bus method call message.
     * @param destination Service name (e.g., "org.freedesktop.Notifications"). Can be null.
     * @param path Object path (e.g., "/org/freedesktop/Notifications"). Must not be null.
     * @param interface Interface name (e.g., "org.freedesktop.Notifications"). Can be null.
     * @param method Method name (e.g., "Notify"). Must not be null.
     * @return Result containing a MessageGuard on success, or DraconisError on failure.
     */
    static fn newMethodCall(const char* destination, const char* path, const char* interface, const char* method)
      -> Result<Message, DracError> {
      DBusMessage* rawMsg = dbus_message_new_method_call(destination, path, interface, method);

      if (!rawMsg)
        return Err(DracError(DracErrorCode::OutOfMemory, "dbus_message_new_method_call failed (allocation failed?)"));

      return Message(rawMsg);
    }

   private:
    template <typename T>
    fn appendArgInternal(DBusMessageIter& iter, T&& arg) -> bool {
      using DecayedT = std::decay_t<T>;

      if constexpr (std::is_convertible_v<DecayedT, const char*>) {
        const char* valuePtr = static_cast<const char*>(arg);
        return dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &valuePtr);
      } else {
        static_assert(!sizeof(T*), "Unsupported type passed to appendArgs");
        return false;
      }
    }
  };

  /**
   * @brief RAII wrapper for DBusConnection. Automatically unrefs.
   */
  class Connection {
    DBusConnection* m_conn = nullptr;

   public:
    explicit Connection(DBusConnection* conn = nullptr) : m_conn(conn) {}

    ~Connection() {
      if (m_conn)
        dbus_connection_unref(m_conn);
    }

    Connection(const Connection&)                = delete;
    fn operator=(const Connection&)->Connection& = delete;

    Connection(Connection&& other) noexcept : m_conn(std::exchange(other.m_conn, nullptr)) {}

    fn operator=(Connection&& other) noexcept -> Connection& {
      if (this != &other) {
        if (m_conn)
          dbus_connection_unref(m_conn);

        m_conn = std::exchange(other.m_conn, nullptr);
      }
      return *this;
    }

    /**
     * @brief Gets the underlying DBusConnection pointer. Use with caution.
     * @return The raw DBusConnection pointer, or nullptr if not holding a connection.
     */
    [[nodiscard]] fn get() const -> DBusConnection* { return m_conn; }

    /**
     * @brief Sends a message and waits for a reply, blocking execution.
     * @param message The D-Bus message guard to send.
     * @param timeout_milliseconds Timeout duration in milliseconds.
     * @return Result containing the reply MessageGuard on success, or DraconisError on failure.
     */
    [[nodiscard]] fn sendWithReplyAndBlock(const Message& message, const i32 timeout_milliseconds = 1000) const
      -> Result<Message, DracError> {
      if (!m_conn || !message.get())
        return Err(
          DracError(DracErrorCode::InvalidArgument, "Invalid connection or message provided to sendWithReplyAndBlock")
        );

      Error        err;
      DBusMessage* rawReply =
        dbus_connection_send_with_reply_and_block(m_conn, message.get(), timeout_milliseconds, err.get());

      if (err.isSet()) {
        if (const char* errName = err.name()) {
          if (strcmp(errName, DBUS_ERROR_TIMEOUT) == 0 || strcmp(errName, DBUS_ERROR_NO_REPLY) == 0)
            return Err(err.toDraconisError(DracErrorCode::Timeout));

          if (strcmp(errName, DBUS_ERROR_SERVICE_UNKNOWN) == 0)
            return Err(err.toDraconisError(DracErrorCode::NotFound));

          if (strcmp(errName, DBUS_ERROR_ACCESS_DENIED) == 0)
            return Err(err.toDraconisError(DracErrorCode::PermissionDenied));
        }

        return Err(err.toDraconisError(DracErrorCode::PlatformSpecific));
      }

      if (!rawReply)
        return Err(DracError(
          DracErrorCode::ApiUnavailable,
          "dbus_connection_send_with_reply_and_block returned null without setting error (likely timeout or "
          "disconnected)"
        ));

      return Message(rawReply);
    }

    /**
     * @brief Connects to a D-Bus bus type (Session or System).
     * @param bus_type The type of bus (DBUS_BUS_SESSION or DBUS_BUS_SYSTEM).
     * @return Result containing a ConnectionGuard on success, or DraconisError on failure.
     */
    static fn busGet(const DBusBusType bus_type) -> Result<Connection, DracError> {
      Error           err;
      DBusConnection* rawConn = dbus_bus_get(bus_type, err.get());

      if (err.isSet())
        return Err(err.toDraconisError(DracErrorCode::ApiUnavailable));

      if (!rawConn)
        return Err(DracError(DracErrorCode::ApiUnavailable, "dbus_bus_get returned null without setting error"));

      return Connection(rawConn);
    }
  };
} // namespace dbus

#endif // __linux__
