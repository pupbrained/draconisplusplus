#pragma once

#ifdef __APPLE__

#include <expected>

#include "../../util/macros.h"

#ifdef __OBJC__

#import <Foundation/Foundation.h>

@interface Bridge : NSObject
+ (void)fetchCurrentPlayingMetadata:(void (^)(std::expected<NSDictionary*, const char*>))completion;
+ (std::expected<String, String>)macOSVersion;
@end

#else

extern "C++" {
  fn GetCurrentPlayingInfo() -> std::expected<String, NowPlayingError>;
  fn GetMacOSVersion() -> std::expected<String, String>;
}

#endif
#endif
