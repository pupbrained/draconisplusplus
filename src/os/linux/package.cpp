#ifdef __linux__

// clang-format off
#include "src/core/package.hpp"

#include <SQLiteCpp/Database.h>   // SQLite::{Database, OPEN_READONLY}
#include <SQLiteCpp/Exception.h>  // SQLite::Exception
#include <SQLiteCpp/Statement.h>  // SQLite::Statement
#include <filesystem>             // std::filesystem::{current_path, directory_entry, directory_iterator, etc.}
#include <glaze/beve/read.hpp>    // glz::read_beve
#include <glaze/beve/write.hpp>   // glz::write_beve

#include "src/util/defs.hpp"
// clang-format on

using namespace std::string_literals;

namespace package {
  fn GetDpkgCount() -> Result<u64, DracError> {
    return GetCountFromDirectory("Dpkg", fs::current_path().root_path() / "var" / "lib" / "dpkg" / "info", ".list"s);
  }

  fn GetMossCount() -> Result<u64, DracError> {
    const PackageManagerInfo mossInfo = {
      .id         = "moss",
      .dbPath     = "/.moss/db/install",
      .countQuery = "SELECT COUNT(*) FROM meta",
    };

    Result<u64, DracError> countResult = GetCountFromDb(mossInfo);

    if (countResult)
      if (*countResult > 0)
        return *countResult - 1;

    return countResult;
  }

  fn GetPacmanCount() -> Result<u64, DracError> {
    return GetCountFromDirectory("Pacman", fs::current_path().root_path() / "var" / "lib" / "pacman" / "local", true);
  }
} // namespace package

#endif // __linux__
