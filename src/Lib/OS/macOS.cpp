#ifdef __APPLE__
  #include <CoreFoundation/CFPropertyList.h> // CFPropertyListCreateWithData, kCFPropertyListImmutable
  #include <CoreFoundation/CFStream.h>       // CFReadStreamClose, CFReadStreamCreateWithFile, CFReadStreamOpen, CFReadStreamRead, CFReadStreamRef
  #include <CoreGraphics/CGDirectDisplay.h>  // CGDisplayCopyDeviceDescription, CGDisplayCopyDisplayMode, CGDisplayIsMain, CGDisplayModeGetMaximumRefreshRate, CGDisplayModeGetRefreshRate, CGDisplayPixelsHigh, CGDisplayPixelsWide, CGDisplayRef, CGDisplayModeRef, CGDirectDisplayID
  #include <IOKit/IOKitLib.h>                // IOKit types and functions
  #include <IOKit/ps/IOPSKeys.h>
  #include <IOKit/ps/IOPowerSources.h>
  #include <algorithm> // std::ranges::equal
  #include <arpa/inet.h>
  #include <flat_map> // std::flat_map
  #include <ifaddrs.h>
  #include <mach/mach_host.h>     // host_statistics64
  #include <mach/mach_init.h>     // host_page_size, mach_host_self
  #include <mach/vm_statistics.h> // vm_statistics64_data_t
  #include <net/if.h>
  #include <net/if_dl.h>
  #include <net/route.h>
  #include <netdb.h>
  #include <netinet/in.h>
  #include <sys/statvfs.h> // statvfs
  #include <sys/sysctl.h>  // {CTL_KERN, KERN_PROC, KERN_PROC_ALL, kinfo_proc, sysctl, sysctlbyname}

  #include <Drac++/Core/System.hpp>
  #include <Drac++/Services/Packages.hpp>

  #include <Drac++/Utils/Caching.hpp>
  #include <Drac++/Utils/Definitions.hpp>
  #include <Drac++/Utils/Env.hpp>
  #include <Drac++/Utils/Error.hpp>
  #include <Drac++/Utils/Logging.hpp>
  #include <Drac++/Utils/Types.hpp>

  #include "OS/macOS/Bridge.hpp"

using namespace draconis::utils::types;

using draconis::utils::env::GetEnv;
using draconis::utils::error::DracError;
using enum draconis::utils::error::DracErrorCode;

namespace {
  fn getDisplayInfoById(CGDirectDisplayID displayID) -> Result<Display> {
    // Get display resolution
    const usize width  = CGDisplayPixelsWide(displayID);
    const usize height = CGDisplayPixelsHigh(displayID);

    if (width == 0 || height == 0)
      return Err(DracError(PlatformSpecific, "Failed to get display resolution."));

    // Get the current display mode to find the refresh rate
    CGDisplayModeRef currentMode = CGDisplayCopyDisplayMode(displayID);
    if (currentMode == nullptr)
      return Err(DracError(PlatformSpecific, "Failed to get display mode."));

    f64 refreshRate = CGDisplayModeGetRefreshRate(currentMode);

    // Release the display mode object
    CGDisplayModeRelease(currentMode);

    // Check if this is the main display
    const bool isPrimary = displayID == CGMainDisplayID();

    return Display(
      displayID,
      { .width = static_cast<u16>(width), .height = static_cast<u16>(height) },
      static_cast<u16>(refreshRate),
      isPrimary
    );
  }

  template <typename T>
  fn getNumericValue(const CFDictionaryRef dict, const CFStringRef key) -> Option<T> {
    const auto* value = static_cast<const CFNumberRef>(CFDictionaryGetValue(dict, key));

    if (value == nullptr)
      return None;

    int64_t intermediateResult = 0;

    if (CFNumberGetValue(value, kCFNumberSInt64Type, &intermediateResult))
      if (intermediateResult >= std::numeric_limits<T>::min() && intermediateResult <= std::numeric_limits<T>::max())
        return static_cast<T>(intermediateResult);

    return None;
  }
} // namespace

namespace draconis::core::system {
  fn GetMemInfo() -> Result<ResourceUsage> {
    // Mach ports are used for communicating with the kernel. mach_host_self
    // provides a port to the host kernel, which is needed for host-level queries.
    static mach_port_t HostPort = mach_host_self();

    // The size of a virtual memory page in bytes.
    static vm_size_t PageSize = 0;

    // Make sure the page size is set.
    if (PageSize == 0)
      if (host_page_size(HostPort, &PageSize) != KERN_SUCCESS)
        return Err(DracError("Failed to get page size"));

    u64   totalMem = 0;
    usize size     = sizeof(totalMem);

    // "hw.memsize" is the standard key for getting the total memory size from the system.
    if (sysctlbyname("hw.memsize", &totalMem, &size, nullptr, 0) == -1)
      return Err(DracError("Failed to get total memory info"));

    vm_statistics64_data_t vmStats;
    mach_msg_type_number_t infoCount = sizeof(vmStats) / sizeof(natural_t);

    // Retrieve detailed virtual memory statistics from the Mach kernel.
    // The `reinterpret_cast` is required here to interface with the C-style
    // Mach API, which expects a generic `host_info64_t` pointer.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    if (host_statistics64(HostPort, HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&vmStats), &infoCount) != KERN_SUCCESS)
      return Err(DracError("Failed to get memory statistics"));

    // Calculate "used" memory based on the statistics returned by host_statistics64.
    // - active_count: Memory that is actively being used by the process.
    // - wire_count: Memory that is wired to physical memory.
    u64 usedMem = (vmStats.active_count + vmStats.wire_count) * PageSize;

    return ResourceUsage {
      .usedBytes  = usedMem,
      .totalBytes = totalMem
    };
  }

  fn GetNowPlaying() -> Result<MediaInfo> {
    return macOS::GetNowPlayingInfo();
  }

  fn GetOSVersion(draconis::utils::cache::CacheManager& cache) -> Result<String> {
    return cache.getOrSet<String>("macos_os_version", []() -> Result<String> {
        return macOS::GetOSVersion();
    });
  }

  fn GetDesktopEnvironment(draconis::utils::cache::CacheManager& cache) -> Result<String> {
    return cache.getOrSet<String>("macos_desktop_environment", []() -> Result<String> {
        return "Aqua";
    });
  }

  fn GetWindowManager(draconis::utils::cache::CacheManager& cache) -> Result<String> {
    return cache.getOrSet<String>("macos_wm", []() -> Result<String> {
        constexpr Array<StringView, 5> knownWms = {
            "Yabai",
            "ChunkWM",
            "Amethyst",
            "Spectacle",
            "Rectangle",
        };

        Array<i32, 3> request = { CTL_KERN, KERN_PROC, KERN_PROC_ALL };

        usize len = 0;

        if (sysctl(request.data(), request.size(), nullptr, &len, nullptr, 0) == -1)
            return Err(DracError("sysctl size query failed for KERN_PROC_ALL"));

        if (len == 0)
            return Err(DracError(NotFound, "sysctl for KERN_PROC_ALL returned zero length"));

        Vec<char> buf(len);

        if (sysctl(request.data(), request.size(), buf.data(), &len, nullptr, 0) == -1)
            return Err(DracError("sysctl data fetch failed for KERN_PROC_ALL"));

        if (len % sizeof(kinfo_proc) != 0)
            return Err(DracError(
                PlatformSpecific,
                std::format("sysctl returned size {} which is not a multiple of kinfo_proc size {}", len, sizeof(kinfo_proc))
            ));

        usize count = len / sizeof(kinfo_proc);

        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        Span<const kinfo_proc> processes(reinterpret_cast<const kinfo_proc*>(buf.data()), count);

        for (const kinfo_proc& procInfo : processes)
            for (const StringView& wmName : knownWms)
                if (std::ranges::equal(StringView(procInfo.kp_proc.p_comm), wmName, [](char chrA, char chrB) {
                    return std::tolower(static_cast<unsigned char>(chrA)) == std::tolower(static_cast<unsigned char>(chrB));
                })) {
                    return String(wmName);
                }

        return "Quartz";
    });
  }

  fn GetKernelVersion(draconis::utils::cache::CacheManager& cache) -> Result<String> {
    return cache.getOrSet<String>("macos_kernel", []() -> Result<String> {
        Array<char, 256> kernelVersion {};
        usize kernelVersionLen = kernelVersion.size();
        if (sysctlbyname("kern.osrelease", kernelVersion.data(), &kernelVersionLen, nullptr, 0) == -1)
            return Err(DracError("Failed to get kernel version"));
        return String(kernelVersion.data());
    });
  }

  fn GetHost(draconis::utils::cache::CacheManager& cache) -> Result<String> {
    return cache.getOrSet<String>("macos_host", []() -> Result<String> {
        Array<char, 256> hwModel {};
        usize hwModelLen = hwModel.size();
        if (sysctlbyname("hw.model", hwModel.data(), &hwModelLen, nullptr, 0) == -1)
            return Err(DracError("Failed to get host info"));

        static const std::flat_map<StringView, StringView> MODEL_NAME_BY_HW_MODEL = {
            // ... (map data) ...
        };

        const auto iter = MODEL_NAME_BY_HW_MODEL.find(hwModel.data());
        if (iter == MODEL_NAME_BY_HW_MODEL.end())
            return Err(DracError("Failed to get host info"));

        return String(iter->second);
    });
  }

  fn GetCPUModel(draconis::utils::cache::CacheManager& cache) -> Result<String> {
    return cache.getOrSet<String>("macos_cpu_model", []() -> Result<String> {
        Array<char, 256> cpuModel {};
        usize cpuModelLen = cpuModel.size();
        if (sysctlbyname("machdep.cpu.brand_string", cpuModel.data(), &cpuModelLen, nullptr, 0) == -1)
            return Err(DracError("Failed to get CPU model"));
        return String(cpuModel.data());
    });
  }

  fn GetCPUCores(draconis::utils::cache::CacheManager& cache) -> Result<CPUCores> {
    return cache.getOrSet<CPUCores>("macos_cpu_cores", []() -> Result<CPUCores> {
        u32   physicalCores = 0;
        u32   logicalCores  = 0;
        usize size          = sizeof(u32);

        if (sysctlbyname("hw.physicalcpu", &physicalCores, &size, nullptr, 0) == -1)
            return Err(DracError("sysctlbyname for hw.physicalcpu failed"));

        size = sizeof(u32);

        if (sysctlbyname("hw.logicalcpu", &logicalCores, &size, nullptr, 0) == -1)
            return Err(DracError("sysctlbyname for hw.logicalcpu failed"));

        debug_log("Physical cores: {}", physicalCores);
        debug_log("Logical cores: {}", logicalCores);

        return CPUCores(physicalCores, logicalCores);
    });
  }

  fn GetGPUModel(draconis::utils::cache::CacheManager& cache) -> Result<String> {
    return cache.getOrSet<String>("macos_gpu", []() -> Result<String> {
        const Result<String> gpuModel = macOS::GetGPUModel();
        if (!gpuModel)
            return Err(DracError("Failed to get GPU model"));
        return *gpuModel;
    }, draconis::utils::cache::CachePolicy::NeverExpire());
  }

  fn GetDiskUsage() -> Result<ResourceUsage> {
    struct statvfs vfs;

    if (statvfs("/", &vfs) != 0)
      return Err(DracError("Failed to get disk usage"));

    return ResourceUsage {
      .usedBytes  = (vfs.f_blocks - vfs.f_bfree) * vfs.f_frsize,
      .totalBytes = vfs.f_blocks * vfs.f_frsize,
    };
  }

  fn GetShell(draconis::utils::cache::CacheManager& cache) -> Result<String> {
    return cache.getOrSet<String>("macos_shell", []() -> Result<String> {
        if (const Result<String> shellPath = GetEnv("SHELL")) {
            // clang-format off
            constexpr Array<Pair<StringView, StringView>, 8> shellMap {{
                { "bash", "Bash"      },
                { "zsh",  "Zsh"       },
                { "ksh",  "KornShell" },
                { "fish", "Fish"      },
                { "tcsh", "TCsh"      },
                { "csh",  "Csh"       },
                { "sh",   "Sh"        },
                { "nu",   "NuShell"   },
            }};
            // clang-format on

            for (const auto& [exe, name] : shellMap)
                if (shellPath->ends_with(exe))
                    return String(name);

            return *shellPath;
        }
        return Err(DracError(NotFound, "Could not find SHELL environment variable"));
    });
  }

  fn GetUptime() -> Result<std::chrono::seconds> {
    using namespace std::chrono;

    Array<i32, 2> mib = { CTL_KERN, KERN_BOOTTIME };

    struct timeval boottime;
    usize          len = sizeof(boottime);

    if (sysctl(mib.data(), mib.size(), &boottime, &len, nullptr, 0) == -1)
      return Err(DracError("Failed to get system boot time using sysctl"));

    const system_clock::time_point bootTimepoint = system_clock::from_time_t(boottime.tv_sec);

    const system_clock::time_point now = system_clock::now();

    return duration_cast<seconds>(now - bootTimepoint);
  }

  fn GetPrimaryDisplay() -> Result<Display> {
    return getDisplayInfoById(CGMainDisplayID());
  }

  fn GetDisplays() -> Result<Vec<Display>> {
    u32 displayCount = 0;

    if (CGGetOnlineDisplayList(0, nullptr, &displayCount) != kCGErrorSuccess)
      return Err(DracError(ApiUnavailable, "Failed to get display count."));

    if (displayCount == 0)
      return Err(DracError(NotFound, "No displays found"));

    Vec<CGDirectDisplayID> displayIDs(displayCount);

    if (CGGetOnlineDisplayList(displayCount, displayIDs.data(), &displayCount) != kCGErrorSuccess)
      return Err(DracError(ApiUnavailable, "Failed to get display list."));

    Vec<Display> displays;
    displays.reserve(displayCount);

    for (const CGDirectDisplayID displayID : displayIDs)
      if (Result<Display> display = getDisplayInfoById(displayID); display)
        displays.push_back(*display);

    return displays;
  }

  fn GetPrimaryNetworkInterface(draconis::utils::cache::CacheManager& cache) -> Result<NetworkInterface> {
    return cache.getOrSet<NetworkInterface>("macos_primary_network_interface", []() -> Result<NetworkInterface> {
        // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast) - This requires a lot of casts and there's no good way to avoid them.
        Array<i32, 6> mib = { CTL_NET, PF_ROUTE, 0, AF_INET, NET_RT_FLAGS, RTF_GATEWAY };
        usize         len = 0;

        if (sysctl(mib.data(), mib.size(), nullptr, &len, nullptr, 0) == -1)
            return Err(DracError("sysctl(1) failed to get routing table size"));

        Vec<char> buf(len);

        if (sysctl(mib.data(), mib.size(), buf.data(), &len, nullptr, 0) == -1)
            return Err(DracError("sysctl(2) failed to get routing table dump"));

        String primaryInterfaceName;
        for (usize offset = 0; offset < len;) {
            const auto* rtm   = reinterpret_cast<const rt_msghdr*>(&buf[offset]);
            const auto* saddr = reinterpret_cast<const sockaddr*>(std::next(rtm));

            if (rtm->rtm_msglen == 0)
                break;

            if (saddr->sa_family == AF_INET && rtm->rtm_addrs & RTA_DST)
                if (reinterpret_cast<const sockaddr_in*>(saddr)->sin_addr.s_addr == 0) {
                    Array<char, IF_NAMESIZE> ifName = {};
                    if (if_indextoname(rtm->rtm_index, ifName.data()) != nullptr) {
                        primaryInterfaceName = String(ifName.data());
                        break;
                    }
                }

            offset += rtm->rtm_msglen;
        }

        if (primaryInterfaceName.empty())
            return Err(DracError(NotFound, "Could not determine primary interface name from routing table."));

        ifaddrs* ifaddrList = nullptr;
        if (getifaddrs(&ifaddrList) == -1)
            return Err(DracError("getifaddrs failed"));

        UniquePointer<ifaddrs, decltype(&freeifaddrs)> ifaddrsDeleter(ifaddrList, &freeifaddrs);

        NetworkInterface primaryInterface;
        primaryInterface.name = primaryInterfaceName;
        bool foundDetails     = false;

        for (ifaddrs* ifa = ifaddrList; ifa != nullptr; ifa = ifa->ifa_next) {
            // Skip any entries that don't match our primary interface name
            if (ifa->ifa_addr == nullptr || primaryInterfaceName != ifa->ifa_name)
                continue;

            foundDetails = true;

            // Set flags
            primaryInterface.isUp       = ifa->ifa_flags & IFF_UP;
            primaryInterface.isLoopback = ifa->ifa_flags & IFF_LOOPBACK;

            // Get IPv4 details
            if (ifa->ifa_addr->sa_family == AF_INET) {
                Array<char, NI_MAXHOST> host = {};
                if (getnameinfo(ifa->ifa_addr, sizeof(sockaddr_in), host.data(), host.size(), nullptr, 0, NI_NUMERICHOST) == 0)
                    primaryInterface.ipv4Address = String(host.data());
            } else if (ifa->ifa_addr->sa_family == AF_LINK) {
                auto* sdl = reinterpret_cast<sockaddr_dl*>(ifa->ifa_addr);

                if (sdl && sdl->sdl_alen == 6) {
                    const auto* macPtr = reinterpret_cast<const u8*>(LLADDR(sdl));

                    const Span<const u8> macAddr(macPtr, sdl->sdl_alen);

                    primaryInterface.macAddress = std::format(
                        "{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
                        macAddr[0],
                        macAddr[1],
                        macAddr[2],
                        macAddr[3],
                        macAddr[4],
                        macAddr[5]
                    );
                }
            }
        }

        if (!foundDetails)
            return Err(DracError(NotFound, "Found primary interface name, but could not find its details via getifaddrs."));

        return primaryInterface;
        // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)
    });
  }

  fn GetNetworkInterfaces() -> Result<Vec<NetworkInterface>> {
    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast) - This requires a lot of casts and there's no good way to avoid them.
    ifaddrs* ifaddrList = nullptr;
    if (getifaddrs(&ifaddrList) == -1)
      return Err(DracError("getifaddrs failed"));

    UniquePointer<ifaddrs, decltype(&freeifaddrs)> ifaddrsDeleter(ifaddrList, &freeifaddrs);

    // Use a map to collect interface information since getifaddrs returns multiple entries per interface
    std::flat_map<String, NetworkInterface> interfaceMap;

    for (ifaddrs* ifa = ifaddrList; ifa != nullptr; ifa = ifa->ifa_next) {
      if (ifa->ifa_addr == nullptr)
        continue;

      const String interfaceName = String(ifa->ifa_name);

      // Get or create the interface entry
      auto& interface = interfaceMap[interfaceName];
      interface.name  = interfaceName;

      // Set flags
      interface.isUp       = ifa->ifa_flags & IFF_UP;
      interface.isLoopback = ifa->ifa_flags & IFF_LOOPBACK;

      // Get IPv4 details
      if (ifa->ifa_addr->sa_family == AF_INET) {
        Array<char, NI_MAXHOST> host = {};
        if (getnameinfo(ifa->ifa_addr, sizeof(sockaddr_in), host.data(), host.size(), nullptr, 0, NI_NUMERICHOST) == 0)
          interface.ipv4Address = String(host.data());
      } else if (ifa->ifa_addr->sa_family == AF_LINK) {
        auto* sdl = reinterpret_cast<sockaddr_dl*>(ifa->ifa_addr);

        if (sdl && sdl->sdl_alen == 6) {
          const auto*          macPtr = reinterpret_cast<const u8*>(LLADDR(sdl));
          const Span<const u8> macAddr(macPtr, sdl->sdl_alen);

          interface.macAddress = std::format(
            "{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
            macAddr[0],
            macAddr[1],
            macAddr[2],
            macAddr[3],
            macAddr[4],
            macAddr[5]
          );
        }
      }
    }

    // Convert the map to a vector
    Vec<NetworkInterface> interfaces;
    interfaces.reserve(interfaceMap.size());

    for (const auto& pair : interfaceMap)
      interfaces.push_back(pair.second);

    if (interfaces.empty())
      return Err(DracError(NotFound, "No network interfaces found"));

    return interfaces;
    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)
  }

  fn GetBatteryInfo() -> Result<Battery> {
    using matchit::match, matchit::is, matchit::_;
    using enum Battery::Status;

    // This snapshot contains information about all power sources (e.g., AC Power, Battery).
    // It's a Core Foundation object that we must release.
    CFTypeRef powerSourcesInfo = IOPSCopyPowerSourcesInfo();

    if (powerSourcesInfo == nullptr)
      return Err(DracError(NotFound, "Could not get power source information from IOKit."));

    // RAII to ensure the CF object is released.
    const UniquePointer<const void, decltype(&CFRelease)> powerSourcesInfoDeleter(powerSourcesInfo, &CFRelease);

    // The snapshot is an array of power sources.
    const auto* const powerSourcesList = static_cast<CFArrayRef>(powerSourcesInfo);
    const CFIndex     sourceCount      = CFArrayGetCount(powerSourcesList);

    for (CFIndex i = 0; i < sourceCount; ++i) {
      // Get the dictionary of properties for a single power source.
      CFDictionaryRef sourceDescription = IOPSGetPowerSourceDescription(powerSourcesInfo, CFArrayGetValueAtIndex(powerSourcesList, i));
      if (sourceDescription == nullptr)
        continue;

      // Check if this source is an internal battery.
      const auto* type = static_cast<const CFStringRef>(CFDictionaryGetValue(sourceDescription, CFSTR(kIOPSTypeKey)));

      if (type == nullptr || CFStringCompare(type, CFSTR(kIOPSInternalBatteryType), 0) != kCFCompareEqualTo)
        continue;

      u8 percentage = getNumericValue<u8>(sourceDescription, CFSTR(kIOPSCurrentCapacityKey)).value_or(0);

      CFTypeRef isChargingRef = CFDictionaryGetValue(sourceDescription, CFSTR(kIOPSIsChargingKey));
      bool      isCharging    = false; // Default to a safe value.

      if (isChargingRef != nullptr) {
        if (CFGetTypeID(isChargingRef) == CFBooleanGetTypeID()) {
          isCharging = CFBooleanGetValue(static_cast<CFBooleanRef>(isChargingRef));
        } else if (CFGetTypeID(isChargingRef) == CFNumberGetTypeID()) {
          i32 numericValue = 0;
          if (CFNumberGetValue(static_cast<CFNumberRef>(isChargingRef), kCFNumberIntType, &numericValue))
            isCharging = (numericValue != 0);
        }
      }

      Battery::Status status = match(isCharging)(
        is | (_ == true && percentage == 100) = Full,
        is | true                             = Charging,
        is | false                            = Discharging,
        is | _                                = Unknown
      );

      Option<std::chrono::seconds> timeRemaining = None;

      // Time to empty is given in minutes. A value of 0 means calculating, < 0 means unlimited/plugged in.
      if (Option<i32> timeMinutes = getNumericValue<i32>(sourceDescription, CFSTR(kIOPSTimeToEmptyKey)); timeMinutes && *timeMinutes > 0)
        timeRemaining = std::chrono::minutes(*timeMinutes);

      return Battery(status, percentage, timeRemaining);
    }

    // If the loop finishes without finding an internal battery.
    return Err(DracError(NotFound, "No internal battery found."));
  }
} // namespace draconis::core::system

  #if DRAC_ENABLE_PACKAGECOUNT
namespace draconis::services::packages {
  namespace fs = std::filesystem;

  fn GetHomebrewCount(draconis::utils::cache::CacheManager& cache) -> Result<u64> {
    return cache.getOrSet<u64>("homebrew_total", [&]() -> Result<u64> {
        Array<fs::path, 2> cellarPaths {
            "/opt/homebrew/Cellar",
            "/usr/local/Cellar",
        };

        u64 count = 0;

        for (const fs::path& cellarPath : cellarPaths) {
            if (std::error_code errc; !fs::exists(cellarPath, errc) || errc) {
                if (errc && errc != std::errc::no_such_file_or_directory)
                    return Err(DracError(errc));

                continue;
            }

            Result dirCount = GetCountFromDirectory(cache, "homebrew_" + cellarPath.filename().string(), cellarPath, true);

            if (!dirCount) {
                if (dirCount.error().code != NotFound)
                    return dirCount;

                continue;
            }

            count += *dirCount;
        }

        if (count == 0)
            return Err(DracError(NotFound, "No Homebrew packages found in any Cellar directory"));

        return count;
    });
  }

  fn GetMacPortsCount(draconis::utils::cache::CacheManager& cache) -> Result<u64> {
    return GetCountFromDb(cache, "macports", "/opt/local/var/macports/registry/registry.db", "SELECT COUNT(*) FROM ports WHERE state='installed';");
  }
} // namespace draconis::services::packages
  #endif

#endif
