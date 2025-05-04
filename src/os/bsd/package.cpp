#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__NetBSD__)

// clang-format off
#include "src/core/package.hpp"

#include <SQLiteCpp/Database.h>  // SQLite::{Database, OPEN_READONLY}
#include <SQLiteCpp/Exception.h> // SQLite::Exception
#include <SQLiteCpp/Statement.h> // SQLite::Statement
#include <glaze/beve/write.hpp>  // glz::write_beve
#include <glaze/core/common.hpp> // glz::object
#include <glaze/core/meta.hpp>   // glz::detail::Object

#include "src/util/defs.hpp"
// clang-format on

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
