/*
 *  AppDelegate.m
 *  MagicZoom
 *
 *  Created by alpha on 2025/5/6.
 *  Copyright © 2025 alphaArgon.
 */

#import "AppDelegate.h"
#import "MGZScrollToZoom.h"
#import "MGZWindow.h"
#import "STZOverlay.h"
#import "GeneratedAssetSymbols.h"


static NSString *const REPO_URL_PATH = @"https://github.com/alphaArgon/ScrollToZoom";


@implementation AppDelegate {
    NSStatusItem *_statusItem;
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    [[NSApplication sharedApplication] setActivationPolicy:NSApplicationActivationPolicyAccessory];

    _statusItem = [[NSStatusBar systemStatusBar] statusItemWithLength:NSVariableStatusItemLength];
    [[_statusItem button] setImage:[NSImage imageNamed:ACImageNameStatusIcon]];

    NSArray *nibObjects;
    [[NSBundle mainBundle] loadNibNamed:@"StatusMenu" owner:self topLevelObjects:&nibObjects];

    for (id object in nibObjects) {
        if ([object isKindOfClass:[NSMenu self]]) {
            [_statusItem setMenu:object];
            break;
        }
    }

    STZLoadArgumentsFromUserDefaults();
    if (!MGZSetScrollToZoomEnabled(true)) {
        [MGZWindow orderFrontSharedWindow];
    }
}

- (BOOL)applicationShouldHandleReopen:(NSApplication *)sender hasVisibleWindows:(BOOL)flag {
    [MGZWindow orderFrontSharedWindow];
    return NO;
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    return !MGZGetScrollToZoomEnabled();
}

- (void)orderFrontAboutPanel:(id)sender {
    [[NSApplication sharedApplication] activateIgnoringOtherApps:YES];
    [[NSApplication sharedApplication] orderFrontStandardAboutPanelWithOptions:@{
        NSAboutPanelOptionCredits: [[NSAttributedString alloc] initWithString:NSLocalizedString(@"view-source-on-github", nil)
                                                                   attributes:@{
            NSLinkAttributeName: [NSURL URLWithString:REPO_URL_PATH],
            NSFontAttributeName: [NSFont toolTipsFontOfSize:12]
        }]
    }];
}

- (void)orderFrontSharedWindow:(id)sender {
    [MGZWindow orderFrontSharedWindow];
}

@end
