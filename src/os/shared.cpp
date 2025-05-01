#include <filesystem>

#include "src/core/util/defs.hpp"
#include "src/core/util/error.hpp"
#include "src/core/util/helpers.hpp"
#include "src/core/util/logging.hpp"
#include "src/core/util/types.hpp"

#include "os.hpp"

namespace os::shared {
  using util::error::DracError, util::error::DracErrorCode;
  using util::types::u64, util::types::String, util::types::Result, util::types::Err;

  namespace fs = std::filesystem;

  fn GetPackageCount() -> Result<u64, DracError> {
    using util::helpers::GetEnv;

    fs::path cargoPath {};

    if (Result<String, DracError> cargoHome = GetEnv("CARGO_HOME"))
      cargoPath = fs::path(*cargoHome) / "bin";
    else if (Result<String, DracError> homeDir = GetEnv("HOME"))
      cargoPath = fs::path(*homeDir) / ".cargo" / "bin";

    if (cargoPath.empty() || !fs::exists(cargoPath))
      return Err(DracError(DracErrorCode::NotFound, "Could not find cargo directory"));

    u64 count = 0;

    for (const fs::directory_entry& entry : fs::directory_iterator(cargoPath))
      if (entry.is_regular_file())
        ++count;

    debug_log("Found {} packages in cargo directory: {}", count, cargoPath.string());

    return count;
  }
} // namespace os::shared
