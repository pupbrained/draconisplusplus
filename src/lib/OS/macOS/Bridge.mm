#ifdef __APPLE__

// clang-format off
#import "Bridge.hpp"

#import <dispatch/dispatch.h>
#import <objc/runtime.h>

#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "Util/Error.hpp"
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
    completion(nil, [NSError errorWithDomain:@"com.draconis.error"
                                        code:1
                                    userInfo:@{
                                      NSLocalizedDescriptionKey : @"Failed to create CFURL for MediaRemote framework",
                                      NSLocalizedFailureReasonErrorKey : @"The MediaRemote framework path could not be resolved",
                                      NSLocalizedRecoverySuggestionErrorKey : @"Ensure the MediaRemote framework exists at /System/Library/PrivateFrameworks/MediaRemote.framework"
                                    }]);
    return;
  }

  CFBundleRef bundleRef = CFBundleCreate(kCFAllocatorDefault, urlRef);

  CFRelease(urlRef);

  if (!bundleRef) {
    completion(nil, [NSError errorWithDomain:@"com.draconis.error"
                                        code:2
                                    userInfo:@{
                                      NSLocalizedDescriptionKey : @"Failed to create bundle for MediaRemote framework",
                                      NSLocalizedFailureReasonErrorKey : @"The MediaRemote framework could not be loaded",
                                      NSLocalizedRecoverySuggestionErrorKey : @"Ensure you have the necessary permissions to access the MediaRemote framework"
                                    }]);
    return;
  }

  auto mrMediaRemoteGetNowPlayingInfo = std::bit_cast<MRMediaRemoteGetNowPlayingInfoFunction>(
    CFBundleGetFunctionPointerForName(bundleRef, CFSTR("MRMediaRemoteGetNowPlayingInfo"))
  );

  if (!mrMediaRemoteGetNowPlayingInfo) {
    CFRelease(bundleRef);
    completion(nil, [NSError errorWithDomain:@"com.draconis.error"
                                        code:3
                                    userInfo:@{
                                      NSLocalizedDescriptionKey : @"Failed to get MRMediaRemoteGetNowPlayingInfo function pointer",
                                      NSLocalizedFailureReasonErrorKey : @"The MediaRemote framework does not export the required function",
                                      NSLocalizedRecoverySuggestionErrorKey : @"This may indicate an incompatible macOS version or missing permissions"
                                    }]);
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
        completion(nil, [NSError errorWithDomain:@"com.draconis.error"
                                            code:4
                                        userInfo:@ {
                                          NSLocalizedDescriptionKey : @"No now playing information",
                                          NSLocalizedFailureReasonErrorKey : @"No media is currently playing or the media player is not accessible",
                                          NSLocalizedRecoverySuggestionErrorKey : @"Try playing media in a supported application"
                                        }]);
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
        using matchit::match, matchit::is, matchit::or_, matchit::_;

        DracErrorCode errorCode = match(error.code)(
          is | or_(1, 4) = DracErrorCode::NotFound,
          is | or_(2, 3) = DracErrorCode::ApiUnavailable,
          is | _         = DracErrorCode::PlatformSpecific
        );

        result = Err(DracError(
          errorCode,
          String([NSString stringWithFormat:@"%@ - %@", error.localizedDescription, error.localizedFailureReason].UTF8String)
        ));

        dispatch_semaphore_signal(semaphore);
        return;
      }

      if (!information) {
        result = Err(DracError(
          DracErrorCode::NotFound,
          "No media metadata available - no media is currently playing"
        ));
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
      : Err(DracError(DracErrorCode::PlatformSpecific, "Failed to get macOS version"));
  }
  // NOLINTEND(misc-use-internal-linkage)
}

#endif
