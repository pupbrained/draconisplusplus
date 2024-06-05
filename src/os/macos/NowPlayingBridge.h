// NowPlayingBridge.h

#ifdef __OBJC__
#import <Foundation/Foundation.h>

@interface NowPlayingBridge : NSObject
+ (NSDictionary*)currentPlayingMetadata;
@end
#else
extern "C" {
const char* getCurrentPlayingTitle();
const char* getCurrentPlayingArtist();
}
#endif
