#ifdef __APPLE__

// clang-format off
#import "Bridge.hpp"

#include <CoreFoundation/CFURL.h> // CFURLCreateWithFileSystemPath, CFURLRef, kCFURLPOSIXPathStyle
#include <dispatch/dispatch.h>    // DISPATCH_QUEUE_PRIORITY_DEFAULT, DISPATCH_TIME_FOREVER, dispatch_get_global_queue, dispatch_queue_t, dispatch_semaphore_create, ...
#include <memory>                 // std::shared_ptr

#include "Util/Error.hpp"
// clang-format on

using namespace util::types;
using util::error::DracError;
using enum util::error::DracErrorCode;

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
@end

extern "C++" {
  // NOLINTBEGIN(misc-use-internal-linkage)
  fn GetCurrentPlayingInfo() -> Result<MediaInfo> {
    __block Result<MediaInfo> result;

    const dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);

    [Bridge fetchCurrentPlayingMetadata:^(NSDictionary* __nullable information, NSError* __nullable error) {
      if (error) {
        using matchit::match, matchit::is, matchit::or_;

        result = Err(DracError(
          match(error.code)(
            is | or_(1, 4) = NotFound,
            is | or_(2, 3) = ApiUnavailable
          ),
          [NSString stringWithFormat:@"%@ - %@", error.localizedDescription, error.localizedFailureReason].UTF8String
        ));

        dispatch_semaphore_signal(semaphore);
        return;
      }

      if (!information) {
        result = Err(DracError(
          NotFound,
          "No media metadata available - no media is currently playing"
        ));
        dispatch_semaphore_signal(semaphore);
        return;
      }

      const NSString* const title  = information[@"kMRMediaRemoteNowPlayingInfoTitle"];
      const NSString* const artist = information[@"kMRMediaRemoteNowPlayingInfoArtist"];

      result = MediaInfo(
        title
          ? Option([title UTF8String])
          : None,
        artist
          ? Option([artist UTF8String])
          : None
      );

      dispatch_semaphore_signal(semaphore);
    }];

    dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
    return result;
  }
  // NOLINTEND(misc-use-internal-linkage)
}

#endif
