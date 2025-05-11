#pragma once

#include <filesystem>             // std::filesystem
#include <fstream>                // std::{ifstream, ofstream}
#include <glaze/beve/read.hpp>    // glz::read_beve
#include <glaze/beve/write.hpp>   // glz::write_beve
#include <glaze/core/context.hpp> // glz::{context, error_code, error_ctx}
#include <iterator>               // std::istreambuf_iterator
#include <system_error>           // std::error_code
#include <type_traits>            // std::decay_t

#include "Util/Definitions.hpp"
#include "Util/Error.hpp"
#include "Util/Logging.hpp"
#include "Util/Types.hpp"

namespace util::cache {
  namespace fs = std::filesystem;
  using error::DracError, error::DracErrorCode;
  using types::Err, types::Exception, types::Result, types::String, types::isize;

  /**
   * @brief Gets the full path for a cache file based on a unique key.
   * @param cache_key A unique identifier for the cache (e.g., "weather", "pkg_count_pacman").
   * Should ideally only contain filesystem-safe characters.
   * @return Result containing the filesystem path on success, or a DracError on failure.
   */
  inline fn GetCachePath(const String& cache_key) -> Result<fs::path> {
    if (cache_key.empty())
      return Err(DracError(DracErrorCode::InvalidArgument, "Cache key cannot be empty."));

    if (cache_key.find_first_of("/\\:*?\"<>|") != String::npos)
      return Err(
        DracError(DracErrorCode::InvalidArgument, std::format("Cache key '{}' contains invalid characters.", cache_key))
      );

    std::error_code errc;

    const fs::path cacheDir = fs::temp_directory_path(errc) / "draconis++";

    if (!fs::exists(cacheDir, errc)) {
      if (errc)
        return Err(DracError(DracErrorCode::IoError, "Failed to check existence of cache directory: " + errc.message()));

      fs::create_directories(cacheDir, errc);

      if (errc)
        return Err(DracError(DracErrorCode::IoError, "Failed to create cache directory: " + errc.message()));
    }

    if (errc)
      return Err(DracError(DracErrorCode::IoError, "Failed to get system temporary directory: " + errc.message()));

    return cacheDir / (cache_key + "_cache.beve");
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

    if (std::error_code existsEc; !fs::exists(cachePath, existsEc) || existsEc) {
      if (existsEc)
        warn_log("Error checking existence of cache file '{}': {}", cachePath.string(), existsEc.message());

      return Err(DracError(DracErrorCode::NotFound, "Cache file not found: " + cachePath.string()));
    }

    std::ifstream ifs(cachePath, std::ios::binary);
    if (!ifs.is_open())
      return Err(DracError(DracErrorCode::IoError, "Failed to open cache file for reading: " + cachePath.string()));

    try {
      const String content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
      ifs.close();

      if (content.empty())
        return Err(DracError(DracErrorCode::ParseError, "BEVE cache file is empty: " + cachePath.string()));

      static_assert(std::is_default_constructible_v<T>, "Cache type T must be default constructible for Glaze.");
      T result {};

      if (glz::error_ctx glazeErr = glz::read_beve(result, content); glazeErr.ec != glz::error_code::none) {
        return Err(DracError(
          DracErrorCode::ParseError,
          std::format(
            "BEVE parse error reading cache '{}' (code {}): {}",
            cachePath.string(),
            static_cast<int>(glazeErr.ec),
            glz::format_error(glazeErr, content)
          )
        ));
      }

      return result;
    } catch (const std::ios_base::failure& e) {
      return Err(DracError(
        DracErrorCode::IoError, std::format("Filesystem error reading cache file {}: {}", cachePath.string(), e.what())
      ));
    } catch (const Exception& e) {
      return Err(DracError(
        DracErrorCode::InternalError,
        std::format("Standard exception reading cache file {}: {}", cachePath.string(), e.what())
      ));
    } catch (...) {
      return Err(DracError(DracErrorCode::Other, "Unknown error reading cache file: " + cachePath.string()));
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

    try {
      String binaryBuffer;

      using DecayedT           = std::decay_t<T>;
      DecayedT dataToSerialize = data;

      if (glz::error_ctx glazeErr = glz::write_beve(dataToSerialize, binaryBuffer); glazeErr) {
        return Err(DracError(
          DracErrorCode::ParseError,
          std::format(
            "BEVE serialization error writing cache for key '{}' (code {}): {}",
            cache_key,
            static_cast<int>(glazeErr.ec),
            glz::format_error(glazeErr, binaryBuffer)
          )
        ));
      }

      {
        std::ofstream ofs(tempPath, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open())
          return Err(DracError(DracErrorCode::IoError, "Failed to open temporary cache file: " + tempPath.string()));

        ofs.write(binaryBuffer.data(), static_cast<isize>(binaryBuffer.size()));

        if (!ofs) {
          std::error_code removeEc;
          fs::remove(tempPath, removeEc);
          return Err(DracError(DracErrorCode::IoError, "Failed to write to temporary cache file: " + tempPath.string()));
        }
      }

      std::error_code renameEc;
      fs::rename(tempPath, cachePath, renameEc);
      if (renameEc) {
        std::error_code removeEc;
        fs::remove(tempPath, removeEc);
        return Err(DracError(
          DracErrorCode::IoError,
          std::format(
            "Failed to replace cache file '{}' with temporary file '{}': {}",
            cachePath.string(),
            tempPath.string(),
            renameEc.message()
          )
        ));
      }

      return {};
    } catch (const std::ios_base::failure& e) {
      std::error_code removeEc;
      fs::remove(tempPath, removeEc);
      return Err(DracError(
        DracErrorCode::IoError, std::format("Filesystem error writing cache file {}: {}", tempPath.string(), e.what())
      ));
    } catch (const Exception& e) {
      std::error_code removeEc;
      fs::remove(tempPath, removeEc);
      return Err(DracError(
        DracErrorCode::InternalError,
        std::format("Standard exception writing cache file {}: {}", tempPath.string(), e.what())
      ));
    } catch (...) {
      std::error_code removeEc;
      fs::remove(tempPath, removeEc);
      return Err(DracError(DracErrorCode::Other, "Unknown error writing cache file: " + tempPath.string()));
    }
  }
} // namespace util::cache
