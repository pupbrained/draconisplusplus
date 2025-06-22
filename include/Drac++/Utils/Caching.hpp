#pragma once

#include <chrono>                 // std::chrono::{hours, system_clock, clock_cast}
#include <filesystem>             // std::filesystem
#include <fstream>                // std::{ifstream, ofstream}
#include <glaze/beve/read.hpp>    // glz::read_beve
#include <glaze/beve/write.hpp>   // glz::write_beve
#include <glaze/core/context.hpp> // glz::{context, error_code, error_ctx}
#include <system_error>           // std::error_code
#include <type_traits>            // std::decay_t
#include <utility>

#include <Drac++/Utils/Definitions.hpp>
#include <Drac++/Utils/Error.hpp>
#include <Drac++/Utils/Types.hpp>

namespace draconis::utils::cache {
  namespace {
    using error::DracError;
    using enum error::DracErrorCode;

    using types::Err;
    using types::Exception;
    using types::i32;
    using types::isize;
    using types::Result;
    using types::String;
    using types::StringView;

    namespace fs = std::filesystem;

    constexpr std::chrono::hours CACHE_EXPIRY_DURATION = std::chrono::hours(1);
  } // namespace

  /**
   * @brief Gets the full path for a cache file based on a unique key.
   * @param cache_key A unique identifier for the cache (e.g., "weather", "pkg_count_pacman").
   * Should ideally only contain filesystem-safe characters.
   * @return Result containing the filesystem path on success, or a DracError on failure.
   */
  inline fn GetCachePath(const String& cache_key) -> Result<fs::path> {
    if (cache_key.empty())
      return Err(DracError(InvalidArgument, "Cache key cannot be empty."));

    if (cache_key.find_first_of("/\\:*?\"<>|") != String::npos)
      return Err(DracError(InvalidArgument, std::format("Cache key '{}' contains invalid characters.", cache_key)));

    std::error_code errc;

    const fs::path tempDir = fs::temp_directory_path(errc);

    if (errc)
      return Err(DracError(IoError, "Failed to get system temporary directory: " + errc.message()));

    const fs::path cacheDir = tempDir / "draconis++";

    fs::create_directories(cacheDir, errc);

    if (errc)
      return Err(DracError(IoError, "Failed to create cache directory: " + errc.message()));

    const String filename = String(cache_key) + "_cache.beve";

    return cacheDir / filename;
  }

  /**
   * @brief Reads and deserializes data from a BEVE cache file.
   * @tparam T The type of the object to deserialize from the cache. Must be Glaze-compatible.
   * @param cache_key The unique identifier for the cache.
   * @return Result containing the deserialized object of type T on success, or a DracError on failure.
   */
  template <typename T>
  fn ReadCache(const String& cache_key) -> Result<T> {
    Result<fs::path> cachePathResult = GetCachePath(cache_key);
    if (!cachePathResult)
      return Err(cachePathResult.error());

    const fs::path& cachePath = *cachePathResult;

    std::ifstream ifs(cachePath, std::ios::binary);
    if (!ifs.is_open())
      return Err(DracError(IoError, "Failed to open cache file for reading: " + cachePath.string()));

    try {
      const String content(std::istreambuf_iterator<char>(ifs), {});

      if (content.empty())
        return Err(DracError(IoError, "Cache file is empty."));

      static_assert(std::is_default_constructible_v<T>, "Cache type T must be default constructible for Glaze.");
      T result {};

      if (glz::error_ctx glazeErr = glz::read_beve(result, content); glazeErr.ec != glz::error_code::none) {
        const String errorString = glz::format_error(glazeErr, StringView(content));
        return Err(DracError(IoError, std::format("BEVE parse error reading cache '{}' (code {}): {}", cachePath.string(), static_cast<i32>(glazeErr.ec), errorString)));
      }

      return result;
    } catch (const Exception& e) {
      return Err(DracError(InternalError, std::format("Standard exception reading cache file {}: {}", cachePath.string(), e.what())));
    } catch (...) {
      return Err(DracError(Other, "Unknown error reading cache file: " + cachePath.string()));
    }
  }

  /**
   * @brief Serializes and writes data to a BEVE cache file safely.
   * @tparam T The type of the object to serialize. Must be Glaze-compatible.
   * @param cache_key The unique identifier for the cache.
   * @param data The data object of type T to write to the cache.
   * @return Result containing void on success, or a DracError on failure.
   */
  template <typename T>
  fn WriteCache(const String& cache_key, const T& data) -> Result<> {
    Result<fs::path> cachePathResult = GetCachePath(cache_key);
    if (!cachePathResult)
      return Err(cachePathResult.error());

    const fs::path& cachePath = *cachePathResult;
    fs::path        tempPath  = cachePath;
    tempPath += ".tmp";

    struct TempFileGuard {
      fs::path path;
      bool     committed = false;

      explicit TempFileGuard(fs::path _path)
        : path(std::move(_path)) {}

      ~TempFileGuard() {
        if (!committed) {
          std::error_code errc;
          fs::remove(path, errc);
        }
      }

      TempFileGuard(const TempFileGuard&)                = delete;
      TempFileGuard(TempFileGuard&&)                     = delete;
      fn operator=(const TempFileGuard&)->TempFileGuard& = delete;
      fn operator=(TempFileGuard&&)->TempFileGuard&      = delete;
    };

    TempFileGuard guard { tempPath };

    try {
      String binaryBuffer;

      if (glz::error_ctx glazeErr = glz::write_beve(data, binaryBuffer); glazeErr) {
        const String errorString = glz::format_error(glazeErr, StringView(binaryBuffer));
        return Err(DracError(ParseError, std::format("BEVE serialization error for key '{}': {}", cache_key, errorString)));
      }

      std::ofstream ofs(tempPath, std::ios::binary | std::ios::trunc);
      if (!ofs)
        return Err(DracError(IoError, "Failed to open temporary cache file: " + tempPath.string()));

      ofs.write(binaryBuffer.data(), static_cast<isize>(binaryBuffer.size()));
      if (!ofs)
        return Err(DracError(IoError, "Failed to write to temporary cache file: " + tempPath.string()));

      ofs.close();

      std::error_code renameEc;

      fs::rename(tempPath, cachePath, renameEc);

      if (renameEc)
        return Err(DracError(IoError, std::format("Failed to replace cache file '{}': {}", cachePath.string(), renameEc.message())));

      guard.committed = true;

      return {};
    } catch (const Exception& e) {
      return Err(DracError(InternalError, std::format("Standard exception writing cache file {}: {}", tempPath.string(), e.what())));
    } catch (...) {
      return Err(DracError(Other, "Unknown error writing cache file: " + tempPath.string()));
    }
  }

  /**
   * @brief Checks if a cache file is valid and within the expiry duration, and if so, reads and returns its content.
   * @tparam T The type of the object to deserialize from the cache. Must be Glaze-compatible.
   * @param cache_key The unique identifier for the cache.
   * @return Result containing the deserialized object of type T on success, or a DracError on failure (e.g., not found, expired, parse error).
   */
  template <typename T>
  fn GetValidCache(const String& cache_key) -> Result<T> {
    Result<fs::path> cachePathResult = GetCachePath(cache_key);
    if (!cachePathResult)
      return Err(cachePathResult.error());

    const fs::path& cachePath = *cachePathResult;

    std::error_code errc;

    const auto lastWriteTime = fs::last_write_time(cachePath, errc);

    if (errc)
      return Err(DracError(NotFound, "Cache not found or is inaccessible: " + cachePath.string()));

    const auto now = std::chrono::file_clock::now();
    if ((now - lastWriteTime) > CACHE_EXPIRY_DURATION)
      return Err(DracError(NotFound, std::format("Cache expired: {}", cache_key)));

    return ReadCache<T>(cache_key);
  }
} // namespace draconis::utils::cache
