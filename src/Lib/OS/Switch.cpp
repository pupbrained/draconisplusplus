#ifdef __SWITCH__

  #include <bit>
  #include <chrono>
  #include <format>
  #include <switch.h>
  #include <switch/kernel/detect.h>
  #include <switch/services/psm.h>
  #include <switch/services/set.h>
  #include <switch/services/spl.h>

  #include <Drac++/Core/System.hpp>

  #include <Drac++/Utils/CacheManager.hpp>
  #include <Drac++/Utils/Error.hpp>
  #include <Drac++/Utils/Types.hpp>

namespace draconis::core::system {
  using namespace draconis::utils::types;
  using draconis::utils::error::DracError;
  using enum draconis::utils::error::DracErrorCode;

  // Helper macro to quickly stub a function
  #define DRAC_SWITCH_STUB(func_name) \
    ERR(NotSupported, std::format("{} not implemented on Nintendo Switch", #func_name))

  fn GetMemInfo(CacheManager& /*cache*/) -> Result<ResourceUsage> {
    u64 total = 0;
    u64 used  = 0;

    u32 rcTot = svcGetInfo(&total, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
    if (R_FAILED(rcTot))
      ERR(ApiUnavailable, std::format("svcGetInfo TotalMemorySize failed: 0x{:08X}", rcTot));

    u32 rcUsed = svcGetInfo(&used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);
    if (R_FAILED(rcUsed))
      ERR(ApiUnavailable, std::format("svcGetInfo UsedMemorySize failed: 0x{:08X}", rcUsed));

    return ResourceUsage { .usedBytes = used, .totalBytes = total };
  }

  fn GetNowPlaying() -> Result<MediaInfo> {
    DRAC_SWITCH_STUB(GetNowPlaying);
  }

  fn GetOSVersion(CacheManager& cache) -> Result<String> {
    return cache.getOrSet<String>("switch_os_version", []() -> Result<String> {
      SetSysFirmwareVersion fw = {};

      if (u32 rc = setsysInitialize(); R_FAILED(rc))
        ERR(ApiUnavailable, std::format("setsysInitialize failed: 0x{:08X}", rc));

      u32 rc = setsysGetFirmwareVersion(&fw);
      setsysExit();

      if (R_FAILED(rc))
        ERR(ApiUnavailable, std::format("setsysGetFirmwareVersion failed: 0x{:08X}", rc));

      return String(fw.display_version);
    });
  }

  fn GetDesktopEnvironment(CacheManager& /*cache*/) -> Result<String> {
    DRAC_SWITCH_STUB(GetDesktopEnvironment);
  }

  fn GetWindowManager(CacheManager& /*cache*/) -> Result<String> {
    DRAC_SWITCH_STUB(GetWindowManager);
  }

  fn GetShell(CacheManager& /*cache*/) -> Result<String> {
    DRAC_SWITCH_STUB(GetShell);
  }

  fn GetHost(CacheManager& /*cache*/) -> Result<String> {
    DRAC_SWITCH_STUB(GetHost);
  }

  fn GetCPUModel(CacheManager& cache) -> Result<String> {
    return cache.getOrSet<String>("switch_cpu_model", []() -> Result<String> {
      if (u32 rc = splInitialize(); R_FAILED(rc))
        ERR(ApiUnavailable, std::format("splInitialize failed: 0x{:08X}", rc));

      u64 hwType = 0;
      u32 rc     = splGetConfig(SplConfigItem_NewHardwareType, &hwType);
      splExit();

      if (R_FAILED(rc))
        ERR(ApiUnavailable, std::format("splGetConfig(NewHardwareType) failed: 0x{:08X}", rc));

      const char* modelName = "Unknown";
      switch (hwType) {
        case 0:
          modelName = "T210, Erista";
          break;
        case 2:
          modelName = "T210B01, Mariko";
          break;
        case 3:
          modelName = "T210B01, Aula";
          break;
        default:
          break;
      }

      return String(std::format("Tegra X1 ({})", modelName));
    });
  }

  fn GetCPUCores(CacheManager& cache) -> Result<CPUCores> {
    return cache.getOrSet<CPUCores>("switch_cpu_cores", []() -> Result<CPUCores> {
      u64 coreMask = 0;

      u32 rc = svcGetInfo(&coreMask, InfoType_CoreMask, CUR_PROCESS_HANDLE, 0);
      if (R_FAILED(rc))
        ERR(ApiUnavailable, std::format("svcGetInfo CoreMask failed: 0x{:08X}", rc));

      const usize cores = static_cast<usize>(std::popcount(coreMask));
      return CPUCores { cores, cores };
    });
  }

  fn GetGPUModel(CacheManager& /*cache*/) -> Result<String> {
    return "Tegra X1 - Hello from draconis++!";
  }

  fn GetKernelVersion(CacheManager& cache) -> Result<String> {
    return cache.getOrSet<String>("switch_kernel", []() -> Result<String> {
      const bool isMesosphere = detectMesosphere();

      return String(isMesosphere ? "Mesosphere" : "Horizon");
    });
  }

  fn GetDiskUsage(CacheManager& /*cache*/) -> Result<ResourceUsage> {
    DRAC_SWITCH_STUB(GetDiskUsage);
  }

  fn GetUptime() -> Result<std::chrono::seconds> {
    const u64 ticks = armGetSystemTick();
    const u64 freq  = armGetSystemTickFreq(); // Hz, usually 19.2 MHz

    if (freq == 0)
      ERR(PlatformSpecific, "armGetSystemTickFreq returned 0");

    return std::chrono::seconds(ticks / freq);
  }

  fn GetOutputs(CacheManager& /*cache*/) -> Result<Vec<DisplayInfo>> {
    DRAC_SWITCH_STUB(GetOutputs);
  }

  fn GetPrimaryOutput(CacheManager& /*cache*/) -> Result<DisplayInfo> {
    DRAC_SWITCH_STUB(GetPrimaryOutput);
  }

  fn GetNetworkInterfaces(CacheManager& /*cache*/) -> Result<Vec<NetworkInterface>> {
    DRAC_SWITCH_STUB(GetNetworkInterfaces);
  }

  fn GetPrimaryNetworkInterface(CacheManager& /*cache*/) -> Result<NetworkInterface> {
    DRAC_SWITCH_STUB(GetPrimaryNetworkInterface);
  }

  fn GetBatteryInfo(CacheManager& /*cache*/) -> Result<Battery> {
    if (u32 rc = psmInitialize(); R_FAILED(rc))
      ERR(ApiUnavailable, std::format("psmInitialize failed: 0x{:08X}", rc));

    u32 percentage = 0;
    if (u32 rc = psmGetBatteryChargePercentage(&percentage); R_FAILED(rc)) {
      psmExit();
      ERR(ApiUnavailable, std::format("psmGetBatteryChargePercentage failed: 0x{:08X}", rc));
    }

    PsmChargerType chargerType = PsmChargerType_Unconnected;
    if (u32 rc = psmGetChargerType(&chargerType); R_FAILED(rc)) {
      psmExit();
      ERR(ApiUnavailable, std::format("psmGetChargerType failed: 0x{:08X}", rc));
    }

    Battery::Status status = Battery::Status::Unknown;
    switch (chargerType) {
      case PsmChargerType_Unconnected:
        status = Battery::Status::Discharging;
        break;
      case PsmChargerType_EnoughPower:
      case PsmChargerType_LowPower:
        status = (percentage >= 100) ? Battery::Status::Full : Battery::Status::Charging;
        break;
      case PsmChargerType_NotSupported:
      default:
        status = Battery::Status::Unknown;
        break;
    }

    Battery battery {
      status,
      Some(static_cast<u8>(percentage)),
      None // libnx/PSM does not expose time-remaining information
    };

    psmExit();

    return battery;
  }

  #undef DRAC_SWITCH_STUB
} // namespace draconis::core::system

#endif // __SWITCH__