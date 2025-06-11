#ifdef __APPLE__

// clang-format off
#include <CoreFoundation/CFPropertyList.h> // CFPropertyListCreateWithData, kCFPropertyListImmutable
#include <CoreFoundation/CFStream.h>       // CFReadStreamClose, CFReadStreamCreateWithFile, CFReadStreamOpen, CFReadStreamRead, CFReadStreamRef
#include <IOKit/IOKitLib.h>                // IOKit types and functions
#include <flat_map>                        // std::flat_map
#include <mach/mach_host.h>                // host_statistics64
#include <mach/mach_init.h>                // host_page_size, mach_host_self
#include <mach/vm_statistics.h>            // vm_statistics64_data_t
#include <sys/statvfs.h>                   // statvfs
#include <sys/sysctl.h>                    // {CTL_KERN, KERN_PROC, KERN_PROC_ALL, kinfo_proc, sysctl, sysctlbyname}

#include "Core/System.hpp"

#include "Services/PackageCounting.hpp"

#include "Util/Caching.hpp"
#include "Util/Definitions.hpp"
#include "Util/Env.hpp"
#include "Util/Error.hpp"
#include "Util/Types.hpp"

#include "macOS/Bridge.hpp"
// clang-format on

using namespace util::types;
using util::cache::GetValidCache, util::cache::WriteCache;
using util::error::DracError, util::error::DracErrorCode;
using util::helpers::GetEnv;

namespace {
  fn StrEqualsIgnoreCase(StringView strA, StringView strB) -> bool {
    return std::ranges::equal(strA, strB, [](char aChar, char bChar) {
      return std::tolower(static_cast<u8>(aChar)) == std::tolower(static_cast<u8>(bChar));
    });
  }

  fn Capitalize(StringView sview) -> Option<String> {
    if (sview.empty())
      return None;

    String result(sview);
    result.front() = static_cast<char>(std::toupper(static_cast<u8>(result.front())));

    return result;
  }
} // namespace

namespace os {
  fn System::getMemInfo() -> Result<ResourceUsage> {
    static mach_port_t HostPort = mach_host_self();
    static vm_size_t   PageSize = 0;

    if (PageSize == 0) {
      if (host_page_size(HostPort, &PageSize) != KERN_SUCCESS)
        return Err(DracError("Failed to get page size"));
    }

    u64   totalMem = 0;
    usize size     = sizeof(totalMem);

    if (sysctlbyname("hw.memsize", &totalMem, &size, nullptr, 0) == -1)
      return Err(DracError("Failed to get total memory info"));

    vm_statistics64_data_t vmStats;
    mach_msg_type_number_t infoCount = sizeof(vmStats) / sizeof(natural_t);

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    if (host_statistics64(HostPort, HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&vmStats), &infoCount) != KERN_SUCCESS)
      return Err(DracError("Failed to get memory statistics"));

    u64 usedMem = (vmStats.active_count + vmStats.wire_count) * PageSize;

    return ResourceUsage {
      .usedBytes  = usedMem,
      .totalBytes = totalMem
    };
  }

  fn System::getNowPlaying() -> Result<MediaInfo> {
    return GetCurrentPlayingInfo();
  }

  fn System::getOSVersion() -> Result<String> {
    // clang-format off
    static constexpr Array<Pair<u8, StringView>, 6> VERSION_NAMES = {{
      { 11, "Big Sur"  },
      { 12, "Monterey" },
      { 13, "Ventura"  },
      { 14, "Sonoma"   },
      { 15, "Sequoia"  },
      { 26, "Tahoe"    },
    }};
    // clang-format on

    CFURLRef url = CFURLCreateWithFileSystemPath(
      kCFAllocatorDefault,
      CFSTR("/System/Library/CoreServices/SystemVersion.plist"),
      kCFURLPOSIXPathStyle,
      false
    );

    if (!url)
      return Err(DracError(DracErrorCode::PlatformSpecific, "Failed to create CFURL for SystemVersion.plist"));

    CFReadStreamRef stream = CFReadStreamCreateWithFile(kCFAllocatorDefault, url);
    CFRelease(url);

    if (!stream)
      return Err(DracError(DracErrorCode::PlatformSpecific, "Failed to create read stream for SystemVersion.plist"));

    if (!CFReadStreamOpen(stream)) {
      CFRelease(stream);
      return Err(DracError(DracErrorCode::PlatformSpecific, "Failed to open SystemVersion.plist"));
    }

    static constexpr usize    BUFFER_SIZE = 4096;
    Array<UInt8, BUFFER_SIZE> buffer;
    CFMutableDataRef          data      = CFDataCreateMutable(kCFAllocatorDefault, 0);
    CFIndex                   bytesRead = 0;

    while ((bytesRead = CFReadStreamRead(stream, buffer.data(), buffer.size())) > 0)
      CFDataAppendBytes(data, buffer.data(), bytesRead);

    CFReadStreamClose(stream);
    CFRelease(stream);

    if (CFDataGetLength(data) == 0) {
      CFRelease(data);
      return Err(DracError(DracErrorCode::PlatformSpecific, "SystemVersion.plist is empty"));
    }

    CFPropertyListRef plist = CFPropertyListCreateWithData(
      kCFAllocatorDefault, data, kCFPropertyListImmutable, nullptr, nullptr
    );

    CFRelease(data);

    if (!plist || CFGetTypeID(plist) != CFDictionaryGetTypeID()) {
      if (plist)
        CFRelease(plist);

      return Err(DracError(DracErrorCode::PlatformSpecific, "Failed to parse SystemVersion.plist"));
    }

    const auto* dict          = static_cast<CFDictionaryRef>(plist);
    const auto* versionString = static_cast<CFStringRef>(CFDictionaryGetValue(dict, CFSTR("iOSSupportVersion")));

    if (!versionString || CFGetTypeID(versionString) != CFStringGetTypeID()) {
      CFRelease(plist);
      return Err(DracError(DracErrorCode::PlatformSpecific, "Failed to get version string from SystemVersion.plist"));
    }

    static constexpr usize VERSION_BUFFER_SIZE = 256;

    Array<char, VERSION_BUFFER_SIZE> versionBuffer;

    if (!CFStringGetCString(versionString, versionBuffer.data(), versionBuffer.size(), kCFStringEncodingUTF8)) {
      CFRelease(plist);
      return Err(DracError(DracErrorCode::PlatformSpecific, "Failed to convert version string to C string"));
    }

    String versionNumber(versionBuffer.data());
    CFRelease(plist);

    if (versionNumber.empty())
      return Err(DracError(DracErrorCode::PlatformSpecific, "Version string is empty"));

    const usize dotPos = versionNumber.find('.');
    if (dotPos == String::npos)
      return Err(DracError(DracErrorCode::PlatformSpecific, "Invalid version number format"));

    const u8   majorVersion = static_cast<u8>(std::stoi(versionNumber.substr(0, dotPos)));
    StringView versionName  = "Unknown";

    for (const auto& [version, name] : VERSION_NAMES)
      if (version == majorVersion) {
        versionName = name;
        break;
      }

    return std::format("macOS {} {}", versionNumber, versionName);
  }

  fn System::getDesktopEnvironment() -> Result<String> {
    return "Aqua";
  }

  fn System::getWindowManager() -> Result<String> {
    constexpr Array<StringView, 6> knownWms = {
      "yabai",
      "kwm",
      "chunkwm",
      "amethyst",
      "spectacle",
      "rectangle",
    };

    Array<i32, 3> request = { CTL_KERN, KERN_PROC, KERN_PROC_ALL };

    usize len = 0;

    if (sysctl(request.data(), request.size(), nullptr, &len, nullptr, 0) == -1)
      return Err(DracError("sysctl size query failed for KERN_PROC_ALL"));

    if (len == 0)
      return Err(DracError(DracErrorCode::NotFound, "sysctl for KERN_PROC_ALL returned zero length"));

    Vec<char> buf(len);

    if (sysctl(request.data(), request.size(), buf.data(), &len, nullptr, 0) == -1)
      return Err(DracError("sysctl data fetch failed for KERN_PROC_ALL"));

    if (len % sizeof(kinfo_proc) != 0)
      return Err(DracError(
        DracErrorCode::PlatformSpecific,
        std::format("sysctl returned size {} which is not a multiple of kinfo_proc size {}", len, sizeof(kinfo_proc))
      ));

    usize count = len / sizeof(kinfo_proc);

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    std::span<const kinfo_proc> processes = std::span(reinterpret_cast<const kinfo_proc*>(buf.data()), count);

    for (const kinfo_proc& procInfo : processes) {
      StringView comm(procInfo.kp_proc.p_comm);

      for (const StringView& wmName : knownWms)
        if (StrEqualsIgnoreCase(comm, wmName)) {
          if (const Option<String> capitalized = Capitalize(comm))
            return *capitalized;

          return Err(DracError(DracErrorCode::ParseError, "Failed to capitalize window manager name"));
        }
    }

    return "Quartz";
  }

  fn System::getKernelVersion() -> Result<String> {
    const String cacheKey = "macos_kernel";

    if (Result<String> cachedKernel = GetValidCache<String>(cacheKey))
      return *cachedKernel;

    Array<char, 256> kernelVersion {};
    usize            kernelVersionLen = sizeof(kernelVersion);

    if (sysctlbyname("kern.osrelease", kernelVersion.data(), &kernelVersionLen, nullptr, 0) == -1)
      return Err(DracError("Failed to get kernel version"));

    String version(kernelVersion.data());
    if (Result writeResult = WriteCache(cacheKey, version); !writeResult)
      debug_at(writeResult.error());

    return version;
  }

  fn System::getHost() -> Result<String> {
    const String cacheKey = "macos_host";

    if (Result<String> cachedHost = GetValidCache<String>(cacheKey))
      return *cachedHost;

    Array<char, 256> hwModel {};
    usize            hwModelLen = sizeof(hwModel);

    if (sysctlbyname("hw.model", hwModel.data(), &hwModelLen, nullptr, 0) == -1)
      return Err(DracError("Failed to get host info"));

    // taken from https://github.com/fastfetch-cli/fastfetch/blob/dev/src/detection/host/host_mac.c
    // shortened a lot of the entries to remove unnecessary info
    std::flat_map<StringView, StringView> modelNameByHwModel = {
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

    const auto iter = modelNameByHwModel.find(hwModel.data());
    if (iter == modelNameByHwModel.end())
      return Err(DracError("Failed to get host info"));

    String host(iter->second);
    if (Result writeResult = WriteCache(cacheKey, host); !writeResult)
      debug_at(writeResult.error());

    return host;
  }

  fn System::getCPUModel() -> Result<String> {
    Array<char, 256> cpuModel {};
    usize            cpuModelLen = sizeof(cpuModel);

    if (sysctlbyname("machdep.cpu.brand_string", cpuModel.data(), &cpuModelLen, nullptr, 0) == -1)
      return Err(DracError("Failed to get CPU model"));

    return String(cpuModel.data());
  }

  fn System::getGPUModel() -> Result<String> {
    io_iterator_t          iterator = 0;
    CFMutableDictionaryRef matches  = IOServiceMatching("IOAccelerator");
    CFDictionaryAddValue(matches, CFSTR("IOMatchCategory"), CFSTR("IOAccelerator"));

    if (IOServiceGetMatchingServices(MACH_PORT_NULL, matches, &iterator) != kIOReturnSuccess)
      return Err(DracError(DracErrorCode::PlatformSpecific, "Failed to get GPU services"));

    String              gpuModel;
    io_registry_entry_t device = 0;

    while ((device = IOIteratorNext(iterator)) != 0) {
      CFMutableDictionaryRef properties = nullptr;
      if (IORegistryEntryCreateCFProperties(device, &properties, kCFAllocatorDefault, 0) == kIOReturnSuccess) {
        const auto* modelRef = static_cast<CFStringRef>(CFDictionaryGetValue(properties, CFSTR("model")));

        if (modelRef) {
          static constexpr usize         MODEL_BUFFER_SIZE = 256;
          Array<char, MODEL_BUFFER_SIZE> modelBuffer;

          if (CFStringGetCString(modelRef, modelBuffer.data(), modelBuffer.size(), kCFStringEncodingUTF8)) {
            gpuModel = String(modelBuffer.data());
            CFRelease(properties);
            IOObjectRelease(device);
            break;
          }
        }

        io_registry_entry_t parentEntry = 0;
        if (IORegistryEntryGetParentEntry(device, kIOServicePlane, &parentEntry) == kIOReturnSuccess) {
          CFMutableDictionaryRef parentProperties = nullptr;
          if (IORegistryEntryCreateCFProperties(parentEntry, &parentProperties, kCFAllocatorDefault, 0) == kIOReturnSuccess) {
            const auto* parentModelRef = static_cast<CFStringRef>(CFDictionaryGetValue(parentProperties, CFSTR("model")));
            if (parentModelRef) {
              static constexpr usize         MODEL_BUFFER_SIZE = 256;
              Array<char, MODEL_BUFFER_SIZE> modelBuffer;

              if (CFStringGetCString(parentModelRef, modelBuffer.data(), modelBuffer.size(), kCFStringEncodingUTF8))
                gpuModel = String(modelBuffer.data());
            }
            CFRelease(parentProperties);
          }
          IOObjectRelease(parentEntry);
        }

        CFRelease(properties);
      }
      IOObjectRelease(device);
    }

    IOObjectRelease(iterator);

    if (gpuModel.empty())
      return Err(DracError(DracErrorCode::PlatformSpecific, "Failed to get GPU model"));

    return gpuModel;
  }

  fn System::getDiskUsage() -> Result<ResourceUsage> {
    struct statvfs vfs;

    if (statvfs("/", &vfs) != 0)
      return Err(DracError("Failed to get disk usage"));

    return ResourceUsage {
      .usedBytes  = (vfs.f_blocks - vfs.f_bfree) * vfs.f_frsize,
      .totalBytes = vfs.f_blocks * vfs.f_frsize,
    };
  }

  fn System::getShell() -> Result<String> {
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

    return Err(DracError(DracErrorCode::NotFound, "Could not find SHELL environment variable"));
  }
} // namespace os

  #if DRAC_ENABLE_PACKAGECOUNT
namespace package {
  namespace fs = std::filesystem;

  fn GetHomebrewCount() -> Result<u64> {
    const String cacheKey = "homebrew_total";

    if (Result<u64> cachedCountResult = GetValidCache<u64>(cacheKey))
      return *cachedCountResult;
    else
      debug_at(cachedCountResult.error());

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

      const String cacheKey = "homebrew_" + cellarPath.filename().string();
      Result       dirCount = GetCountFromDirectory(cacheKey, cellarPath, true);

      if (!dirCount) {
        if (dirCount.error().code != DracErrorCode::NotFound)
          return dirCount;

        continue;
      }

      count += *dirCount;
    }

    if (count == 0)
      return Err(DracError(DracErrorCode::NotFound, "No Homebrew packages found in any Cellar directory"));

    if (Result writeResult = WriteCache(cacheKey, count); !writeResult)
      debug_at(writeResult.error());

    return count;
  }

  fn GetMacPortsCount() -> Result<u64> {
    return GetCountFromDb("macports", "/opt/local/var/macports/registry/registry.db", "SELECT COUNT(*) FROM ports WHERE state='installed';");
  }
} // namespace package
  #endif

#endif
