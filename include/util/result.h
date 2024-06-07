#pragma once

#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

// Define an error type
class Error {
 public:
  explicit Error(std::string message) : m_Message(std::move(message)) {}
  [[nodiscard]] const std::string& message() const { return m_Message; }

 private:
  std::string m_Message;
};

// Primary template for Result with a default type of void
template <typename T = void>
class Result;

// Specialization for Result<void>
template <>
class Result<void> {
 public:
  // Constructors for success and error
  Result() : m_Result(std::monostate {}) {}
  Result(const Error& error) : m_Result(error) {}
  Result(Error&& error) : m_Result(std::move(error)) {}

  // Check if the result is an error
  [[nodiscard]] bool isOk() const {
    return std::holds_alternative<std::monostate>(m_Result);
  }
  [[nodiscard]] bool isErr() const {
    return std::holds_alternative<Error>(m_Result);
  }

  // Throw an exception if it is an error
  void value() const {
    if (isErr()) {
      throw std::logic_error("Attempted to access value of an error Result");
    }
  }

  // Get the error or throw an exception if it is a value
  [[nodiscard]] const Error& error() const {
    if (isOk()) {
      throw std::logic_error("Attempted to access error of an ok Result");
    }
    return std::get<Error>(m_Result);
  }

 private:
  std::variant<std::monostate, Error> m_Result;
};

// Primary template for Result
template <typename T>
class Result {
 public:
  // Constructors for success and error
  Result(const T& value) : m_Result(value) {}
  Result(T&& value) : m_Result(std::move(value)) {}
  Result(const Error& error) : m_Result(error) {}
  Result(Error&& error) : m_Result(std::move(error)) {}

  // Check if the result is an error
  [[nodiscard]] bool isOk() const {
    return std::holds_alternative<T>(m_Result);
  }
  [[nodiscard]] bool isErr() const {
    return std::holds_alternative<Error>(m_Result);
  }

  // Get the value or throw an exception if it is an error
  const T& value() const {
    if (isErr()) {
      throw std::logic_error("Attempted to access value of an error Result");
    }
    return std::get<T>(m_Result);
  }

  // Get the error or throw an exception if it is a value
  [[nodiscard]] const Error& error() const {
    if (isOk()) {
      throw std::logic_error("Attempted to access error of an ok Result");
    }
    return std::get<Error>(m_Result);
  }

  // Optional: Get the value or provide a default
  T valueOr(const T& defaultValue) const {
    return isOk() ? std::get<T>(m_Result) : defaultValue;
  }

 private:
  std::variant<T, Error> m_Result;
};

template <typename T>
auto Ok(T&& value) {
  return Result<std::decay_t<T>>(std::forward<T>(value));
}

inline auto Ok() {
  return Result<void>();
}
