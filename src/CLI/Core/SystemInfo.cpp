#include "SystemInfo.hpp"

#include <Drac++/Core/System.hpp>

#include <Drac++/Utils/Error.hpp>
#include <Drac++/Utils/Logging.hpp>
#include <Drac++/Utils/Types.hpp>

#include "Config/Config.hpp"

namespace draconis::core::system {
  namespace {
    using draconis::config::Config;
    using namespace draconis::utils::types;

    using draconis::utils::error::DracError;
    using enum draconis::utils::error::DracErrorCode;

    fn getDate() -> Result<String> {
      using std::chrono::system_clock;

      const system_clock::time_point nowTp = system_clock::now();
      const std::time_t              nowTt = system_clock::to_time_t(nowTp);

      std::tm nowTm;

#ifdef _WIN32
      if (localtime_s(&nowTm, &nowTt) == 0) {
#else
      if (localtime_r(&nowTt, &nowTm) != nullptr) {
#endif
        i32 day = nowTm.tm_mday;

        String monthBuffer(32, '\0');

        if (const usize monthLen = std::strftime(monthBuffer.data(), monthBuffer.size(), "%B", &nowTm); monthLen > 0) {
          using matchit::match, matchit::is, matchit::_, matchit::in;

          monthBuffer.resize(monthLen);

          PCStr suffix = match(day)(
            is | in(11, 13)    = "th",
            is | (_ % 10 == 1) = "st",
            is | (_ % 10 == 2) = "nd",
            is | (_ % 10 == 3) = "rd",
            is | _             = "th"
          );

          return std::format("{} {}{}", monthBuffer, day, suffix);
        }

        return Err(DracError(ParseError, "Failed to format date"));
      }

      return Err(DracError(ParseError, "Failed to get local time"));
    }
  } // namespace

  SystemInfo::SystemInfo(utils::cache::CacheManager& cache, const Config& config) {
    using enum std::launch;

    // I'm not sure if AMD uses trademark symbols in their CPU models, but I know
    // Intel does. Might as well replace them with their unicode counterparts.
    fn replaceTrademarkSymbols = [](Result<String> str) -> Result<String> {
      if (!str)
        return Err(str.error());

      usize pos = 0;

      while ((pos = str->find("(TM)")) != String::npos)
        str->replace(pos, 4, "™");

      while ((pos = str->find("(R)")) != String::npos)
        str->replace(pos, 3, "®");

      return str;
    };

    // Use batch operations for related information
    Future<Result<String>>               desktopEnvFut = std::async(async, [&cache]() { return GetDesktopEnvironment(cache); });
    Future<Result<String>>               windowMgrFut  = std::async(async, [&cache]() { return GetWindowManager(cache); });
    Future<Result<String>>               osFut         = std::async(async, [&cache]() { return GetOSVersion(cache); });
    Future<Result<String>>               kernelFut     = std::async(async, [&cache]() { return GetKernelVersion(cache); });
    Future<Result<String>>               hostFut       = std::async(async, [&cache]() { return GetHost(cache); });
    Future<Result<String>>               cpuFut        = std::async(async, [&cache]() { return GetCPUModel(cache); });
    Future<Result<CPUCores>>             cpuCoresFut   = std::async(async, [&cache]() { return GetCPUCores(cache); });
    Future<Result<String>>               gpuFut        = std::async(async, [&cache]() { return GetGPUModel(cache); });
    Future<Result<String>>               shellFut      = std::async(async, [&cache]() { return GetShell(cache); });
    Future<Result<ResourceUsage>>        memFut        = std::async(async, &GetMemInfo);
    Future<Result<ResourceUsage>>        diskFut       = std::async(async, &GetDiskUsage);
    Future<Result<String>>               dateFut       = std::async(async, &getDate);
    Future<Result<std::chrono::seconds>> uptimeFut     = std::async(async, &GetUptime);

#if DRAC_ENABLE_PACKAGECOUNT
    Future<Result<u64>> pkgFut = std::async(async, [&cache, &config]() { return draconis::services::packages::GetTotalCount(cache, config.enabledPackageManagers); });
#endif

#if DRAC_ENABLE_NOWPLAYING
    Future<Result<MediaInfo>> npFut = std::async(config.nowPlaying.enabled ? async : deferred, &GetNowPlaying);
#endif

    this->desktopEnv    = desktopEnvFut.get();
    this->windowMgr     = windowMgrFut.get();
    this->osVersion     = osFut.get();
    this->kernelVersion = kernelFut.get();
    this->host          = hostFut.get();
    this->cpuModel      = replaceTrademarkSymbols(cpuFut.get());
    this->cpuCores      = cpuCoresFut.get();
    this->gpuModel      = gpuFut.get();
    this->shell         = shellFut.get();
    this->memInfo       = memFut.get();
    this->diskUsage     = diskFut.get();
    this->uptime        = uptimeFut.get();
    this->date          = dateFut.get();

#if DRAC_ENABLE_PACKAGECOUNT
    this->packageCount = pkgFut.get();
#endif

#if DRAC_ENABLE_NOWPLAYING
    this->nowPlaying = config.nowPlaying.enabled ? npFut.get() : Err(DracError(ApiUnavailable, "Now Playing API disabled"));
#endif
  }
} // namespace draconis::core::system
