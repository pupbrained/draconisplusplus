#pragma once

#include <charconv>
#include <cstring>
#include <stringzilla/stringzilla.hpp>

#include "Definitions.hpp"
#include "Types.hpp"

namespace util::formatting {
  template <typename T, typename CharT = char, typename = void>
  struct Formatter;

  namespace detail {
    class FormatParseContext {
      types::SZStringView m_specView;

     public:
      constexpr explicit FormatParseContext(types::SZStringView spec) noexcept : m_specView(spec) {}

      [[nodiscard]] constexpr auto begin() const noexcept {
        return m_specView.begin();
      }
      [[nodiscard]] constexpr auto end() const noexcept {
        return m_specView.end();
      }
      [[nodiscard]] constexpr auto size() const noexcept {
        return m_specView.size();
      }
      [[nodiscard]] constexpr auto empty() const noexcept -> bool {
        return m_specView.empty();
      }

      constexpr fn advanceTo(const char* iter) {
        m_specView.remove_prefix(iter - begin());
      }
    };

    // A simple context holding the output buffer and arguments
    template <typename... Args>
    class FormatContext {
      types::SZString*     m_out;
      std::tuple<Args...>* m_args;

     public:
      FormatContext(types::SZString& out, std::tuple<Args...>& args) : m_out(&out), m_args(&args) {}

      // Access an argument by its index
      template <typename T>
      fn arg(types::usize index) const {
        if (index >= sizeof...(Args))
          throw std::runtime_error("Format argument index out of bounds.");
        return std::get<T>(*m_args);
      }

      // Get a reference to the output string
      fn out() -> types::SZString& {
        return *m_out;
      }

      // Get the arguments tuple
      fn args() -> std::tuple<Args...>& {
        return *m_args;
      }
    };

    template <typename F, typename... Args>
    fn VisitAt(types::usize index, F&& func, std::tuple<Args...>& tuple) {
      [&]<types::usize... i>(std::index_sequence<i...>) {
        bool called = false;
        (... && (i == index ? (std::forward<F>(func)(std::get<i>(tuple)), called = true) : true));
        if (!called && index >= sizeof...(Args)) {
          throw std::runtime_error("Format argument index out of bounds.");
        }
      }(std::make_index_sequence<sizeof...(Args)> {});
    }

    template <typename T>
    inline fn GetSizeEstimate(const T& value) -> size_t {
      using DecayedT = std::decay_t<T>;
      if constexpr (std::is_convertible_v<DecayedT, types::SZStringView>)
        return types::SZStringView(value).size();
      else if constexpr (std::is_same_v<DecayedT, types::CStr>)
        return std::strlen(value);
      else if constexpr (std::is_arithmetic_v<DecayedT>)
        return 32; // Conservative estimate for numbers
      else
        return 64; // Conservative estimate for other types
    }
  } // namespace detail

  template <typename T, typename CharT, typename>
  struct Formatter {
    // This is intentionally left unimplemented.
  };

  template <typename T, typename CharT>
  struct Formatter<T, CharT, std::enable_if_t<std::is_convertible_v<T, types::SZStringView>>> {
    enum class Align : types::u8 {
      Left,
      Right,
      Center
    };

    Align        align = Align::Left;
    types::usize width = 0;
    char         fill  = ' ';

    constexpr fn parse(detail::FormatParseContext& parse_ctx) {
      const char*       iter = parse_ctx.begin();
      const char* const end  = parse_ctx.end();

      if (iter == end)
        return;

      if (std::next(iter) != end && (*std::next(iter) == '<' || *std::next(iter) == '>' || *std::next(iter) == '^')) {
        fill = *iter;
        iter = std::next(iter);
      }

      // Alignment
      if (*iter == '<')
        align = Align::Left;
      else if (*iter == '>')
        align = Align::Right;
      else if (*iter == '^')
        align = Align::Center;
      if (align != Align::Left || *iter == '<' || *iter == '>' || *iter == '^')
        iter = std::next(iter);

      // Width
      if (iter != end && *iter >= '0' && *iter <= '9') {
        auto [ptr, ec] = std::from_chars(iter, end, width);
        if (ec == std::errc()) {
          iter = ptr;
        }
      }

      if (iter != end) {
        throw std::runtime_error("Invalid format specifier for string.");
      }
    }

    fn format(const T& value, auto& ctx) const {
      types::SZStringView sview(value);
      auto&               out = ctx.out();

      if (sview.length() >= width) {
        out.append(sview);
        return;
      }

      types::usize padding = width - sview.length();
      if (align == Align::Right) {
        out.append(padding, fill);
        out.append(sview);
      } else if (align == Align::Center) {
        types::usize leftPad  = padding / 2;
        types::usize rightPad = padding - leftPad;
        out.append(leftPad, fill);
        out.append(sview);
        out.append(rightPad, fill);
      } else {
        out.append(sview);
        out.append(padding, fill);
      }
    }
  };

  // Formatter for booleans
  template <typename CharT>
  struct Formatter<bool, CharT> {
    constexpr fn parse(detail::FormatParseContext& parse_ctx) {
      if (!parse_ctx.empty())
        throw std::runtime_error("Invalid format specifier for bool.");
    }

    fn format(bool value, auto& ctx) const {
      ctx.out().append(value ? "true" : "false");
    }
  };

  // Formatter for integers
  template <typename T, typename CharT>
  struct Formatter<T, CharT, std::enable_if_t<std::is_integral_v<T>>> {
    enum class Radix : types::u8 {
      Dec = 10,
      Hex = 16,
      Oct = 8,
      Bin = 2
    };

    Radix        radix     = Radix::Dec;
    bool         upperCase = false;
    bool         zeroPad   = false;
    types::usize width     = 0;

    constexpr fn parse(detail::FormatParseContext& parse_ctx) {
      const char*       iter = parse_ctx.begin();
      const char* const end  = parse_ctx.end();

      if (iter == end)
        return;

      if (*iter == '0') {
        zeroPad = true;
        iter    = std::next(iter);
      }

      auto [ptr, ec] = std::from_chars(iter, end, width);
      if (ec == std::errc()) {
        iter = ptr;
      }

      if (iter != end) {
        switch (*iter) {
          case 'd': radix = Radix::Dec; break;
          case 'x': radix = Radix::Hex; break;
          case 'X':
            radix     = Radix::Hex;
            upperCase = true;
            break;
          case 'o': radix = Radix::Oct; break;
          case 'b': radix = Radix::Bin; break;
          case 'B':
            radix     = Radix::Bin;
            upperCase = true;
            break;
          default: throw std::runtime_error("Invalid type specifier for integer.");
        }
        iter = std::next(iter);
      }

      if (iter != end)
        throw std::runtime_error("Invalid format specifier for integer.");
    }

    fn format(T value, auto& ctx) const {
      types::Array<char, 65> buffer;

      auto [ptr, ec] = std::to_chars(buffer.data(), std::next(buffer.data(), buffer.size() - 1), value, static_cast<int>(radix));

      if (ec != std::errc()) {
        throw std::runtime_error("Failed to format integer.");
      }

      types::SZStringView numView(buffer.data(), ptr - buffer.data());
      auto&               out = ctx.out();

      if (upperCase && radix == Radix::Hex) {
        types::SZString tempStr(numView);
        for (char& character : tempStr)
          if (character >= 'a' && character <= 'f')
            character = static_cast<char>(character - 'a' + 'A');

        numView = tempStr;
      }

      if (numView.length() < width) {
        types::usize padding  = width - numView.length();
        char         fillChar = zeroPad ? '0' : ' ';
        out.append(padding, fillChar);
      }
      out.append(numView);
    }
  };

  template <typename T, typename CharT>
  struct Formatter<T, CharT, std::enable_if_t<std::is_floating_point_v<T>>> {
    int               precision = -1; // Default precision
    std::chars_format fmt       = std::chars_format::general;
    constexpr fn      parse(detail::FormatParseContext& parse_ctx) {
      const char*       iter = parse_ctx.begin();
      const char* const end  = parse_ctx.end();
      if (iter == end)
        return;
      if (*iter == '.') {
        iter           = std::next(iter);
        auto [ptr, ec] = std::from_chars(iter, end, precision);
        if (ec != std::errc()) {
          throw std::runtime_error("Invalid precision in format specifier.");
        }
        iter = ptr;
      }
      if (iter != end) {
        if (*iter == 'f' || *iter == 'F') {
          fmt = std::chars_format::fixed;
        } else if (*iter == 'e' || *iter == 'E') {
          fmt = std::chars_format::scientific;
        } else if (*iter == 'g' || *iter == 'G') {
          fmt = std::chars_format::general;
        } else {
          throw std::runtime_error("Invalid type specifier for float.");
        }
        iter = std::next(iter);
      }
      if (iter != end) {
        throw std::runtime_error("Invalid format specifier for float.");
      }
    }
    fn format(T value, auto& ctx) const {
      types::Array<char, 128> buffer; // A reasonable buffer for floats
      auto [ptr, ec] = std::to_chars(buffer.data(), std::next(buffer.data(), buffer.size() - 1), value, fmt, precision);
      if (ec != std::errc()) {
        throw std::runtime_error("Failed to format floating-point number.");
      }
      ctx.out().append(buffer.data(), ptr - buffer.data());
    }
  };

  // Formatter for pointers
  template <typename CharT>
  struct Formatter<const void*, CharT> {
    constexpr fn parse(detail::FormatParseContext& parse_ctx) {
      if (!parse_ctx.empty() && (parse_ctx.size() != 1 || *parse_ctx.begin() != 'p')) {
        throw std::runtime_error("Invalid format specifier for pointer.");
      }
    }

    fn format(const void* value, auto& ctx) const {
      auto& out = ctx.out();
      out.append("0x");

      types::Array<char, 17> buffer;

      // NOLINTNEXTLINE(*-pro-type-reinterpret-cast)
      auto intVal = reinterpret_cast<uintptr_t>(value);

      auto [ptr, ec] = std::to_chars(buffer.data(), std::next(buffer.data(), buffer.size() - 1), intVal, 16);

      if (ec != std::errc())
        throw std::runtime_error("Failed to format pointer.");

      out.append(buffer.data(), ptr - buffer.data());
    }
  };

  /// MARK: - Main Formatting Function

  /**
   * @brief Formats a string with arguments.
   * @tparam Args The types of the arguments.
   * @param fmt The format string.
   * @param args The arguments to be formatted.
   * @return A new string with the formatted result.
   */
  template <typename... Args>
  inline fn SzFormat(types::SZStringView fmt, Args&&... args) -> types::SZString {
    types::SZString result;

    // Pre-allocate memory to reduce re-allocations
    size_t estimatedSize = fmt.size();
    ((estimatedSize += detail::GetSizeEstimate(args)), ...);
    result.reserve(estimatedSize);

    auto                  argTuple = std::make_tuple(std::forward<Args>(args)...);
    detail::FormatContext context(result, argTuple);

    const char*       ptr        = fmt.data();
    const char* const end        = std::next(ptr, static_cast<std::ptrdiff_t>(fmt.size()));
    size_t            autoArgIdx = 0;

    while (ptr < end) {
      const char* brace = strchr(ptr, '{');
      if (!brace) {
        result.append(ptr, end - ptr);
        break;
      }

      result.append(ptr, brace - ptr);
      ptr = brace;

      if (std::next(ptr) < end && *std::next(ptr) == '{') { // Escaped {{
        result.push_back('{');
        ptr = std::next(ptr, 2);
        continue;
      }

      const char* closeBrace = strchr(ptr, '}');
      if (!closeBrace)
        throw std::runtime_error("Unmatched '{' in format string.");

      // At this point, ptr points to '{' and closeBrace points to '}'
      types::SZStringView specView(std::next(ptr), std::distance(std::next(ptr), closeBrace));

      size_t argIdx = autoArgIdx++;

      auto                colonPos = specView.find(':');
      types::SZStringView argIdView;
      types::SZStringView formatSpecView;

      if (colonPos != types::SZStringView::npos) {
        argIdView      = specView.substr(0, colonPos);
        formatSpecView = specView.substr(colonPos + 1);
      } else {
        argIdView = specView;
      }

      if (!argIdView.empty()) {
        auto [ptr, ec] = std::from_chars(argIdView.data(), argIdView.data() + argIdView.size(), argIdx);
        if (ec != std::errc())
          throw std::runtime_error("Invalid argument index in format string.");

        autoArgIdx = argIdx + 1; // Next auto index follows the last specified one
      }

      detail::FormatParseContext parseCtx(formatSpecView);

      // clang-format off
      detail::VisitAt(argIdx, [&](auto& value) {
        using DecayedT = std::decay_t<decltype(value)>;
        Formatter<DecayedT> formatter;
        formatter.parse(parseCtx);
        formatter.format(value, context);
      }, argTuple);
      // clang-format on

      ptr = std::next(closeBrace);
    }

    return result;
  }

  /**
   * @brief Formats a value using its specialized formatter.
   * @tparam T The type to format
   * @param value The value to format
   * @return A formatted string
   */
  template <typename T>
  inline fn Format(const T& value) -> types::SZString {
    return SzFormat("{}", value);
  }

  /**
   * @brief Formats a single value with a custom format string.
   * @tparam T The type to format
   * @param value The value to format
   * @param fmt The format string (e.g., "{:04x}")
   * @return A formatted string
   */
  template <typename T>
  inline fn Format(const T& value, types::SZStringView fmt) -> types::SZString {
    return SzFormat(fmt, value);
  }

  /**
   * @brief Formats to a thread_local buffer and returns a view.
   * @warning The returned view is only valid until the next call to this function
   * in the same thread. Use with extreme caution.
   */
  template <typename... Args>
  inline fn SzFormatView(types::SZStringView format, Args&&... args) -> types::SZStringView {
    // This is a potentially unsafe optimization. The returned view is temporary.
    // It's invalidated by the next call to this function in the same thread.
    // Useful for cases like single-threaded loops where performance is critical
    // and the view is consumed immediately (e.g., logging).
    static thread_local types::SZString Buffer;
    Buffer = SzFormat(format, std::forward<Args>(args)...);
    return types::SZStringView(Buffer);
  }
} // namespace util::formatting

#ifndef __cpp_lib_print
inline fn operator<<(std::ostream& ostr, const types::SZString& str)->std::ostream& {
  return ostr.write(str.data(), static_cast<std::streamsize>(str.size()));
}
#endif
