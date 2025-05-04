#ifdef __HAIKU__
// clang-format off
#include "src/core/package.hpp"

#include <os/package/PackageDefs.h>    // BPackageKit::BPackageInfoSet
#include <os/package/PackageInfoSet.h> // BPackageKit::BPackageInfo
#include <os/package/PackageRoster.h>  // BPackageKit::BPackageRoster

#include "src/util/defs.hpp"
#include "src/util/error.hpp"
// clang-format on

namespace package {
  using util::error::DracError, util::error::DracErrorCode;
  using util::types::Err;

  fn GetHaikuCount() -> Result<u64, DracError> {
    BPackageKit::BPackageRoster  roster;
    BPackageKit::BPackageInfoSet packageList;

    const status_t status = roster.GetActivePackages(BPackageKit::B_PACKAGE_INSTALLATION_LOCATION_SYSTEM, packageList);

    if (status != B_OK)
      return Err(DracError(DracErrorCode::ApiUnavailable, "Failed to get active package list"));

    return static_cast<u64>(packageList.CountInfos());
  }
} // namespace package

#endif // __HAIKU__
