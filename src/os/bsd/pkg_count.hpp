#pragma once

// clang-format off
#include <filesystem>            // std::filesystem::path
#include <glaze/core/common.hpp> // glz::object
#include <glaze/core/meta.hpp>   // glz::detail::Object

#include "src/util/error.hpp"
#include "src/util/types.hpp"
// clang-format on

namespace os::bsd {
  using util::error::DracError;
  using util::types::Result, util::types::u64, util::types::i64, util::types::String;
  namespace fs = std::filesystem;

  struct PackageManagerInfo {
    String   id;
    fs::path dbPath;
    String   countQuery;
  };

  struct PkgCountCacheData {
    u64 count {};
    i64 timestampEpochSeconds {};

    // NOLINTBEGIN(readability-identifier-naming)
    struct [[maybe_unused]] glaze {
      using T = PkgCountCacheData;

      static constexpr glz::detail::Object value =
        glz::object("count", &T::count, "timestamp", &T::timestampEpochSeconds);
    };
    // NOLINTEND(readability-identifier-naming)
  };
} // namespace os::bsd
