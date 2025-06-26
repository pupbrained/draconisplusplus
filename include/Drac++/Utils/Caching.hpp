#pragma once

#include <chrono>
#include <filesystem>

namespace draconis::utils::cache {

// Defines where the cache data should be stored on disk.
enum class CacheLocation {
    InMemory,      // Volatile, lost on app exit. Fastest.
    TempDirectory, // Persists until next reboot or system cleanup.
    Persistent     // Stored in a user-level cache dir (e.g., ~/.cache).
};

// Defines the caching strategy for an item.
struct CachePolicy {
    CacheLocation location = CacheLocation::Persistent;
    std::chrono::seconds ttl = std::chrono::hours(24); // Default to 24 hours.

    // A static helper for a policy that never expires.
    static CachePolicy NeverExpire() {
        return { CacheLocation::Persistent, std::chrono::seconds::max() };
    }
};

} // namespace draconis::utils::cache