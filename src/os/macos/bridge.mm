#ifdef __APPLE__

#import <dispatch/dispatch.h>
#include <expected>
#import <objc/runtime.h>
#include <string>

#import "bridge.h"

using MRMediaRemoteGetNowPlayingInfoFunction =
  void (*)(dispatch_queue_t queue, void (^handler)(NSDictionary* information));

@implementation Bridge
+ (void)fetchCurrentPlayingMetadata:(void (^)(std::expected<NSDictionary*, const char*>))completion {
  CFURLRef ref = CFURLCreateWithFileSystemPath(
    kCFAllocatorDefault, CFSTR("/System/Library/PrivateFrameworks/MediaRemote.framework"), kCFURLPOSIXPathStyle, false
  );

  if (!ref) {
    completion(std::unexpected("Failed to create CFURL for MediaRemote framework"));
    return;
  }

  CFBundleRef bundle = CFBundleCreate(kCFAllocatorDefault, ref);
  CFRelease(ref);

  if (!bundle) {
    completion(std::unexpected("Failed to create bundle for MediaRemote framework"));
    return;
  }

  auto mrMediaRemoteGetNowPlayingInfo = std::bit_cast<MRMediaRemoteGetNowPlayingInfoFunction>(
    CFBundleGetFunctionPointerForName(bundle, CFSTR("MRMediaRemoteGetNowPlayingInfo"))
  );

  if (!mrMediaRemoteGetNowPlayingInfo) {
    CFRelease(bundle);
    completion(std::unexpected("Failed to get MRMediaRemoteGetNowPlayingInfo function pointer"));
    return;
  }

  mrMediaRemoteGetNowPlayingInfo(
    dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
    ^(NSDictionary* information) {
      NSDictionary* nowPlayingInfo = information; // Immutable, no copy needed
      CFRelease(bundle);
      completion(
        nowPlayingInfo ? std::expected<NSDictionary*, const char*>(nowPlayingInfo)
                       : std::unexpected("No now playing information")
      );
    }
  );
}

+ (std::expected<string, string>)macOSVersion {
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
  return std::string([fullVersion UTF8String]);
}
@end

extern "C++" {
  // NOLINTBEGIN(misc-use-internal-linkage)
  fn GetCurrentPlayingInfo() -> std::expected<std::string, NowPlayingError> {
    __block std::expected<std::string, NowPlayingError> result;
    dispatch_semaphore_t                                semaphore = dispatch_semaphore_create(0);

    [Bridge fetchCurrentPlayingMetadata:^(std::expected<NSDictionary*, const char*> metadataResult) {
      if (!metadataResult) {
        result = std::unexpected(NowPlayingError { metadataResult.error() });
        dispatch_semaphore_signal(semaphore);
        return;
      }

      NSDictionary* metadata = *metadataResult;
      if (!metadata) {
        result = std::unexpected(NowPlayingError { NowPlayingCode::NoPlayers });
        dispatch_semaphore_signal(semaphore);
        return;
      }

      NSString* title  = [metadata objectForKey:@"kMRMediaRemoteNowPlayingInfoTitle"];
      NSString* artist = [metadata objectForKey:@"kMRMediaRemoteNowPlayingInfoArtist"];

      if (!title && !artist)
        result = std::unexpected("No metadata");
      else if (!title)
        result = std::string([artist UTF8String]);
      else if (!artist)
        result = std::string([title UTF8String]);
      else
        result = std::string([[NSString stringWithFormat:@"%@ - %@", title, artist] UTF8String]);

      dispatch_semaphore_signal(semaphore);
    }];

    dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
    return result;
  }

  fn GetMacOSVersion() -> std::expected<string, string> { return [Bridge macOSVersion]; }
  // NOLINTEND(misc-use-internal-linkage)
}

#endif
