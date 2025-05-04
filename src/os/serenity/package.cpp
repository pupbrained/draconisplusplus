#ifdef __serenity__

// clang-format off
#include "src/core/package.hpp"

#include "src/util/defs.hpp"
#include "src/util/error.hpp"
#include "src/util/types.hpp"
// clang-format on

using util::error::DracError, util::error::DracErrorCode;
using util::types::u64, util::types::String, util::types::Result;

namespace {
  fn CountUniquePackages(const String& dbPath) -> Result<u64, DracError> {
    std::ifstream dbFile(dbPath);

    if (!dbFile.is_open())
      return Err(DracError(DracErrorCode::NotFound, std::format("Failed to open file: {}", dbPath.string())));

    std::unordered_set<String> uniquePackages;
    String                     line;

    while (std::getline(dbFile, line))
      if (line.starts_with("manual ") || line.starts_with("auto "))
        uniquePackages.insert(line);

    return uniquePackages.size();
  }
} // namespace

namespace package {
  fn GetSerenityCount() -> Result<u64, DracError> { return CountUniquePackages("/usr/ports/installed.db"); }
} // namespace package

#endif // __serenity__
