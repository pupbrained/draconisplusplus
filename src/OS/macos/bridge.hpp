#pragma once

#ifdef __APPLE__

// clang-format off
#include "Util/Definitions.hpp"
#include "Util/Error.hpp"
#include "Util/Types.hpp"
// clang-format on

using util::error::DracError;
using util::types::MediaInfo, util::types::String, util::types::Result;

  #ifdef __OBJC__
    #import <Foundation/Foundation.h> // Foundation

@interface Bridge : NSObject
+ (void)fetchCurrentPlayingMetadata:(void (^_Nonnull)(NSDictionary* __nullable, NSError* __nullable))completion;
+ (NSString* __nullable)macOSVersion;
@end
  #else
extern "C++" {
  fn GetCurrentPlayingInfo() -> Result<MediaInfo, DracError>;
  fn GetMacOSVersion() -> Result<String, DracError>;
}
  #endif
#endif
