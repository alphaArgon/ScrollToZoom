/*
 *  STZOptionsPanel.h
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/4/16.
 *  Copyright © 2025 alphaArgon.
 */

#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN


@interface STZOptionsPanel : NSWindow

@property(nonatomic, class, readonly) STZOptionsPanel *sharedPanel;

+ (void)orderFrontSharedPanel;
+ (void)noteChangeForBundleIdentifier:(NSString *)bundleID;

@end


NS_ASSUME_NONNULL_END
