// NowPlayingBridge.h

#ifdef __OBJC__
#import <Foundation/Foundation.h>

@interface NowPlayingBridge : NSObject
+ (NSDictionary*)currentPlayingMetadata;
@end
#else
extern "C" {
const char* GetCurrentPlayingTitle();
const char* GetCurrentPlayingArtist();
}
#endif
