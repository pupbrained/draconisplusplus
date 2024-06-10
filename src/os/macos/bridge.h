#pragma once

#ifdef __APPLE__
#ifdef __OBJC__
#import <Foundation/Foundation.h>

@interface Bridge : NSObject
+ (NSDictionary*)currentPlayingMetadata;
+ (NSString*)macOSVersion;
@end
#else
#include "util/numtypes.h"

extern "C" {
  fn GetCurrentPlayingTitle() -> const char*;
  fn GetCurrentPlayingArtist() -> const char*;
  fn GetMacOSVersion() -> const char*;
}
#endif
#endif
