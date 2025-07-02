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

  #include <Drac++/Utils/CacheManager.hpp>
  #include <Drac++/Utils/Definitions.hpp>
  #include <Drac++/Utils/Env.hpp>
  #include <Drac++/Utils/Error.hpp>
  #include <Drac++/Utils/Logging.hpp>
  #include <Drac++/Utils/Types.hpp>

  #include "OS/macOS/Bridge.hpp"

using namespace draconis::utils::types;

using enum draconis::utils::error::DracErrorCode;

namespace {
  fn getDisplayInfoById(CGDirectDisplayID displayID) -> Result<Output> {
    // Get display resolution
    const usize width  = CGDisplayPixelsWide(displayID);
    const usize height = CGDisplayPixelsHigh(displayID);

    if (width == 0 || height == 0)
      ERR_FMT(UnavailableFeature, "CGDisplayPixelsWide/High returned 0 for displayID {} (no display or API unavailable)", displayID);

    // Get the current display mode to find the refresh rate
    CGDisplayModeRef currentMode = CGDisplayCopyDisplayMode(displayID);
    if (currentMode == nullptr)
      ERR_FMT(UnavailableFeature, "CGDisplayCopyDisplayMode failed for displayID {} (no display mode available)", displayID);

    f64 refreshRate = CGDisplayModeGetRefreshRate(currentMode);

    // Release the display mode object
    CGDisplayModeRelease(currentMode);

    // Check if this is the main display
    const bool isPrimary = displayID == CGMainDisplayID();

    return Output(
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
        ERR(ResourceExhausted, "host_page_size failed to get page size (Mach API unavailable or resource exhausted)");

    u64   totalMem = 0;
    usize size     = sizeof(totalMem);

    // "hw.memsize" is the standard key for getting the total memory size from the system.
    if (sysctlbyname("hw.memsize", &totalMem, &size, nullptr, 0) == -1)
      ERR_FMT(ResourceExhausted, "sysctlbyname('hw.memsize') failed: {}", std::system_category().message(errno));

    vm_statistics64_data_t vmStats;
    mach_msg_type_number_t infoCount = sizeof(vmStats) / sizeof(natural_t);

    // Retrieve detailed virtual memory statistics from the Mach kernel.
    // The `reinterpret_cast` is required here to interface with the C-style
    // Mach API, which expects a generic `host_info64_t` pointer.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    if (host_statistics64(HostPort, HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&vmStats), &infoCount) != KERN_SUCCESS)
      ERR(ResourceExhausted, "host_statistics64 failed to get memory statistics (Mach API unavailable or resource exhausted)");

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
    return cache.getOrSet<String>("macos_os_version", macOS::GetOSVersion);
  }

  fn GetDesktopEnvironment(draconis::utils::cache::CacheManager& cache) -> Result<String> {
    // macOS doesn't really have the concept of a desktop environment,
    // and an immediate return doesn't need caching.
    (void)cache;

    return "Aqua";
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
        ERR(ResourceExhausted, "sysctl size query failed for KERN_PROC_ALL (process list unavailable or resource exhausted)");

      if (len == 0)
        ERR(NotFound, "sysctl for KERN_PROC_ALL returned zero length (no processes found, feature not present)");

      Vec<char> buf(len);

      if (sysctl(request.data(), request.size(), buf.data(), &len, nullptr, 0) == -1)
        ERR(ResourceExhausted, "sysctl data fetch failed for KERN_PROC_ALL (process list unavailable or resource exhausted)");

      if (len % sizeof(kinfo_proc) != 0)
        ERR_FMT(CorruptedData, "sysctl returned size {} which is not a multiple of kinfo_proc size {} (corrupt process list)", len, sizeof(kinfo_proc));

      usize count = len / sizeof(kinfo_proc);

      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
      Span<const kinfo_proc> processes(reinterpret_cast<const kinfo_proc*>(buf.data()), count);

      for (const kinfo_proc& procInfo : processes)
        for (const StringView& wmName : knownWms)
          if (
            std::ranges::equal(
              StringView(procInfo.kp_proc.p_comm),
              wmName,
              [](char chrA, char chrB) {
                return std::tolower(chrA) == std::tolower(chrB);
              }
            )
          )
            return String(wmName);

      return "Quartz";
    });
  }

  fn GetKernelVersion(draconis::utils::cache::CacheManager& cache) -> Result<String> {
    // clang-format off
    return cache.getOrSet<String>("macos_kernel", []() -> Result<String> {
      Array<char, 256> kernelVersion {};
      usize            kernelVersionLen = kernelVersion.size();

      if (sysctlbyname("kern.osrelease", kernelVersion.data(), &kernelVersionLen, nullptr, 0) == -1)
        ERR_FMT(ResourceExhausted, "sysctlbyname('kern.osrelease') failed: {}", std::system_category().message(errno));

      return String(kernelVersion.data());
    }, draconis::utils::cache::CachePolicy::neverExpire());
    // clang-format on
  }

  fn GetHost(draconis::utils::cache::CacheManager& cache) -> Result<String> {
    return cache.getOrSet<String>("macos_host", []() -> Result<String> {
      Array<char, 256> hwModel {};
      usize            hwModelLen = hwModel.size();

      if (sysctlbyname("hw.model", hwModel.data(), &hwModelLen, nullptr, 0) == -1)
        ERR_FMT(ResourceExhausted, "sysctlbyname('hw.model') failed: {}", std::system_category().message(errno));

      // taken from https://github.com/fastfetch-cli/fastfetch/blob/dev/src/detection/host/host_mac.c
      // shortened a lot of the entries to remove unnecessary info
      static const std::flat_map<StringView, StringView> MODEL_NAME_BY_HW_MODEL = {
        // MacBook Pro
        { "MacBookPro18,3",      "MacBook Pro (14-inch, 2021)" },
        { "MacBookPro18,4",      "MacBook Pro (14-inch, 2021)" },
        { "MacBookPro18,1",      "MacBook Pro (16-inch, 2021)" },
        { "MacBookPro18,2",      "MacBook Pro (16-inch, 2021)" },
        { "MacBookPro17,1",  "MacBook Pro (13-inch, M1, 2020)" },
        { "MacBookPro16,3",      "MacBook Pro (13-inch, 2020)" },
        { "MacBookPro16,2",      "MacBook Pro (13-inch, 2020)" },
        { "MacBookPro16,4",      "MacBook Pro (16-inch, 2019)" },
        { "MacBookPro16,1",      "MacBook Pro (16-inch, 2019)" },
        { "MacBookPro15,4",      "MacBook Pro (13-inch, 2019)" },
        { "MacBookPro15,3",      "MacBook Pro (15-inch, 2019)" },
        { "MacBookPro15,2", "MacBook Pro (13-inch, 2018/2019)" },
        { "MacBookPro15,1", "MacBook Pro (15-inch, 2018/2019)" },
        { "MacBookPro14,3",      "MacBook Pro (15-inch, 2017)" },
        { "MacBookPro14,2",      "MacBook Pro (13-inch, 2017)" },
        { "MacBookPro14,1",      "MacBook Pro (13-inch, 2017)" },
        { "MacBookPro13,3",      "MacBook Pro (15-inch, 2016)" },
        { "MacBookPro13,2",      "MacBook Pro (13-inch, 2016)" },
        { "MacBookPro13,1",      "MacBook Pro (13-inch, 2016)" },
        { "MacBookPro12,1",      "MacBook Pro (13-inch, 2015)" },
        { "MacBookPro11,4",      "MacBook Pro (15-inch, 2015)" },
        { "MacBookPro11,5",      "MacBook Pro (15-inch, 2015)" },
        { "MacBookPro11,2", "MacBook Pro (15-inch, 2013/2014)" },
        { "MacBookPro11,3", "MacBook Pro (15-inch, 2013/2014)" },
        { "MacBookPro11,1", "MacBook Pro (13-inch, 2013/2014)" },
        { "MacBookPro10,2", "MacBook Pro (13-inch, 2012/2013)" },
        { "MacBookPro10,1", "MacBook Pro (15-inch, 2012/2013)" },
        {  "MacBookPro9,2",      "MacBook Pro (13-inch, 2012)" },
        {  "MacBookPro9,1",      "MacBook Pro (15-inch, 2012)" },
        {  "MacBookPro8,3",      "MacBook Pro (17-inch, 2011)" },
        {  "MacBookPro8,2",      "MacBook Pro (15-inch, 2011)" },
        {  "MacBookPro8,1",      "MacBook Pro (13-inch, 2011)" },
        {  "MacBookPro7,1",      "MacBook Pro (13-inch, 2010)" },
        {  "MacBookPro6,2",      "MacBook Pro (15-inch, 2010)" },
        {  "MacBookPro6,1",      "MacBook Pro (17-inch, 2010)" },
        {  "MacBookPro5,5",      "MacBook Pro (13-inch, 2009)" },
        {  "MacBookPro5,3",      "MacBook Pro (15-inch, 2009)" },
        {  "MacBookPro5,2",      "MacBook Pro (17-inch, 2009)" },
        {  "MacBookPro5,1",      "MacBook Pro (15-inch, 2008)" },
        {  "MacBookPro4,1",   "MacBook Pro (17/15-inch, 2008)" },

        // MacBook Air
        { "MacBookAir10,1",           "MacBook Air (M1, 2020)" },
        {  "MacBookAir9,1",      "MacBook Air (13-inch, 2020)" },
        {  "MacBookAir8,2",      "MacBook Air (13-inch, 2019)" },
        {  "MacBookAir8,1",      "MacBook Air (13-inch, 2018)" },
        {  "MacBookAir7,2", "MacBook Air (13-inch, 2015/2017)" },
        {  "MacBookAir7,1",      "MacBook Air (11-inch, 2015)" },
        {  "MacBookAir6,2", "MacBook Air (13-inch, 2013/2014)" },
        {  "MacBookAir6,1", "MacBook Air (11-inch, 2013/2014)" },
        {  "MacBookAir5,2",      "MacBook Air (13-inch, 2012)" },
        {  "MacBookAir5,1",      "MacBook Air (11-inch, 2012)" },
        {  "MacBookAir4,2",      "MacBook Air (13-inch, 2011)" },
        {  "MacBookAir4,1",      "MacBook Air (11-inch, 2011)" },
        {  "MacBookAir3,2",      "MacBook Air (13-inch, 2010)" },
        {  "MacBookAir3,1",      "MacBook Air (11-inch, 2010)" },
        {  "MacBookAir2,1",               "MacBook Air (2009)" },

        // Mac mini
        {     "Macmini9,1",              "Mac mini (M1, 2020)" },
        {     "Macmini8,1",                  "Mac mini (2018)" },
        {     "Macmini7,1",                  "Mac mini (2014)" },
        {     "Macmini6,1",                  "Mac mini (2012)" },
        {     "Macmini6,2",                  "Mac mini (2012)" },
        {     "Macmini5,1",                  "Mac mini (2011)" },
        {     "Macmini5,2",                  "Mac mini (2011)" },
        {     "Macmini4,1",                  "Mac mini (2010)" },
        {     "Macmini3,1",                  "Mac mini (2009)" },

        // MacBook
        {    "MacBook10,1",          "MacBook (12-inch, 2017)" },
        {     "MacBook9,1",          "MacBook (12-inch, 2016)" },
        {     "MacBook8,1",          "MacBook (12-inch, 2015)" },
        {     "MacBook7,1",          "MacBook (13-inch, 2010)" },
        {     "MacBook6,1",          "MacBook (13-inch, 2009)" },
        {     "MacBook5,2",          "MacBook (13-inch, 2009)" },

        // Mac Pro
        {      "MacPro7,1",                   "Mac Pro (2019)" },
        {      "MacPro6,1",                   "Mac Pro (2013)" },
        {      "MacPro5,1",            "Mac Pro (2010 - 2012)" },
        {      "MacPro4,1",                   "Mac Pro (2009)" },

        // Mac (Generic)
        {        "Mac16,3",             "iMac (24-inch, 2024)" },
        {        "Mac16,2",             "iMac (24-inch, 2024)" },
        {        "Mac16,1",      "MacBook Pro (14-inch, 2024)" },
        {        "Mac16,6",      "MacBook Pro (14-inch, 2024)" },
        {        "Mac16,8",      "MacBook Pro (14-inch, 2024)" },
        {        "Mac16,7",      "MacBook Pro (16-inch, 2024)" },
        {        "Mac16,5",      "MacBook Pro (16-inch, 2024)" },
        {       "Mac16,15",                  "Mac mini (2024)" },
        {       "Mac16,10",                  "Mac mini (2024)" },
        {       "Mac15,13",  "MacBook Air (15-inch, M3, 2024)" },
        {        "Mac15,2",  "MacBook Air (13-inch, M3, 2024)" },
        {        "Mac15,3",  "MacBook Pro (14-inch, Nov 2023)" },
        {        "Mac15,4",             "iMac (24-inch, 2023)" },
        {        "Mac15,5",             "iMac (24-inch, 2023)" },
        {        "Mac15,6",  "MacBook Pro (14-inch, Nov 2023)" },
        {        "Mac15,8",  "MacBook Pro (14-inch, Nov 2023)" },
        {       "Mac15,10",  "MacBook Pro (14-inch, Nov 2023)" },
        {        "Mac15,7",  "MacBook Pro (16-inch, Nov 2023)" },
        {        "Mac15,9",  "MacBook Pro (16-inch, Nov 2023)" },
        {       "Mac15,11",  "MacBook Pro (16-inch, Nov 2023)" },
        {       "Mac14,15",  "MacBook Air (15-inch, M2, 2023)" },
        {       "Mac14,14",      "Mac Studio (M2 Ultra, 2023)" },
        {       "Mac14,13",        "Mac Studio (M2 Max, 2023)" },
        {        "Mac14,8",                   "Mac Pro (2023)" },
        {        "Mac14,6",      "MacBook Pro (16-inch, 2023)" },
        {       "Mac14,10",      "MacBook Pro (16-inch, 2023)" },
        {        "Mac14,5",      "MacBook Pro (14-inch, 2023)" },
        {        "Mac14,9",      "MacBook Pro (14-inch, 2023)" },
        {        "Mac14,3",              "Mac mini (M2, 2023)" },
        {       "Mac14,12",              "Mac mini (M2, 2023)" },
        {        "Mac14,7",  "MacBook Pro (13-inch, M2, 2022)" },
        {        "Mac14,2",           "MacBook Air (M2, 2022)" },
        {        "Mac13,1",        "Mac Studio (M1 Max, 2022)" },
        {        "Mac13,2",      "Mac Studio (M1 Ultra, 2022)" },

        // iMac
        {       "iMac21,1",         "iMac (24-inch, M1, 2021)" },
        {       "iMac21,2",         "iMac (24-inch, M1, 2021)" },
        {       "iMac20,1",             "iMac (27-inch, 2020)" },
        {       "iMac20,2",             "iMac (27-inch, 2020)" },
        {       "iMac19,1",             "iMac (27-inch, 2019)" },
        {       "iMac19,2",           "iMac (21.5-inch, 2019)" },
        {     "iMacPro1,1",                  "iMac Pro (2017)" },
        {       "iMac18,3",             "iMac (27-inch, 2017)" },
        {       "iMac18,2",           "iMac (21.5-inch, 2017)" },
        {       "iMac18,1",           "iMac (21.5-inch, 2017)" },
        {       "iMac17,1",             "iMac (27-inch, 2015)" },
        {       "iMac16,2",           "iMac (21.5-inch, 2015)" },
        {       "iMac16,1",           "iMac (21.5-inch, 2015)" },
        {       "iMac15,1",        "iMac (27-inch, 2014/2015)" },
        {       "iMac14,4",           "iMac (21.5-inch, 2014)" },
        {       "iMac14,2",             "iMac (27-inch, 2013)" },
        {       "iMac14,1",           "iMac (21.5-inch, 2013)" },
        {       "iMac13,2",             "iMac (27-inch, 2012)" },
        {       "iMac13,1",           "iMac (21.5-inch, 2012)" },
        {       "iMac12,2",             "iMac (27-inch, 2011)" },
        {       "iMac12,1",           "iMac (21.5-inch, 2011)" },
        {       "iMac11,3",             "iMac (27-inch, 2010)" },
        {       "iMac11,2",           "iMac (21.5-inch, 2010)" },
        {       "iMac10,1",        "iMac (27/21.5-inch, 2009)" },
        {        "iMac9,1",          "iMac (24/20-inch, 2009)" },
      };

      const auto iter = MODEL_NAME_BY_HW_MODEL.find(hwModel.data());

      if (iter == MODEL_NAME_BY_HW_MODEL.end())
        ERR_FMT(UnavailableFeature, "Unknown hardware model: {} (feature not present)", hwModel.data());

      return String(iter->second);
    });
  }

  fn GetCPUModel(draconis::utils::cache::CacheManager& cache) -> Result<String> {
    // clang-format off
    return cache.getOrSet<String>("macos_cpu_model", []() -> Result<String> {
      Array<char, 256> cpuModel {};
      usize            cpuModelLen = cpuModel.size();

      if (sysctlbyname("machdep.cpu.brand_string", cpuModel.data(), &cpuModelLen, nullptr, 0) == -1)
        ERR_FMT(ResourceExhausted, "sysctlbyname('machdep.cpu.brand_string') failed: {}", std::system_category().message(errno));

      return String(cpuModel.data());
    }, draconis::utils::cache::CachePolicy::neverExpire());
    // clang-format on
  }

  fn GetCPUCores(draconis::utils::cache::CacheManager& cache) -> Result<CPUCores> {
    // clang-format off
    return cache.getOrSet<CPUCores>("macos_cpu_cores", []() -> Result<CPUCores> {
      u32   physicalCores = 0;
      u32   logicalCores  = 0;
      usize size          = sizeof(u32);

      if (sysctlbyname("hw.physicalcpu", &physicalCores, &size, nullptr, 0) == -1)
        ERR_FMT(ResourceExhausted, "sysctlbyname('hw.physicalcpu') failed: {}", std::system_category().message(errno));

      size = sizeof(u32);

      if (sysctlbyname("hw.logicalcpu", &logicalCores, &size, nullptr, 0) == -1)
        ERR_FMT(ResourceExhausted, "sysctlbyname('hw.logicalcpu') failed: {}", std::system_category().message(errno));

      debug_log("Physical cores: {}", physicalCores);
      debug_log("Logical cores: {}", logicalCores);

      return CPUCores(physicalCores, logicalCores);
    }, draconis::utils::cache::CachePolicy::neverExpire());
    // clang-format on
  }

  fn GetGPUModel(draconis::utils::cache::CacheManager& cache) -> Result<String> {
    // clang-format off
    return cache.getOrSet<String>("macos_gpu", []() -> Result<String> {
      const Result<String> gpuModel = macOS::GetGPUModel();

      if (!gpuModel)
        ERR(UnavailableFeature, "macOS::GetGPUModel() failed: GPU model unavailable (no GPU present)");

      return *gpuModel;
    }, draconis::utils::cache::CachePolicy::neverExpire());
    // clang-format on
  }

  fn GetDiskUsage() -> Result<ResourceUsage> {
    struct statvfs vfs;

    if (statvfs("/", &vfs) != 0)
      ERR_FMT(ResourceExhausted, "statvfs('/') failed: {}", std::system_category().message(errno));

    return ResourceUsage {
      .usedBytes  = (vfs.f_blocks - vfs.f_bfree) * vfs.f_frsize,
      .totalBytes = vfs.f_blocks * vfs.f_frsize,
    };
  }

  fn GetShell(draconis::utils::cache::CacheManager& cache) -> Result<String> {
    return cache.getOrSet<String>("macos_shell", []() -> Result<String> {
      if (const Result<String> shellPath = draconis::utils::env::GetEnv("SHELL")) {
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

      ERR(ConfigurationError, "Could not find SHELL environment variable (SHELL not set in environment)");
    });
  }

  fn GetUptime() -> Result<std::chrono::seconds> {
    using namespace std::chrono;

    Array<i32, 2> mib = { CTL_KERN, KERN_BOOTTIME };

    struct timeval boottime;
    usize          len = sizeof(boottime);

    if (sysctl(mib.data(), mib.size(), &boottime, &len, nullptr, 0) == -1)
      ERR(ResourceExhausted, "sysctl(CTL_KERN, KERN_BOOTTIME) failed: system boot time unavailable or resource exhausted");

    const system_clock::time_point bootTimepoint = system_clock::from_time_t(boottime.tv_sec);

    const system_clock::time_point now = system_clock::now();

    return duration_cast<seconds>(now - bootTimepoint);
  }

  fn GetPrimaryOutput() -> Result<Output> {
    return getDisplayInfoById(CGMainDisplayID());
  }

  fn GetOutputs() -> Result<Vec<Output>> {
    u32 displayCount = 0;

    if (CGGetOnlineDisplayList(0, nullptr, &displayCount) != kCGErrorSuccess)
      ERR(UnavailableFeature, "CGGetOnlineDisplayList failed to get display count (CoreGraphics API unavailable or no displays)");

    if (displayCount == 0)
      ERR(UnavailableFeature, "No displays found (displayCount == 0, feature not present)");

    Vec<CGDirectDisplayID> displayIDs(displayCount);

    if (CGGetOnlineDisplayList(displayCount, displayIDs.data(), &displayCount) != kCGErrorSuccess)
      ERR(UnavailableFeature, "CGGetOnlineDisplayList failed to get display list (CoreGraphics API unavailable or no displays)");

    Vec<Output> displays;
    displays.reserve(displayCount);

    for (const CGDirectDisplayID displayID : displayIDs)
      if (Result<Output> display = getDisplayInfoById(displayID); display)
        displays.push_back(*display);

    return displays;
  }

  fn GetPrimaryNetworkInterface(draconis::utils::cache::CacheManager& cache) -> Result<NetworkInterface> {
    return cache.getOrSet<NetworkInterface>("macos_primary_network_interface", []() -> Result<NetworkInterface> {
      // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast) - This requires a lot of casts and there's no good way to avoid them.
      Array<i32, 6> mib = { CTL_NET, PF_ROUTE, 0, AF_INET, NET_RT_FLAGS, RTF_GATEWAY };
      usize         len = 0;

      if (sysctl(mib.data(), mib.size(), nullptr, &len, nullptr, 0) == -1)
        ERR(ResourceExhausted, "sysctl(CTL_NET, PF_ROUTE, ...) failed to get routing table size (network API unavailable or resource exhausted)");

      Vec<char> buf(len);

      if (sysctl(mib.data(), mib.size(), buf.data(), &len, nullptr, 0) == -1)
        ERR(ResourceExhausted, "sysctl(CTL_NET, PF_ROUTE, ...) failed to get routing table dump (network API unavailable or resource exhausted)");

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
        ERR(UnavailableFeature, "Could not determine primary interface name from routing table (no default route found, feature not present)");

      ifaddrs* ifaddrList = nullptr;
      if (getifaddrs(&ifaddrList) == -1)
        ERR_FMT(ResourceExhausted, "getifaddrs() failed: {} (resource exhausted or API unavailable)", std::system_category().message(errno));

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
        ERR_FMT(UnavailableFeature, "Found primary interface name '{}' but could not find its details via getifaddrs (feature not present)", primaryInterfaceName);

      return primaryInterface;
      // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)
    });
  }

  fn GetNetworkInterfaces() -> Result<Vec<NetworkInterface>> {
    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast) - This requires a lot of casts and there's no good way to avoid them.
    ifaddrs* ifaddrList = nullptr;
    if (getifaddrs(&ifaddrList) == -1)
      ERR_FMT(ResourceExhausted, "getifaddrs() failed: {} (resource exhausted or API unavailable)", std::system_category().message(errno));

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
      ERR(UnavailableFeature, "No network interfaces found (getifaddrs returned empty list, feature not present)");

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
      ERR(UnavailableFeature, "IOPSCopyPowerSourcesInfo() returned nullptr (IOKit unavailable or no power sources/feature not present)");

    // RAII to ensure the CF object is released.
    const UniquePointer<const Unit, decltype(&CFRelease)> powerSourcesInfoDeleter(powerSourcesInfo, &CFRelease);

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
    ERR(UnavailableFeature, "No internal battery found (no IOPSInternalBatteryType in power sources, feature not present)");
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
            ERR_FMT(ResourceExhausted, "fs::exists('{}') failed: {} (resource exhausted or API unavailable)", cellarPath.string(), errc.message());

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
        ERR(NotFound, "No Homebrew packages found in any Cellar directory");

      return count;
    });
  }

  fn GetMacPortsCount(draconis::utils::cache::CacheManager& cache) -> Result<u64> {
    return GetCountFromDb(cache, "macports", "/opt/local/var/macports/registry/registry.db", "SELECT COUNT(*) FROM ports WHERE state='installed';");
  }
} // namespace draconis::services::packages
  #endif

#endif
