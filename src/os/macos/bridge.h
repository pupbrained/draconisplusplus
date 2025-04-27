#pragma once

#ifdef __APPLE__

#include <expected>

#include "../../util/macros.h"

#ifdef __OBJC__

#import <Foundation/Foundation.h>

@interface Bridge : NSObject
+ (void)fetchCurrentPlayingMetadata:(void (^)(Result<NSDictionary*, const char*>))completion;
+ (Result<String, OsError>)macOSVersion;
@end

#else

extern "C++" {
  fn GetCurrentPlayingInfo() -> Result<MediaInfo, NowPlayingError>;
  fn GetMacOSVersion() -> Result<String, OsError>;
}

#endif
#endif
