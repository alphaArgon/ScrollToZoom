/*
 *  Untitled.h
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/7/29.
 *  Copyright © 2025 alphaArgon.
 */

#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN


@interface STZPermissionViewController : NSViewController

@property(nonatomic, readonly) BOOL cancelled;
- (void)showPrompt:(nullable id)sender;

@end


NS_ASSUME_NONNULL_END
