/*
 *  STZLaunchAtLogin.m
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/4/19.
 *  Copyright © 2025 alphaArgon.
 */

#import "STZLaunchAtLogin.h"
#import <ServiceManagement/ServiceManagement.h>
#import <ApplicationServices/ApplicationServices.h>
#import <AppKit/NSWorkspace.h>


#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

static bool legacyGetEnabled(void) {
    CFURLRef appURL = (__bridge void *)[[NSBundle mainBundle] bundleURL];
    bool win = false;

    LSSharedFileListRef list = LSSharedFileListCreate(kCFAllocatorDefault, kLSSharedFileListSessionLoginItems, NULL);
    if (!list) {goto RELEASE_NONE;}

    CFArrayRef items = LSSharedFileListCopySnapshot(list, NULL);
    if (!items) {goto RELEASE_LIST;}

    CFIndex count = CFArrayGetCount(items);
    for (CFIndex i = 0; i < count; i++) {
        LSSharedFileListItemRef item = (void *)CFArrayGetValueAtIndex(items, i);
        CFURLRef url = LSSharedFileListItemCopyResolvedURL(item, 0, NULL);
        if (!url) {continue;}

        win = CFEqual(url, appURL);
        CFRelease(url);
        if (win) {break;}
    }

    CFRelease(items);
RELEASE_LIST:
    CFRelease(list);
RELEASE_NONE:
    return win;
}

static bool legacySetEnabled(bool enable) {
    CFURLRef appURL = (__bridge void *)[[NSBundle mainBundle] bundleURL];
    bool win = false;

    LSSharedFileListRef list = LSSharedFileListCreate(kCFAllocatorDefault, kLSSharedFileListSessionLoginItems, NULL);
    if (!list) {return false;}

    if (enable) {
        LSSharedFileListItemRef item = LSSharedFileListInsertItemURL(list, kLSSharedFileListItemLast, NULL, NULL, appURL, NULL, NULL);
        if (item) {
            win = true;
            CFRelease(item);
        }

    } else {
        CFArrayRef items = LSSharedFileListCopySnapshot(list, NULL);
        if (!items) {goto RELEASE_LIST;}

        CFIndex count = CFArrayGetCount(items);
        for (CFIndex i = 0; i < count; i++) {
            LSSharedFileListItemRef item = (void *)CFArrayGetValueAtIndex(items, i);
            CFURLRef url = LSSharedFileListItemCopyResolvedURL(item, 0, NULL);
            if (!url) {continue;}

            bool equal = CFEqual(url, appURL);
            CFRelease(url);
            if (equal) {
                win = LSSharedFileListItemRemove(list, item) == 0;
                break;
            }
        }

        CFRelease(items);
    }

RELEASE_LIST:
    CFRelease(list);
RELEASE_NONE:
    return win;
}

#pragma clang diagnostic pop


bool STZShouldEnableLaunchAtLogin(void) {
    NSURL *url = [[NSWorkspace sharedWorkspace] URLForApplicationWithBundleIdentifier:[[NSBundle mainBundle] bundleIdentifier]];
    if (![url isEqual:[[NSBundle mainBundle] bundleURL]]) {return false;}

    //  Should be in:
    //  /Applications/
    //  ~/Applications/
    //  /Users/Shared/Applications/

    NSString *path = [url path];
    if ([path hasPrefix:@"/Applications/"]) {return true;}
    if ([path hasPrefix:@"/Users/Shared/Applications/"]) {return true;}
    if ([path hasPrefix:[NSHomeDirectory() stringByAppendingPathComponent:@"Applications/"]]) {return true;}
    return false;
}


bool STZGetLaunchAtLoginEnabled(void) {
    if (@available(macOS 13, *)) {
        return [[SMAppService mainAppService] status] == SMAppServiceStatusRequiresApproval
            || legacyGetEnabled();
    } else {
        return legacyGetEnabled();
    }
}


bool STZSetLaunchAtLoginEnabled(bool enable) {
    if (@available(macOS 13, *)) {
        return enable
            ? [[SMAppService mainAppService] registerAndReturnError:NULL]
            : [[SMAppService mainAppService] unregisterAndReturnError:NULL];
    } else {
        return legacySetEnabled(enable);
    }
}
