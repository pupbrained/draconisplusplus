#ifdef __APPLE__

#import <dispatch/dispatch.h>
#import <objc/runtime.h>

#import "bridge.h"

using MRMediaRemoteGetNowPlayingInfoFunction = void (*)(
    dispatch_queue_t queue,
    void (^handler)(NSDictionary* information)
);

@implementation Bridge
+ (NSDictionary*)currentPlayingMetadata {
  CFURLRef ref = CFURLCreateWithFileSystemPath(
      kCFAllocatorDefault,
      CFSTR("/System/Library/PrivateFrameworks/MediaRemote.framework"),
      kCFURLPOSIXPathStyle,
      false
  );

  if (!ref) {
    NSLog(@"Failed to load MediaRemote framework");
    return nil;
  }

  CFBundleRef bundle = CFBundleCreate(kCFAllocatorDefault, ref);
  CFRelease(ref);

  if (!bundle) {
    NSLog(@"Failed to load MediaRemote framework");
    return nil;
  }

  MRMediaRemoteGetNowPlayingInfoFunction mrMediaRemoteGetNowPlayingInfo =
      reinterpret_cast<MRMediaRemoteGetNowPlayingInfoFunction>(
          CFBundleGetFunctionPointerForName(
              bundle, CFSTR("MRMediaRemoteGetNowPlayingInfo")
          )
      );

  if (!mrMediaRemoteGetNowPlayingInfo) {
    NSLog(@"Failed to get function pointer for MRMediaRemoteGetNowPlayingInfo");
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

+ (NSString*)macOSVersion {
  NSProcessInfo* processInfo = [NSProcessInfo processInfo];

  NSOperatingSystemVersion osVersion = [processInfo operatingSystemVersion];

  NSString* version = [NSString stringWithFormat:@"%ld.%ld.%ld",
                                                 osVersion.majorVersion,
                                                 osVersion.minorVersion,
                                                 osVersion.patchVersion];

  // Dictionary to map macOS versions to their respective names
  NSDictionary<NSNumber*, NSString*>* versionNames =
      @{@11 : @"Big Sur", @12 : @"Monterey", @13 : @"Ventura", @14 : @"Sonoma"};

  NSNumber* majorVersionNumber = @(osVersion.majorVersion);
  NSString* versionName        = versionNames[majorVersionNumber];

  if (versionName == nil)
    versionName = @"Unknown";

  NSString* fullVersion =
      [NSString stringWithFormat:@"macOS %@ %@", version, versionName];

  return fullVersion;
}
@end

#include "util/numtypes.h"

extern "C" {
  fn GetCurrentPlayingTitle() -> const char* {
    NSDictionary* metadata = [Bridge currentPlayingMetadata];

    if (metadata == nil)
      return nullptr;

    NSString* title =
        [metadata objectForKey:@"kMRMediaRemoteNowPlayingInfoTitle"];

    if (title)
      return strdup([title UTF8String]);

    return nullptr;
  }

  fn GetCurrentPlayingArtist() -> const char* {
    NSDictionary* metadata = [Bridge currentPlayingMetadata];

    if (metadata == nil)
      return nullptr;

    NSString* artist =
        [metadata objectForKey:@"kMRMediaRemoteNowPlayingInfoArtist"];

    if (artist)
      return strdup([artist UTF8String]);

    return nullptr;
  }

  fn GetMacOSVersion() -> const char* {
    NSString* version = [Bridge macOSVersion];

    if (version)
      return strdup([version UTF8String]);

    return nullptr;
  }
}

#endif
