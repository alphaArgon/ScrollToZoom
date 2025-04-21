/*
 *  AppDelegate.h
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/1/24.
 *  Copyright © 2025 alphaArgon.
 */

#import <AppKit/AppKit.h>


@interface AppDelegate : NSObject <NSApplicationDelegate, NSMenuItemValidation>

- (IBAction)orderFrontAboutPanel:(nullable id)sender;
- (IBAction)orderFrontSharedWindow:(nullable id)sender;
- (IBAction)toggleEnabledForKeyApplication:(nullable id)sender;
- (IBAction)toggleExcludingFlagsForKeyApplication:(nullable id)sender;

@end
