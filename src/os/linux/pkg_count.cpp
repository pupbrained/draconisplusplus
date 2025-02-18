#include "src/os/linux/pkg_count.h"

namespace fs = std::filesystem;

fn GetApkPackageCount() -> std::optional<usize> {
  fs::path apkDbPath("/lib/apk/db/installed");

  return std::nullopt;
}
