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

    fn GetDate() -> Result<String> {
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

        ERR(ParseError, "Failed to format date");
      }

      ERR(ParseError, "Failed to get local time");
    }
  } // namespace

  SystemInfo::SystemInfo(utils::cache::CacheManager& cache, const Config& config) {
    // I'm not sure if AMD uses trademark symbols in their CPU models, but I know
    // Intel does. Might as well replace them with their unicode counterparts.
    auto replaceTrademarkSymbols = [](Result<String> str) -> Result<String> {
      if (!str)
        ERR_FROM(str.error());

      usize pos = 0;

      while ((pos = str->find("(TM)")) != String::npos)
        str->replace(pos, 4, "™");

      while ((pos = str->find("(R)")) != String::npos)
        str->replace(pos, 3, "®");

      return str;
    };

    this->desktopEnv    = GetDesktopEnvironment(cache);
    this->windowMgr     = GetWindowManager(cache);
    this->osVersion     = GetOSVersion(cache);
    this->kernelVersion = GetKernelVersion(cache);
    this->host          = GetHost(cache);
    this->cpuModel      = replaceTrademarkSymbols(GetCPUModel(cache));
    this->cpuCores      = GetCPUCores(cache);
    this->gpuModel      = GetGPUModel(cache);
    this->shell         = GetShell(cache);
    this->memInfo       = GetMemInfo(cache);
    this->diskUsage     = GetDiskUsage(cache);
    this->uptime        = GetUptime();
    this->date          = GetDate();

#if DRAC_ENABLE_PACKAGECOUNT
    this->packageCount = draconis::services::packages::GetTotalCount(cache, config.enabledPackageManagers);
#endif

#if DRAC_ENABLE_NOWPLAYING
    this->nowPlaying = config.nowPlaying.enabled ? GetNowPlaying() : Err(DracError(ApiUnavailable, "Now Playing API disabled"));
#endif
  }
} // namespace draconis::core::system
