/**
 * @file Bridge.mm
 * @brief macOS-specific implementations for retrieving system information.
 *
 * This file contains functions that interact with private and public macOS frameworks
 * (MediaRemote and Metal) to fetch details about the currently playing media and the system's GPU.
 * This implementation is conditionally compiled and should only be included on Apple platforms.
 */

#ifdef __APPLE__

  #include "Bridge.hpp"

  #include <Metal/Metal.h>       // For MTLDevice to identify the GPU.
  #include <dispatch/dispatch.h> // For Grand Central Dispatch (GCD) semaphores and queues.

  #include <Drac++/Utils/Error.hpp>

using namespace draconis::utils::types;
using draconis::utils::error::DracError;
using enum draconis::utils::error::DracErrorCode;

// Forward-declare the function pointer type for the private MediaRemote API.
using MRMediaRemoteGetNowPlayingInfoFn =
  void (*)(dispatch_queue_t queue, void (^handler)(NSDictionary* information));

namespace draconis::core::system::macOS {
  fn GetNowPlayingInfo() -> Result<MediaInfo> {
    @autoreleasepool {
      // Since MediaRemote.framework is private, we cannot link against it directly.
      // Instead, it must be loaded at runtime using CFURL and CFBundle.
      CFURLRef urlRef = CFURLCreateWithFileSystemPath(
        kCFAllocatorDefault,
        CFSTR("/System/Library/PrivateFrameworks/MediaRemote.framework"),
        kCFURLPOSIXPathStyle,
        false
      );

      if (!urlRef)
        return Err(DracError(NotFound, "Failed to create CFURL for MediaRemote.framework"));

      // Create a bundle from the URL (basically an in-memory representation of the framework).
      CFBundleRef bundleRef = CFBundleCreate(kCFAllocatorDefault, urlRef);
      CFRelease(urlRef); // urlRef is no longer needed after bundle creation.

      if (!bundleRef)
        return Err(DracError(ApiUnavailable, "Failed to create bundle for MediaRemote.framework"));

      // Ensure CFRelease is called even if an error occurs.
      SharedPointer<std::remove_pointer_t<CFBundleRef>> managedBundle(bundleRef, [](CFBundleRef bundle) {
        if (bundle)
          CFRelease(bundle);
      });

      // Get a pointer to the MRMediaRemoteGetNowPlayingInfo function from the bundle.
      auto mrMediaRemoteGetNowPlayingInfo = std::bit_cast<MRMediaRemoteGetNowPlayingInfoFn>(
        CFBundleGetFunctionPointerForName(bundleRef, CFSTR("MRMediaRemoteGetNowPlayingInfo"))
      );

      if (!mrMediaRemoteGetNowPlayingInfo)
        return Err(DracError(ApiUnavailable, "Failed to get MRMediaRemoteGetNowPlayingInfo function pointer"));

      // A semaphore is used to make this asynchronous call behave synchronously.
      // Wait on the semaphore until the callback signals that it has completed.
      __block Result<MediaInfo>  result;
      const dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);

      mrMediaRemoteGetNowPlayingInfo(
        dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
        ^(NSDictionary* information) {
          if (!information) {
            result = Err(DracError(NotFound, "No media is currently playing"));
          } else {
            // Extract the title and artist from the dictionary. These keys are also from the private framework.
            const NSString* const title  = [information objectForKey:@"kMRMediaRemoteNowPlayingInfoTitle"];
            const NSString* const artist = [information objectForKey:@"kMRMediaRemoteNowPlayingInfoArtist"];

            result = MediaInfo(
              title ? Some([title UTF8String]) : None,
              artist ? Some([artist UTF8String]) : None
            );
          }

          // Signal the semaphore to unblock the waiting thread.
          dispatch_semaphore_signal(semaphore);
        }
      );

      // Block this thread indefinitely until the callback signals completion.
      dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
      return result;
    }
  }

  fn GetGPUModel() -> Result<String> {
    @autoreleasepool {
      // Get the default Metal device, which typically corresponds to the active, primary GPU.
      id<MTLDevice> device = MTLCreateSystemDefaultDevice();

      if (!device)
        return Err(DracError(ApiUnavailable, "Failed to get default Metal device. No Metal-compatible GPU found."));

      NSString* gpuName = device.name;
      if (!gpuName)
        return Err(DracError(NotFound, "Failed to get GPU name from Metal device."));

      return [gpuName UTF8String];
    }
  }

  fn GetOSVersion() -> Result<OSInfo> {
    @autoreleasepool {
      using matchit::match, matchit::is, matchit::_;

      // NSProcessInfo is the easiest/fastest way to get the OS version.
      NSProcessInfo*           processInfo = [NSProcessInfo processInfo];
      NSOperatingSystemVersion version     = [processInfo operatingSystemVersion];

      return OSInfo(
        "macOS",
        std::format(
          "{}.{} {}",
          version.majorVersion,
          version.minorVersion,
          match(version.majorVersion)(
            is | 11 = "Big Sur",
            is | 12 = "Monterey",
            is | 13 = "Ventura",
            is | 14 = "Sonoma",
            is | 15 = "Sequoia",
            is | 26 = "Tahoe",
            is | _  = "Unknown"
          )
        ),
        "macos"
      );
    }
  }
} // namespace draconis::core::system::macOS

#endif
