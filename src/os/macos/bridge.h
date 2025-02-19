#pragma once

#ifdef __APPLE__

#include <expected>
#include <string>

#include "../../util/macros.h"

#ifdef __OBJC__

#import <Foundation/Foundation.h>

@interface Bridge : NSObject
+ (void)fetchCurrentPlayingMetadata:(void (^)(std::expected<NSDictionary*, const char*>))completion;
+ (std::expected<string, string>)macOSVersion;
@end

#else

extern "C++" {
  fn GetCurrentPlayingInfo() -> std::expected<std::string, NowPlayingError>;
  fn GetMacOSVersion() -> std::expected<std::string, const char*>;
}

#endif
#endif
