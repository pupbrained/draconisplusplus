#ifdef __APPLE__

// clang-format off
#import "bridge.hpp"

#import <dispatch/dispatch.h>
#import <objc/runtime.h>

#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "src/util/error.hpp"
// clang-format on

using util::error::DracError, util::error::DracErrorCode;
using util::types::Err, util::types::Option, util::types::None, util::types::Result;

using MRMediaRemoteGetNowPlayingInfoFunction =
  void (*)(dispatch_queue_t queue, void (^handler)(NSDictionary* information));

@implementation Bridge
+ (void)fetchCurrentPlayingMetadata:(void (^)(NSDictionary* __nullable, NSError* __nullable))completion {
  CFURLRef urlRef = CFURLCreateWithFileSystemPath(
    kCFAllocatorDefault,
    CFSTR("/System/Library/PrivateFrameworks/MediaRemote.framework"),
    kCFURLPOSIXPathStyle,
    false
  );

  if (!urlRef) {
    completion(nil, [NSError errorWithDomain:@"com.draconis.error" code:1 userInfo:@{ NSLocalizedDescriptionKey : @"Failed to create CFURL for MediaRemote framework" }]);
    return;
  }

  CFBundleRef bundleRef = CFBundleCreate(kCFAllocatorDefault, urlRef);

  CFRelease(urlRef);

  if (!bundleRef) {
    completion(nil, [NSError errorWithDomain:@"com.draconis.error" code:1 userInfo:@{ NSLocalizedDescriptionKey : @"Failed to create bundle for MediaRemote framework" }]);
    return;
  }

  auto mrMediaRemoteGetNowPlayingInfo = std::bit_cast<MRMediaRemoteGetNowPlayingInfoFunction>(
    CFBundleGetFunctionPointerForName(bundleRef, CFSTR("MRMediaRemoteGetNowPlayingInfo"))
  );

  if (!mrMediaRemoteGetNowPlayingInfo) {
    CFRelease(bundleRef);
    completion(nil, [NSError errorWithDomain:@"com.draconis.error" code:1 userInfo:@{ NSLocalizedDescriptionKey : @"Failed to get MRMediaRemoteGetNowPlayingInfo function pointer" }]);
    return;
  }

  std::shared_ptr<std::remove_pointer_t<CFBundleRef>> sharedBundle(bundleRef, [](CFBundleRef bundle) {
    if (bundle)
      CFRelease(bundle);
  });

  mrMediaRemoteGetNowPlayingInfo(
    dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
    ^(NSDictionary* information) {
      if (!information) {
        completion(nil, [NSError errorWithDomain:@"com.draconis.error" code:1 userInfo:@ { NSLocalizedDescriptionKey : @"No now playing information" }]);
        return;
      }

      completion(information, nil);
    }
  );
}

+ (NSString*)macOSVersion {
  NSProcessInfo* processInfo = [NSProcessInfo processInfo];
  if (!processInfo)
    return nil;

  NSOperatingSystemVersion osVersion = [processInfo operatingSystemVersion];
  if (osVersion.majorVersion == 0)
    return nil;

  NSString* versionNumber = nil;
  if (osVersion.patchVersion == 0)
    versionNumber = [NSString stringWithFormat:@"%ld.%ld",
                                               osVersion.majorVersion,
                                               osVersion.minorVersion];
  else
    versionNumber = [NSString stringWithFormat:@"%ld.%ld.%ld",
                                               osVersion.majorVersion,
                                               osVersion.minorVersion,
                                               osVersion.patchVersion];

  if (!versionNumber)
    return nil;

  NSDictionary* versionNames =
    @{
      @11 : @"Big Sur",
      @12 : @"Monterey",
      @13 : @"Ventura",
      @14 : @"Sonoma",
      @15 : @"Sequoia"
    };

  NSNumber* majorVersion = @(osVersion.majorVersion);
  NSString* versionName  = versionNames[majorVersion] ? versionNames[majorVersion] : @"Unknown";

  NSString* fullVersion = [NSString stringWithFormat:@"macOS %@ %@", versionNumber, versionName];

  return fullVersion ? fullVersion : nil;
}
@end

extern "C++" {
  // NOLINTBEGIN(misc-use-internal-linkage)
  fn GetCurrentPlayingInfo() -> Result<MediaInfo> {
    __block Result<MediaInfo> result;

    const dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);

    [Bridge fetchCurrentPlayingMetadata:^(NSDictionary* __nullable information, NSError* __nullable error) {
      if (error) {
        result = Err(DracError(DracErrorCode::InternalError, [error.localizedDescription UTF8String]));
        dispatch_semaphore_signal(semaphore);
        return;
      }

      if (!information) {
        result = Err(DracError(DracErrorCode::InternalError, "No metadata"));
        dispatch_semaphore_signal(semaphore);
        return;
      }

      const NSString* const title  = information[@"kMRMediaRemoteNowPlayingInfoTitle"];
      const NSString* const artist = information[@"kMRMediaRemoteNowPlayingInfoArtist"];

      result = MediaInfo(
        title
          ? Option(String([title UTF8String]))
          : None,
        artist
          ? Option(String([artist UTF8String]))
          : None
      );

      dispatch_semaphore_signal(semaphore);
    }];

    dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
    return result;
  }

  fn GetMacOSVersion() -> Result<String> {
    NSString* version = [Bridge macOSVersion];

    return version
      ? Result<String>(String([version UTF8String]))
      : Err(DracError(DracErrorCode::InternalError, "Failed to get macOS version"));
  }
  // NOLINTEND(misc-use-internal-linkage)
}

#endif
