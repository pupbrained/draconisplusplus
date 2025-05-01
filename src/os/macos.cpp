#ifdef __APPLE__

// clang-format off
#include <flat_map>
#include <sys/statvfs.h>
#include <sys/sysctl.h>

#include "macos/bridge.hpp"
#include "os.hpp"
#include "src/util/defs.hpp"
#include "src/util/error.hpp"
#include "src/util/types.hpp"
// clang-format on

using namespace util::types;
using util::error::DracError;

fn os::GetMemInfo() -> Result<u64, DracError> {
  u64   mem  = 0;
  usize size = sizeof(mem);

  if (sysctlbyname("hw.memsize", &mem, &size, nullptr, 0) == -1)
    return Err(DracError::withErrno("Failed to get memory info"));

  return mem;
}

fn os::GetNowPlaying() -> Result<MediaInfo, DracError> { return GetCurrentPlayingInfo(); }

fn os::GetOSVersion() -> Result<String, DracError> { return GetMacOSVersion(); }

fn os::GetDesktopEnvironment() -> Result<String, DracError> {
  return "Aqua"; // TODO: Implement
}

fn os::GetWindowManager() -> Result<String, DracError> {
  return "Yabai"; // TODO: Implement
}

fn os::GetKernelVersion() -> Result<String, DracError> {
  Array<char, 256> kernelVersion {};
  usize            kernelVersionLen = sizeof(kernelVersion);

  if (sysctlbyname("kern.osrelease", kernelVersion.data(), &kernelVersionLen, nullptr, 0) == -1)
    return Err(DracError::withErrno("Failed to get kernel version"));

  return kernelVersion.data();
}

fn os::GetHost() -> Result<String, DracError> {
  Array<char, 256> hwModel {};
  usize            hwModelLen = sizeof(hwModel);

  if (sysctlbyname("hw.model", hwModel.data(), &hwModelLen, nullptr, 0) == -1)
    return Err(DracError::withErrno("Failed to get host info"));

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
    return Err(DracError::withErrno("Failed to get host info"));

  return String(iter->second);
}

fn os::GetDiskUsage() -> Result<DiskSpace, DracError> {
  struct statvfs vfs;

  if (statvfs("/", &vfs) != 0)
    return Err(DracError::withErrno("Failed to get disk usage"));

  return DiskSpace { .used_bytes  = (vfs.f_blocks - vfs.f_bfree) * vfs.f_frsize,
                     .total_bytes = vfs.f_blocks * vfs.f_frsize };
}

fn os::GetPackageCount() -> Result<u64, DracError> { return shared::GetPackageCount(); }

fn os::GetShell() -> Result<String, DracError> {
  return "Fish"; // TODO: Implement
}

#endif
