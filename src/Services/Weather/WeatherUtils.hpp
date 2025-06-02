#pragma once

#if DRAC_ENABLE_WEATHER

// clang-format off
#include <unordered_map>

#include "Util/Error.hpp"
#include "Util/Types.hpp"
// clang-format on

namespace weather::utils {
  using util::types::StringView, util::types::Result, util::types::usize, util::types::String;

  /**
   * @brief Strips time-of-day suffixes (_day, _night, _polartwilight) from a weather symbol code.
   * @param symbol The weather symbol code.
   * @return The symbol code without the time-of-day suffix.
   */
  fn StripTimeOfDayFromSymbol(StringView symbol) -> StringView;

  /**
   * @brief Parses an ISO8601 timestamp string (YYYY-MM-DDTHH:MM:SSZ) to a Unix epoch timestamp.
   * @param iso8601 The ISO8601 timestamp string (must be UTC, ending in 'Z').
   * @return A Result containing the epoch timestamp (usize) on success, or a DracError on failure.
   */
  fn ParseIso8601ToEpoch(StringView iso8601) -> Result<usize>;

  /**
   * @brief Provides a mapping from MetNo weather symbol codes (after time-of-day stripping) to human-readable descriptions.
   * @return A const reference to an unordered_map of symbol codes to descriptions.
   */
  fn GetMetnoSymbolDescriptions() -> const std::unordered_map<StringView, StringView>&;

  /**
   * @brief Provides a human-readable description for an OpenMeteo weather code.
   * @param code The OpenMeteo weather code (integer).
   * @return A StringView containing the description. Returns "unknown" if the code is not recognized.
   */
  fn GetOpenmeteoWeatherDescription(int code) -> StringView;
} // namespace weather::utils

#endif // DRAC_ENABLE_WEATHER
