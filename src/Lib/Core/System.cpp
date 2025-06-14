#include <Drac++/Core/System.hpp>
#include <chrono>      // std::chrono::system_clock
#include <ctime>       // localtime_r/s, strftime, time_t, tm
#include <format>      // std::format
#include <matchit.hpp> // matchit::{match, is, in, _}

#if DRAC_ENABLE_PACKAGECOUNT
  #include <Drac++/Services/PackageCounting.hpp>
#endif

#if DRAC_ENABLE_WEATHER
  #include <Drac++/Services/Weather.hpp>
#endif

#include <DracUtils/Definitions.hpp>
#include <DracUtils/Error.hpp>
#include <DracUtils/Types.hpp>

using namespace util::types;
using util::error::DracError;
using enum util::error::DracErrorCode;

namespace {
  fn getOrdinalSuffix(const i32 day) -> CStr {
    using matchit::match, matchit::is, matchit::_, matchit::in;

    return match(day)(
      is | in(11, 13)    = "th",
      is | (_ % 10 == 1) = "st",
      is | (_ % 10 == 2) = "nd",
      is | (_ % 10 == 3) = "rd",
      is | _             = "th"
    );
  }
} // namespace

namespace os {
  fn System::getDate() -> Result<SZString> {
    using std::chrono::system_clock;

    const system_clock::time_point nowTp = system_clock::now();
    const std::time_t              nowTt = system_clock::to_time_t(nowTp);

    std::tm nowTm;

#ifdef _WIN32
    if (localtime_s(&nowTm, &nowTt) == 0) {
#else
    if (localtime_r(&nowTt, &nowTm) != nullptr) {
#endif
      i32 day = nowTm.tm_mday;

      SZString monthBuffer(32, '\0');

      if (const usize monthLen = std::strftime(monthBuffer.data(), monthBuffer.size(), "%B", &nowTm); monthLen > 0) {
        monthBuffer.resize(monthLen);

        CStr suffix = getOrdinalSuffix(day);

        try {
          return std::format("{} {}{}", monthBuffer, day, suffix);
        } catch (const std::format_error& e) { return Err(DracError(ParseError, e.what())); }
      }

      return Err(DracError(ParseError, "Failed to format date"));
    }

    return Err(DracError(ParseError, "Failed to get local time"));
  }
} // namespace os