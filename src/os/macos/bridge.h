#pragma once

#ifdef __APPLE__

#include "../../util/macros.h"

#ifdef __OBJC__

#import <Foundation/Foundation.h>
#include <expected>

@interface Bridge : NSObject
+ (NSDictionary*)currentPlayingMetadata;
+ (std::expected<const char*, const char*>)macOSVersion;
@end

#else

extern "C++" {
  fn GetCurrentPlayingTitle() -> const char*;
  fn GetCurrentPlayingArtist() -> const char*;
  fn GetMacOSVersion() -> std::expected<const char*, const char*>;
}

#endif
#endif
