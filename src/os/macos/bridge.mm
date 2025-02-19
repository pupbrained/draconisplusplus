#ifdef __APPLE__

#import <dispatch/dispatch.h>
#include <expected>
#import <objc/runtime.h>

#import "bridge.h"

#include "../../util/macros.h"

using MRMediaRemoteGetNowPlayingInfoFunction =
  void (*)(dispatch_queue_t queue, void (^handler)(NSDictionary* information));

@implementation Bridge
+ (NSDictionary*)currentPlayingMetadata {
  CFURLRef ref = CFURLCreateWithFileSystemPath(
    kCFAllocatorDefault, CFSTR("/System/Library/PrivateFrameworks/MediaRemote.framework"), kCFURLPOSIXPathStyle, false
  );

  if (!ref) {
    ERROR_LOG("Failed to load MediaRemote framework");
    return nil;
  }

  CFBundleRef bundle = CFBundleCreate(kCFAllocatorDefault, ref);
  CFRelease(ref);

  if (!bundle) {
    ERROR_LOG("Failed to load MediaRemote framework");
    return nil;
  }

  auto mrMediaRemoteGetNowPlayingInfo = std::bit_cast<MRMediaRemoteGetNowPlayingInfoFunction>(
    CFBundleGetFunctionPointerForName(bundle, CFSTR("MRMediaRemoteGetNowPlayingInfo"))
  );

  if (!mrMediaRemoteGetNowPlayingInfo) {
    ERROR_LOG("Failed to get function pointer for MRMediaRemoteGetNowPlayingInfo");
    CFRelease(bundle);
    return nil;
  }

  __block NSDictionary* nowPlayingInfo = nil;
  dispatch_semaphore_t  semaphore      = dispatch_semaphore_create(0);

  mrMediaRemoteGetNowPlayingInfo(
    dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
    ^(NSDictionary* information) {
      nowPlayingInfo = [information copy];
      dispatch_semaphore_signal(semaphore);
    }
  );

  dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);

  CFRelease(bundle);

  return nowPlayingInfo;
}

+ (std::expected<const char*, const char*>)macOSVersion {
  NSProcessInfo*           processInfo = [NSProcessInfo processInfo];
  NSOperatingSystemVersion osVersion   = [processInfo operatingSystemVersion];

  // Build version number string
  NSString* versionNumber = nullptr;
  if (osVersion.patchVersion == 0)
    versionNumber = [NSString stringWithFormat:@"%ld.%ld", osVersion.majorVersion, osVersion.minorVersion];
  else
    versionNumber = [NSString
      stringWithFormat:@"%ld.%ld.%ld", osVersion.majorVersion, osVersion.minorVersion, osVersion.patchVersion];

  // Map major version to name
  NSDictionary* versionNames =
    @{ @11 : @"Big Sur", @12 : @"Monterey", @13 : @"Ventura", @14 : @"Sonoma", @15 : @"Sequoia" };
  NSNumber* majorVersion = @(osVersion.majorVersion);
  NSString* versionName  = versionNames[majorVersion];

  if (!versionName)
    return std::unexpected("Unsupported macOS version");

  NSString* fullVersion = [NSString stringWithFormat:@"macOS %@ %@", versionNumber, versionName];
  return strdup([fullVersion UTF8String]);
}
@end

extern "C++" {
  // NOLINTBEGIN(misc-use-internal-linkage)
  fn GetCurrentPlayingTitle() -> const char* {
    NSDictionary* metadata = [Bridge currentPlayingMetadata];

    if (metadata == nil)
      return nullptr;

    NSString* title = [metadata objectForKey:@"kMRMediaRemoteNowPlayingInfoTitle"];

    if (title)
      return strdup([title UTF8String]);

    return nullptr;
  }

  fn GetCurrentPlayingArtist() -> const char* {
    NSDictionary* metadata = [Bridge currentPlayingMetadata];

    if (metadata == nil)
      return nullptr;

    NSString* artist = [metadata objectForKey:@"kMRMediaRemoteNowPlayingInfoArtist"];

    if (artist)
      return strdup([artist UTF8String]);

    return nullptr;
  }

  fn GetMacOSVersion() -> std::expected<const char*, const char*> { return [Bridge macOSVersion]; }
  // NOLINTEND(misc-use-internal-linkage)
}

#endif
