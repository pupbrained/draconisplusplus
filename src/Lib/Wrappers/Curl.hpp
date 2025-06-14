#pragma once

#include <DracUtils/Definitions.hpp>
#include <DracUtils/Error.hpp>
#include <DracUtils/Types.hpp>
#include <curl/curl.h>
#include <utility> // std::{exchange, move}

namespace Curl {
  /**
   * @brief Options for initializing a Curl::Easy handle.
   */
  struct EasyOptions {
    util::types::Option<util::types::String> url                = util::types::None; ///< URL to set for the transfer
    util::types::String*                     writeBuffer        = nullptr;           ///< Pointer to a string buffer to store the response
    util::types::Option<util::types::i64>    timeoutSecs        = util::types::None; ///< Timeout for the entire request in seconds
    util::types::Option<util::types::i64>    connectTimeoutSecs = util::types::None; ///< Timeout for the connection phase in seconds
    util::types::Option<util::types::String> userAgent          = util::types::None; ///< User-agent string
  };

  /**
   * @brief RAII wrapper for CURL easy handle.
   */
  class Easy {
    CURL*                                       m_curl      = nullptr;
    util::types::Option<util::error::DracError> m_initError = util::types::None; ///< Stores any error that occurred during initialization via options constructor

    static fn writeCallback(void* contents, util::types::usize size, util::types::usize nmemb, util::types::String* str) -> util::types::usize {
      const util::types::usize totalSize = size * nmemb;
      str->append(static_cast<char*>(contents), totalSize);
      return totalSize;
    }

   public:
    /**
     * @brief Default constructor. Initializes a CURL easy handle.
     */
    Easy() : m_curl(curl_easy_init()) {
      using util::types::Err, util::error::DracError, util::error::DracErrorCode;

      if (!m_curl)
        m_initError = DracError(DracErrorCode::ApiUnavailable, "curl_easy_init() failed");
    }

    /**
     * @brief Constructor with options. Initializes a CURL easy handle and sets options.
     * @param options The options to configure the CURL handle.
     */
    explicit Easy(const EasyOptions& options) : m_curl(curl_easy_init()) {
      using util::types::Err, util::types::Result, util::error::DracError, util::error::DracErrorCode;

      if (!m_curl) {
        m_initError = DracError(DracErrorCode::ApiUnavailable, "curl_easy_init() failed");
        return;
      }

      if (options.url)
        if (Result res = setUrl(*options.url); !res) {
          m_initError = res.error();
          return;
        }

      if (options.writeBuffer)
        if (Result res = setWriteFunction(options.writeBuffer); !res) {
          m_initError = res.error();
          return;
        }

      if (options.timeoutSecs)
        if (Result res = setTimeout(*options.timeoutSecs); !res) {
          m_initError = res.error();
          return;
        }

      if (options.connectTimeoutSecs)
        if (Result res = setConnectTimeout(*options.connectTimeoutSecs); !res) {
          m_initError = res.error();
          return;
        }

      if (options.userAgent)
        if (Result res = setUserAgent(*options.userAgent); !res) {
          m_initError = res.error();
          return;
        }
    }

    /**
     * @brief Destructor. Cleans up the CURL easy handle.
     */
    ~Easy() {
      if (m_curl)
        curl_easy_cleanup(m_curl);
    }

    // Non-copyable
    Easy(const Easy&)                = delete;
    fn operator=(const Easy&)->Easy& = delete;

    /**
     * @brief Move constructor.
     * @param other The other Easy object to move from.
     */
    Easy(Easy&& other) noexcept
      : m_curl(std::exchange(other.m_curl, nullptr)), m_initError(std::move(other.m_initError)) {}

    /**
     * @brief Move assignment operator.
     * @param other The other Easy object to move from.
     * @return A reference to this object.
     */
    fn operator=(Easy&& other) noexcept -> Easy& {
      if (this != &other) {
        if (m_curl)
          curl_easy_cleanup(m_curl);
        m_curl      = std::exchange(other.m_curl, nullptr);
        m_initError = std::move(other.m_initError);
      }

      return *this;
    }

    /**
     * @brief Checks if the CURL handle is valid and initialized without errors.
     * @return True if the handle is valid and no initialization error occurred, false otherwise.
     */
    [[nodiscard]] explicit operator bool() const {
      return m_curl != nullptr && !m_initError;
    }

    /**
     * @brief Gets any error that occurred during initialization via the options constructor.
     * @return An Option containing a DracError if initialization failed, otherwise None.
     */
    [[nodiscard]] fn getInitializationError() const -> const util::types::Option<util::error::DracError>& {
      return m_initError;
    }

    /**
     * @brief Gets the underlying CURL handle.
     * @return The CURL handle.
     */
    [[nodiscard]] fn get() const -> CURL* {
      return m_curl;
    }

    /**
     * @brief Sets a CURL option.
     * @tparam T The type of the option value.
     * @param option The CURL option to set.
     * @param value The value to set for the option.
     * @return A Result indicating success or failure.
     */
    template <typename T>
    fn setOpt(CURLoption option, T value) -> util::types::Result<> {
      using util::types::Err, util::error::DracError, util::error::DracErrorCode;

      if (!m_curl)
        return Err(DracError(DracErrorCode::InternalError, "CURL handle is not initialized or init failed"));

      if (m_initError)
        return Err(DracError(DracErrorCode::InternalError, "CURL handle initialization previously failed"));

      const CURLcode res = curl_easy_setopt(m_curl, option, value);

      if (res != CURLE_OK)
        return Err(DracError(DracErrorCode::PlatformSpecific, std::format("curl_easy_setopt failed: {}", curl_easy_strerror(res))));

      return {};
    }

    /**
     * @brief Performs a blocking file transfer.
     * @return A Result indicating success or failure.
     */
    fn perform() -> util::types::Result<> {
      using util::types::Err, util::error::DracError, util::error::DracErrorCode;

      if (!m_curl)
        return Err(DracError(DracErrorCode::InternalError, "CURL handle is not initialized or init failed"));

      if (m_initError)
        return Err(DracError(DracErrorCode::InternalError, std::format("Cannot perform request, CURL handle initialization failed: {}", m_initError->message)));

      const CURLcode res = curl_easy_perform(m_curl);

      if (res != CURLE_OK)
        return Err(DracError(DracErrorCode::ApiUnavailable, std::format("curl_easy_perform failed: {}", curl_easy_strerror(res))));

      return {};
    }

    /**
     * @brief Gets information from a CURL transfer.
     * @tparam T The type of the information to get.
     * @param info The CURLINFO to get.
     * @param value A pointer to store the retrieved information.
     * @return A Result indicating success or failure.
     */
    template <typename T>
    fn getInfo(CURLINFO info, T* value) -> util::types::Result<> {
      using util::types::Err, util::error::DracError, util::error::DracErrorCode;

      if (!m_curl)
        return Err(DracError(DracErrorCode::InternalError, "CURL handle is not initialized or init failed"));

      if (m_initError)
        return Err(DracError(DracErrorCode::InternalError, std::format("CURL handle initialization previously failed: {}", m_initError->message)));

      const CURLcode res = curl_easy_getinfo(m_curl, info, value);

      if (res != CURLE_OK)
        return Err(DracError(DracErrorCode::PlatformSpecific, std::format("curl_easy_getinfo failed: {}", curl_easy_strerror(res))));

      return {};
    }

    /**
     * @brief Escapes a URL string.
     * @param url The URL string to escape.
     * @return A Result containing the escaped string or an error.
     */
    static fn escape(const util::types::String& url) -> util::types::Result<util::types::String> {
      using util::types::Err, util::error::DracError, util::error::DracErrorCode, util::types::String;

      char* escapedUrl = curl_easy_escape(nullptr, url.c_str(), static_cast<int>(url.length()));

      if (!escapedUrl)
        return Err(DracError(DracErrorCode::OutOfMemory, "curl_easy_escape failed"));

      String result(escapedUrl);

      curl_free(escapedUrl);

      return result;
    }

    /**
     * @brief Sets the URL for the transfer.
     * @param url The URL to set.
     * @return A Result indicating success or failure.
     */
    fn setUrl(const util::types::String& url) -> util::types::Result<> {
      return setOpt(CURLOPT_URL, url.c_str());
    }

    /**
     * @brief Sets the write function and data for the transfer.
     * @param buffer The string buffer to write the response to.
     * @return A Result indicating success or failure.
     */
    fn setWriteFunction(util::types::String* buffer) -> util::types::Result<> {
      using util::types::Err, util::types::Result, util::error::DracError, util::error::DracErrorCode;

      if (!buffer)
        return Err(DracError(DracErrorCode::InvalidArgument, "Write buffer cannot be null"));

      if (Result res = setOpt(CURLOPT_WRITEFUNCTION, writeCallback); !res)
        return res;

      return setOpt(CURLOPT_WRITEDATA, buffer);
    }

    /**
     * @brief Sets the timeout for the transfer.
     * @param timeout The timeout in seconds.
     * @return A Result indicating success or failure.
     */
    fn setTimeout(util::types::i64 timeout) -> util::types::Result<> {
      return setOpt(CURLOPT_TIMEOUT, timeout);
    }

    /**
     * @brief Sets the connect timeout for the transfer.
     * @param timeout The connect timeout in seconds.
     * @return A Result indicating success or failure.
     */
    fn setConnectTimeout(util::types::i64 timeout) -> util::types::Result<> {
      return setOpt(CURLOPT_CONNECTTIMEOUT, timeout);
    }

    /**
     * @brief Sets the user agent for the transfer.
     * @param userAgent The user agent string.
     * @return A Result indicating success or failure.
     */
    fn setUserAgent(const util::types::String& userAgent) -> util::types::Result<> {
      return setOpt(CURLOPT_USERAGENT, userAgent.c_str());
    }
  };

  /**
   * @brief RAII wrapper for CURL multi handle.
   */
  class Multi {
    CURLM*                                      m_multi     = nullptr;
    util::types::Option<util::error::DracError> m_initError = util::types::None;

   public:
    /**
     * @brief Constructor. Initializes a CURL multi handle.
     */
    Multi() : m_multi(curl_multi_init()) {
      using util::types::Err, util::error::DracError, util::error::DracErrorCode;

      if (!m_multi)
        m_initError = DracError(DracErrorCode::ApiUnavailable, "curl_multi_init() failed");
    }

    /**
     * @brief Destructor. Cleans up the CURL multi handle.
     */
    ~Multi() {
      if (m_multi)
        curl_multi_cleanup(m_multi);
    }

    // Non-copyable
    Multi(const Multi&)                = delete;
    fn operator=(const Multi&)->Multi& = delete;

    /**
     * @brief Move constructor.
     * @param other The other Multi object to move from.
     */
    Multi(Multi&& other) noexcept
      : m_multi(std::exchange(other.m_multi, nullptr)), m_initError(std::move(other.m_initError)) {}

    /**
     * @brief Move assignment operator.
     * @param other The other Multi object to move from.
     * @return A reference to this object.
     */
    fn operator=(Multi&& other) noexcept -> Multi& {
      if (this != &other) {
        if (m_multi)
          curl_multi_cleanup(m_multi);

        m_multi     = std::exchange(other.m_multi, nullptr);
        m_initError = std::move(other.m_initError);
      }

      return *this;
    }

    /**
     * @brief Checks if the CURL multi handle is valid and initialized without error.
     * @return True if the handle is valid and no init error, false otherwise.
     */
    [[nodiscard]] explicit operator bool() const {
      return m_multi != nullptr && !m_initError;
    }

    /**
     * @brief Gets any error that occurred during initialization.
     * @return An Option containing a DracError if initialization failed, otherwise None.
     */
    [[nodiscard]] fn getInitializationError() const -> const util::types::Option<util::error::DracError>& {
      return m_initError;
    }

    /**
     * @brief Gets the underlying CURLM handle.
     * @return The CURLM handle.
     */
    [[nodiscard]] fn get() const -> CURLM* {
      return m_multi;
    }

    /**
     * @brief Adds an easy handle to the multi handle.
     * @param easyHandle The Easy handle to add.
     * @return A Result indicating success or failure.
     */
    fn addHandle(const Easy& easyHandle) -> util::types::Result<> {
      using util::types::Err, util::error::DracError, util::error::DracErrorCode;

      if (!m_multi)
        return Err(DracError(DracErrorCode::InternalError, "CURL multi handle is not initialized or init failed"));

      if (m_initError)
        return Err(DracError(DracErrorCode::InternalError, std::format("CURL multi handle initialization previously failed: {}", m_initError->message)));

      if (!easyHandle.get())
        return Err(DracError(DracErrorCode::InvalidArgument, "Provided CURL easy handle is not valid"));

      if (easyHandle.getInitializationError())
        return Err(DracError(DracErrorCode::InvalidArgument, std::format("Provided CURL easy handle failed initialization: {}", easyHandle.getInitializationError()->message)));

      const CURLMcode res = curl_multi_add_handle(m_multi, easyHandle.get());

      if (res != CURLM_OK)
        return Err(DracError(DracErrorCode::PlatformSpecific, std::format("curl_multi_add_handle failed: {}", curl_multi_strerror(res))));

      return {};
    }

    /**
     * @brief Removes an easy handle from the multi handle.
     * @param easyHandle The Easy handle to remove.
     * @return A Result indicating success or failure.
     */
    fn removeHandle(const Easy& easyHandle) -> util::types::Result<> {
      using util::types::Err, util::error::DracError, util::error::DracErrorCode;

      if (!m_multi)
        return Err(DracError(DracErrorCode::InternalError, "CURL multi handle is not initialized or init failed"));

      if (m_initError)
        return Err(DracError(DracErrorCode::InternalError, std::format("CURL multi handle initialization previously failed: {}", m_initError->message)));

      if (!easyHandle.get()) // It's okay to try to remove a null handle, curl_multi_remove_handle handles it.
        return Err(DracError(DracErrorCode::InvalidArgument, "Provided CURL easy handle is not valid (for removal check)"));

      const CURLMcode res = curl_multi_remove_handle(m_multi, easyHandle.get());

      if (res != CURLM_OK) // CURLM_BAD_EASY_HANDLE is a possible error if handle was not in multi stack
        return Err(DracError(DracErrorCode::PlatformSpecific, std::format("curl_multi_remove_handle failed: {}", curl_multi_strerror(res))));

      return {};
    }

    /**
     * @brief Performs transfers on the multi handle.
     * @param stillRunning A pointer to an integer that will be set to the number of still running transfers.
     * @return A Result indicating success or failure.
     */
    fn perform(util::types::i32* stillRunning) -> util::types::Result<> {
      using util::types::Err, util::error::DracError, util::error::DracErrorCode;

      if (!m_multi)
        return Err(DracError(DracErrorCode::InternalError, "CURL multi handle is not initialized or init failed"));

      if (m_initError)
        return Err(DracError(DracErrorCode::InternalError, std::format("CURL multi handle initialization previously failed: {}", m_initError->message)));

      const CURLMcode res = curl_multi_perform(m_multi, stillRunning);

      if (res != CURLM_OK && res != CURLM_CALL_MULTI_PERFORM)
        return Err(DracError(DracErrorCode::PlatformSpecific, std::format("curl_multi_perform failed: {}", curl_multi_strerror(res))));

      return {};
    }

    /**
     * @brief Reads information about completed transfers.
     * @param msgsInQueue A pointer to an integer that will be set to the number of messages in the queue.
     * @return A Result containing a CURLMsg pointer or an error. The caller is responsible for checking the msg field of CURLMsg.
     */
    fn infoRead(util::types::i32* msgsInQueue) -> util::types::Result<CURLMsg*> {
      using util::types::Err, util::error::DracError, util::error::DracErrorCode;

      if (!m_multi)
        return Err(DracError(DracErrorCode::InternalError, "CURL multi handle is not initialized or init failed"));

      if (m_initError)
        return Err(DracError(DracErrorCode::InternalError, std::format("CURL multi handle initialization previously failed: {}", m_initError->message)));

      CURLMsg* msg = curl_multi_info_read(m_multi, msgsInQueue);

      return msg;
    }

    /**
     * @brief Waits for activity on any of the multi handle's file descriptors using poll.
     * @param timeoutMs The maximum time to wait in milliseconds.
     * @param numfds A pointer to an integer that will be set to the number of file descriptors with activity. Can be nullptr.
     * @return A Result indicating success or failure.
     */
    fn poll(util::types::i32 timeoutMs, util::types::i32* numfds) -> util::types::Result<> {
      using util::types::Err, util::error::DracError, util::error::DracErrorCode;

      if (!m_multi)
        return Err(DracError(DracErrorCode::InternalError, "CURL multi handle is not initialized or init failed"));

      if (m_initError)
        return Err(DracError(DracErrorCode::InternalError, std::format("CURL multi handle initialization previously failed: {}", m_initError->message)));

      const CURLMcode res = curl_multi_poll(m_multi, nullptr, 0, timeoutMs, numfds);

      if (res != CURLM_OK)
        return Err(DracError(DracErrorCode::PlatformSpecific, std::format("curl_multi_poll failed: {}", curl_multi_strerror(res))));

      return {};
    }

    /**
     * @brief Waits for activity on any of the multi handle's file descriptors using select semantics.
     * @param timeoutMs The maximum time to wait in milliseconds.
     * @param numfds A pointer to an integer that will be set to the number of file descriptors with activity.
     * @return A Result indicating success or failure.
     * @note This function is an alternative to poll and might be needed if curl_multi_poll is not available or desired.
     *       It requires more setup with curl_multi_fdset. For simplicity, poll is preferred if available.
     *       This is a simplified version; a full fdset handling is more complex.
     */
    fn wait(util::types::i32 timeoutMs, util::types::i32* numfds) -> util::types::Result<> {
      using util::types::Err, util::error::DracError, util::error::DracErrorCode;

      if (!m_multi)
        return Err(DracError(DracErrorCode::InternalError, "CURL multi handle is not initialized or init failed"));

      if (m_initError)
        return Err(DracError(DracErrorCode::InternalError, std::format("CURL multi handle initialization previously failed: {}", m_initError->message)));

      const CURLMcode res = curl_multi_wait(m_multi, nullptr, 0, timeoutMs, numfds);

      if (res != CURLM_OK)
        return Err(DracError(DracErrorCode::PlatformSpecific, std::format("curl_multi_wait failed: {}", curl_multi_strerror(res))));

      return {};
    }
  };

  /**
   * @brief Initializes CURL globally. Should be called once at the start of the program.
   * @param flags CURL global init flags.
   * @return A Result indicating success or failure.
   */
  inline fn GlobalInit(util::types::i32 flags = CURL_GLOBAL_ALL) -> util::types::Result<> {
    using util::types::Err, util::error::DracError, util::error::DracErrorCode;

    const CURLcode res = curl_global_init(flags);

    if (res != CURLE_OK)
      return Err(DracError(DracErrorCode::PlatformSpecific, std::format("curl_global_init failed: {}", curl_easy_strerror(res))));

    return {};
  }

  /**
   * @brief Cleans up CURL globally. Should be called once at the end of the program.
   */
  inline fn GlobalCleanup() -> void {
    curl_global_cleanup();
  }
} // namespace Curl
