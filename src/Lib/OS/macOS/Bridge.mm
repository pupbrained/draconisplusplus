#ifdef __APPLE__

  #include "Bridge.hpp"

  #include <Metal/Metal.h>       // MTLDevice
  #include <dispatch/dispatch.h> // dispatch_semaphore_t, dispatch_queue_t
  #include <memory>              // std::shared_ptr, std::remove_pointer_t

  #include <DracUtils/Error.hpp>

using namespace util::types;
using util::error::DracError;
using enum util::error::DracErrorCode;

using MRMediaRemoteGetNowPlayingInfoFunction =
  void (*)(dispatch_queue_t queue, void (^handler)(NSDictionary* information));

namespace os::bridge {
  fn GetNowPlayingInfo() -> Result<MediaInfo> {
    @autoreleasepool {
      CFURLRef urlRef = CFURLCreateWithFileSystemPath(
        kCFAllocatorDefault,
        CFSTR("/System/Library/PrivateFrameworks/MediaRemote.framework"),
        kCFURLPOSIXPathStyle,
        false
      );

      if (!urlRef)
        return Err(DracError(NotFound, "Failed to create CFURL for MediaRemote.framework"));

      CFBundleRef bundleRef = CFBundleCreate(kCFAllocatorDefault, urlRef);
      CFRelease(urlRef);

      if (!bundleRef) {
        return Err(DracError(ApiUnavailable, "Failed to create bundle for MediaRemote.framework"));
      }

      std::shared_ptr<std::remove_pointer_t<CFBundleRef>> managedBundle(bundleRef, [](CFBundleRef bundle) {
        if (bundle)
          CFRelease(bundle);
      });

      auto mrMediaRemoteGetNowPlayingInfo = std::bit_cast<MRMediaRemoteGetNowPlayingInfoFunction>(
        CFBundleGetFunctionPointerForName(bundleRef, CFSTR("MRMediaRemoteGetNowPlayingInfo"))
      );

      if (!mrMediaRemoteGetNowPlayingInfo)
        return Err(DracError(ApiUnavailable, "Failed to get MRMediaRemoteGetNowPlayingInfo function pointer"));

      __block Result<MediaInfo>  result;
      const dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);

      mrMediaRemoteGetNowPlayingInfo(
        dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
        ^(NSDictionary* information) {
          if (!information)
            result = Err(DracError(NotFound, "No media metadata available - no media is currently playing"));
          else {
            const NSString* const title  = [information objectForKey:@"kMRMediaRemoteNowPlayingInfoTitle"];
            const NSString* const artist = [information objectForKey:@"kMRMediaRemoteNowPlayingInfoArtist"];

            result = MediaInfo(
              title ? Option([title UTF8String]) : None,
              artist ? Option([artist UTF8String]) : None
            );
          }
          dispatch_semaphore_signal(semaphore);
        }
      );

      dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
      return result;
    }
  }

  fn GetGPUModel() -> Result<String> {
    @autoreleasepool {
      id<MTLDevice> device = MTLCreateSystemDefaultDevice();
      if (!device)
        return Err(DracError(ApiUnavailable, "Failed to get default Metal device. No Metal-compatible GPU found."));

      NSString* gpuName = device.name;
      if (!gpuName)
        return Err(DracError(NotFound, "Failed to get GPU name from Metal device."));

      return [gpuName UTF8String];
    }
  }
} // namespace os::bridge

#endif
