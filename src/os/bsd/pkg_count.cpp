#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__NetBSD__)

// clang-format off
#include <SQLiteCpp/Database.h>  // SQLite::{Database, OPEN_READONLY}
#include <SQLiteCpp/Exception.h> // SQLite::Exception
#include <SQLiteCpp/Statement.h> // SQLite::Statement

#include <chrono>                // std::chrono
#include <filesystem>            // std::filesystem
#include <format>                // std::format
#include <glaze/beve/write.hpp>  // glz::write_beve
#include <glaze/core/common.hpp> // glz::object
#include <glaze/core/meta.hpp>   // glz::detail::Object
#include <system_error>          // std::error_code

#include "src/os/bsd/pkg_count.hpp"
#include "src/os/os.hpp"
#include "src/util/cache.hpp"
#include "src/util/defs.hpp"
#include "src/util/error.hpp"
#include "src/util/logging.hpp"
#include "src/util/types.hpp"
// clang-format on

using util::error::DracError, util::error::DracErrorCode;
using util::types::u64, util::types::i64, util::types::Result, util::types::Err, util::types::String,
  util::types::StringView, util::types::Exception;

namespace package {
  #if defined(__FreeBSD__) || defined(__DragonFly__)
  fn GetPkgNgCount() -> Result<u64, DracError> {
    const PackageManagerInfo pkgInfo = {
      .id         = "pkgng", // Use core struct
      .dbPath     = "/var/db/pkg/local.sqlite",
      .countQuery = "SELECT COUNT(*) FROM packages",
    };

    return GetCountFromDb(pkgInfo);
  }
  #elif defined(__NetBSD__)
  fn GetPkgSrcCount() -> Result<u64, DracError> {
    return GetCountFromDirectory("pkgsrc", fs::current_path().root_path() / "usr" / "pkg" / "pkgdb", true);
  }
  #endif
} // namespace package

#endif // __FreeBSD__ || __DragonFly__ || __NetBSD__
