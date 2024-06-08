#ifdef __APPLE__

#import "bridge.h"
#import <dispatch/dispatch.h>
#import <objc/runtime.h>

using MRMediaRemoteGetNowPlayingInfoFunction = void (*)(
    dispatch_queue_t queue, void (^handler)(NSDictionary *information));

@implementation Bridge

+ (NSDictionary *)currentPlayingMetadata {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"

  CFURLRef ref = (__bridge CFURLRef)
      [NSURL fileURLWithPath:
                 @"/System/Library/PrivateFrameworks/MediaRemote.framework"];

#pragma clang diagnostic pop

  CFBundleRef bundle = CFBundleCreate(kCFAllocatorDefault, ref);

  if (!bundle) {
    NSLog(@"Failed to load MediaRemote framework");
    return nil;
  }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"

  auto mrMediaRemoteGetNowPlayingInfo =
      (MRMediaRemoteGetNowPlayingInfoFunction)CFBundleGetFunctionPointerForName(
          bundle, CFSTR("MRMediaRemoteGetNowPlayingInfo"));

#pragma clang diagnostic pop

  if (!mrMediaRemoteGetNowPlayingInfo) {
    NSLog(@"Failed to get function pointer for MRMediaRemoteGetNowPlayingInfo");
    CFRelease(bundle);
    return nil;
  }

  __block NSDictionary *nowPlayingInfo = nil;
  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);

  mrMediaRemoteGetNowPlayingInfo(
      dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
      ^(NSDictionary *information) {
        nowPlayingInfo = [information copy];
        dispatch_semaphore_signal(semaphore);
      });

  dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);

  CFRelease(bundle);
  return nowPlayingInfo;
}

+ (NSString *)macOSVersion {
  NSProcessInfo *processInfo = [NSProcessInfo processInfo];
  NSOperatingSystemVersion osVersion = [processInfo operatingSystemVersion];
  NSString *version =
      [NSString stringWithFormat:@"%ld.%ld.%ld", (long)osVersion.majorVersion,
                                 (long)osVersion.minorVersion,
                                 (long)osVersion.patchVersion];
  return version;
}

@end

extern "C" {
const char *GetCurrentPlayingTitle() {
  NSDictionary *metadata = [Bridge currentPlayingMetadata];

  if (metadata == nil)
    return nullptr;

  NSString *title =
      [metadata objectForKey:@"kMRMediaRemoteNowPlayingInfoTitle"];

  if (title)
    return strdup([title UTF8String]);

  return nullptr;
}

const char *GetCurrentPlayingArtist() {
  NSDictionary *metadata = [Bridge currentPlayingMetadata];

  if (metadata == nil)
    return nullptr;

  NSString *artist =
      [metadata objectForKey:@"kMRMediaRemoteNowPlayingInfoArtist"];

  if (artist)
    return strdup([artist UTF8String]);

  return nullptr;
}

const char *GetMacOSVersion() {
  NSString *version = [Bridge macOSVersion];
  if (version) {
    return strdup([version UTF8String]);
  }
  return nullptr;
}
}

#endif

