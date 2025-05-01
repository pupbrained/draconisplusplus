#ifdef __APPLE__

// clang-format off
#import "bridge.hpp"

#import <dispatch/dispatch.h>
#include <expected>
#include <functional>
#include <memory>
#import <objc/runtime.h>
#include <string>
#include <utility>

#include "src/core/util/error.hpp"
// clang-format on

using util::error::DracErrorCode;
using util::types::Err, util::types::Option, util::types::None;

using MRMediaRemoteGetNowPlayingInfoFunction =
  void (*)(dispatch_queue_t queue, void (^handler)(NSDictionary* information));

@implementation Bridge
+ (void)fetchCurrentPlayingMetadata:(void (^)(std::expected<NSDictionary*, const char*>))completion {
  CFURLRef urlRef = CFURLCreateWithFileSystemPath(
    kCFAllocatorDefault, CFSTR("/System/Library/PrivateFrameworks/MediaRemote.framework"), kCFURLPOSIXPathStyle, false
  );

  if (!urlRef) {
    completion(std::unexpected("Failed to create CFURL for MediaRemote framework"));
    return;
  }

  CFBundleRef bundleRef = CFBundleCreate(kCFAllocatorDefault, urlRef);

  CFRelease(urlRef);

  if (!bundleRef) {
    completion(std::unexpected("Failed to create bundle for MediaRemote framework"));
    return;
  }

  auto mrMediaRemoteGetNowPlayingInfo = std::bit_cast<MRMediaRemoteGetNowPlayingInfoFunction>(
    CFBundleGetFunctionPointerForName(bundleRef, CFSTR("MRMediaRemoteGetNowPlayingInfo"))
  );

  if (!mrMediaRemoteGetNowPlayingInfo) {
    CFRelease(bundleRef);
    completion(std::unexpected("Failed to get MRMediaRemoteGetNowPlayingInfo function pointer"));
    return;
  }

  std::shared_ptr<std::remove_pointer_t<CFBundleRef>> sharedBundle(bundleRef, [](CFBundleRef bundle) {
    if (bundle)
      CFRelease(bundle);
  });

  mrMediaRemoteGetNowPlayingInfo(
    dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
    ^(NSDictionary* information) {
      completion(
        information ? std::expected<NSDictionary*, const char*>(information)
                    : std::unexpected("No now playing information")
      );
    }
  );
}

+ (Result<String, DracError>)macOSVersion {
  NSProcessInfo*           processInfo = [NSProcessInfo processInfo];
  NSOperatingSystemVersion osVersion   = [processInfo operatingSystemVersion];

  NSString* versionNumber = nil;
  if (osVersion.patchVersion == 0) {
    versionNumber = [NSString stringWithFormat:@"%ld.%ld", osVersion.majorVersion, osVersion.minorVersion];
  } else {
    versionNumber = [NSString
      stringWithFormat:@"%ld.%ld.%ld", osVersion.majorVersion, osVersion.minorVersion, osVersion.patchVersion];
  }

  NSDictionary* versionNames =
    @{ @11 : @"Big Sur", @12 : @"Monterey", @13 : @"Ventura", @14 : @"Sonoma", @15 : @"Sequoia" };

  NSNumber* majorVersion = @(osVersion.majorVersion);
  NSString* versionName  = versionNames[majorVersion] ? versionNames[majorVersion] : @"Unknown";

  NSString* fullVersion = [NSString stringWithFormat:@"macOS %@ %@", versionNumber, versionName];
  return String([fullVersion UTF8String]);
}
@end

extern "C++" {
  // NOLINTBEGIN(misc-use-internal-linkage)
  fn GetCurrentPlayingInfo() -> Result<MediaInfo, DracError> {
    __block Result<MediaInfo, DracError> result;

    const dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);

    [Bridge fetchCurrentPlayingMetadata:^(std::expected<NSDictionary*, const char*> metadataResult) {
      if (!metadataResult) {
        result = Err(DracError(DracErrorCode::InternalError, metadataResult.error()));
        dispatch_semaphore_signal(semaphore);
        return;
      }

      const NSDictionary* const metadata = *metadataResult;
      if (!metadata) {
        result = Err(DracError(DracErrorCode::InternalError, "No metadata"));
        dispatch_semaphore_signal(semaphore);
        return;
      }

      const NSString* const title  = metadata[@"kMRMediaRemoteNowPlayingInfoTitle"];
      const NSString* const artist = metadata[@"kMRMediaRemoteNowPlayingInfoArtist"];

      result = MediaInfo(
        title ? Option(String([title UTF8String])) : None, artist ? Option(String([artist UTF8String])) : None
      );

      dispatch_semaphore_signal(semaphore);
    }];

    dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
    return result;
  }

  fn GetMacOSVersion() -> Result<String, DracError> { return [Bridge macOSVersion]; }
  // NOLINTEND(misc-use-internal-linkage)
}

#endif
