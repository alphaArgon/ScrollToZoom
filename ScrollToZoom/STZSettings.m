/*
 *  STZSettings.m
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/4/27.
 *  Copyright © 2025 alphaArgon.
 */

#import "STZSettings.h"
#import "STZProcessManager.h"
#import <Foundation/Foundation.h>


STZFlags STZScrollToZoomFlags = kSTZModifierOption;
double STZScrollToZoomMagnifier = 0.0025;
double STZScrollMomentumToZoomAttenuation = 0.8;
double STZScrollMinMomentumMagnification = 0.001;
bool STZDisablesDotDashDragToZoom = false;
CFMutableDictionaryRef STZEventTapOptionsForApps = NULL;
CFMutableDictionaryRef STZEventTapOptionsObjsForApps = NULL;


static NSString *const STZScrollToZoomFlagsKey = @"STZScrollToZoomFlags";
static NSString *const STZScrollToZoomMagnifierKey = @"STZScrollToZoomMagnifier";
static NSString *const STZScrollMomentumToZoomAttenuationKey = @"STZScrollMomentumToZoomAttenuation";
static NSString *const STZScrollMinMomentumMagnificationKey = @"STZScrollMinMomentumMagnification";
static NSString *const STZDisablesDotDashDragToZoomKey = @"STZDisableDotDashDragToZoom";
static NSString *const STZEventTapOptionsForAppsKey = @"STZEventTapOptionsForApps";


static struct {
    CFStringRef         bundleID;
    STZEventTapOptions  options;
} STZRecommendedEventTapOptions[] = {
    {CFSTR("org.mozilla.firefox"), kSTZEventTapExcludeFlags}
};


static double clamp(double x, double lo, double hi) {
    //  `NaN` gives average of `lo` and `hi`;
    if (x != x) {return (lo + hi) / 2;}
    return x < lo ? lo : x > hi ? hi : x;
}


STZFlags STZGetScrollToZoomFlags(void) {
    return STZScrollToZoomFlags;
}

void STZSetScrollToZoomFlags(STZFlags flags) {
    CFStringRef desc;
    STZScrollToZoomFlags = STZValidateFlags(flags, &desc);

    [[NSUserDefaults standardUserDefaults] setInteger:STZScrollToZoomFlags
                                               forKey:STZScrollToZoomFlagsKey];
    STZDebugLog("ScrollToZoomFlags set to %@", desc);
}


double STZGetScrollToZoomMagnifier(void) {
    return STZScrollToZoomMagnifier;
}

void STZSetScrollToZoomMagnifier(double magnifier) {
    STZScrollToZoomMagnifier = clamp(magnifier, -1, 1);

    [[NSUserDefaults standardUserDefaults] setDouble:STZScrollToZoomMagnifier
                                              forKey:STZScrollToZoomMagnifierKey];
    STZDebugLog("ScrollToZoomMagnifier set to %f", STZScrollToZoomMagnifier);
}


double STZGetScrollMomentumToZoomAttenuation(void) {
    return STZScrollMomentumToZoomAttenuation;
}

void STZSetScrollMomentumToZoomAttenuation(double attenuation) {
    STZScrollMomentumToZoomAttenuation = clamp(attenuation, 0, 1);

    [[NSUserDefaults standardUserDefaults] setDouble:STZScrollMomentumToZoomAttenuation
                                              forKey:STZScrollMomentumToZoomAttenuationKey];
    STZDebugLog("ScrollMomentumToZoomAttenuation set to %f", attenuation);
}


double STZGetScrollMinMomentumMagnification(void) {
    return STZScrollMinMomentumMagnification;
}

void STZSetScrollMinMomentumMagnification(double minMagnification) {
    STZScrollMinMomentumMagnification = clamp(minMagnification, 0, 1);

    [[NSUserDefaults standardUserDefaults] setDouble:STZScrollMinMomentumMagnification
                                              forKey:STZScrollMinMomentumMagnificationKey];
    STZDebugLog("ScrollMinMomentumMagnification set to %f", minMagnification);
}


bool STZGetDotDashDragToZoomEnabled(void) {
    return !STZDisablesDotDashDragToZoom;
}

void STZSetDotDashDragToZoomEnabled(bool enable) {
    STZDisablesDotDashDragToZoom = !enable;

    [[NSUserDefaults standardUserDefaults] setBool:STZDisablesDotDashDragToZoom
                                            forKey:STZDisablesDotDashDragToZoomKey];
    STZDebugLog("STZDisablesDotDashDragToZoom set to %s", !enable ? "true (disabled)" : "false (enabled)");

    CFNotificationCenterPostNotification(CFNotificationCenterGetLocalCenter(),
                                         kSTZDotDashDragToZoomEnabledDidChangeNotificationName,
                                         NULL, NULL, true);
}

CFStringRef const kSTZDotDashDragToZoomEnabledDidChangeNotificationName = CFSTR("STZDotDashDragToZoomEnabledDidChangeNotification");


STZEventTapOptions STZGetEventTapOptionsForBundleIdentifier(CFStringRef bundleID) {
    if (!bundleID) {return 0;}
    if (!STZEventTapOptionsForApps) {return 0;}
    return (STZEventTapOptions)(uintptr_t)CFDictionaryGetValue(STZEventTapOptionsForApps, bundleID);
}

void STZSetEventTapOptionsForBundleIdentifier(CFStringRef bundleID, STZEventTapOptions options) {
    if (!STZEventTapOptionsForApps) {return;}

    if (options == (uintptr_t)CFDictionaryGetValue(STZEventTapOptionsForApps, bundleID)) {
        return;
    }

    if (!options) {
        CFDictionaryRemoveValue(STZEventTapOptionsForApps, bundleID);
        CFDictionaryRemoveValue(STZEventTapOptionsObjsForApps, bundleID);

    } else {
        CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &options);
        CFDictionarySetValue(STZEventTapOptionsForApps, bundleID, (void *)(uintptr_t)options);
        CFDictionarySetValue(STZEventTapOptionsObjsForApps, bundleID, number);
        CFRelease(number);
    }

    [[NSUserDefaults standardUserDefaults] setObject:(__bridge id)STZEventTapOptionsObjsForApps
                                              forKey:STZEventTapOptionsForAppsKey];
    STZDebugLog("EventTapOptions set to %u for %@", options, bundleID);

    CFDictionaryRef userInfo = (__bridge void *)@{@"bundleIdentifier": (__bridge id)bundleID};
    CFNotificationCenterPostNotification(CFNotificationCenterGetLocalCenter(),
                                         kSTZEventTapOptionsForBundleIdentifierDidChangeNotificationName,
                                         NULL, userInfo, true);
}

CFStringRef const kSTZEventTapOptionsForBundleIdentifierDidChangeNotificationName = CFSTR("STZEventTapOptionsForBundleIdentifierDidChangeNotification");


CFDictionaryRef STZCopyAllEventTapOptions(void) {
    return CFDictionaryCreateCopy(kCFAllocatorDefault, STZEventTapOptionsForApps);
}


STZEventTapOptions STZGetRecommendedEventTapOptionsForBundleIdentifier(CFStringRef bundleID) {
    size_t count = sizeof(STZRecommendedEventTapOptions) / sizeof(*STZRecommendedEventTapOptions);
    for (size_t i = 0; i < count; ++i) {
        if (CFEqual(STZRecommendedEventTapOptions[i].bundleID, bundleID)) {
            return STZRecommendedEventTapOptions[i].options;
        }
    }
    return 0;
}


void STZLoadArgumentsFromUserDefaults(void) {
    NSUserDefaults *userDefaults = [NSUserDefaults standardUserDefaults];

    NSInteger flags = [userDefaults integerForKey:STZScrollToZoomFlagsKey];
    if (flags != 0) {
        STZScrollToZoomFlags = STZValidateFlags(STZScrollToZoomFlags, NULL);
    }

    NSNumber *magnifier = [userDefaults objectForKey:STZScrollToZoomMagnifierKey];
    if (magnifier && [magnifier isKindOfClass:[NSNumber self]]) {
        STZScrollToZoomMagnifier = clamp([magnifier doubleValue], -1, 1);
    }

    NSNumber *attenuation = [userDefaults objectForKey:STZScrollMomentumToZoomAttenuationKey];
    if (attenuation && [attenuation isKindOfClass:[NSNumber self]]) {
        STZScrollMomentumToZoomAttenuation = clamp([attenuation doubleValue], 0, 1);
    }

    NSNumber *minMomentum = [userDefaults objectForKey:STZScrollMinMomentumMagnificationKey];
    if (minMomentum && [minMomentum isKindOfClass:[NSNumber self]]) {
        STZScrollMinMomentumMagnification = clamp([minMomentum doubleValue], 0, 1);
    }

    STZDisablesDotDashDragToZoom = [userDefaults boolForKey:STZDisablesDotDashDragToZoomKey];

    if (STZEventTapOptionsForApps) {
        CFDictionaryRemoveAllValues(STZEventTapOptionsForApps);
        CFDictionaryRemoveAllValues(STZEventTapOptionsObjsForApps);
    } else {
        STZEventTapOptionsForApps = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, NULL);
        STZEventTapOptionsObjsForApps = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    }

    NSDictionary *eventTapOptions = [userDefaults objectForKey:STZEventTapOptionsForAppsKey];
    if (eventTapOptions) {
        for (NSString *key in eventTapOptions) {
            NSNumber *value = [eventTapOptions objectForKey:key];
            if ([value isKindOfClass:[NSNumber self]]) {
                CFDictionarySetValue(STZEventTapOptionsForApps, (__bridge void *)key, (void *)(uintptr_t)[value intValue]);
                CFDictionarySetValue(STZEventTapOptionsObjsForApps, (__bridge void *)key, (__bridge void *)value);
            }
        }

    } else {
        size_t count = sizeof(STZRecommendedEventTapOptions) / sizeof(*STZRecommendedEventTapOptions);
        for (size_t i = 0; i < count; ++i) {
            CFStringRef bundleID = STZRecommendedEventTapOptions[i].bundleID;
            if (!STZGetInstalledURLForBundleIdentifier(bundleID)) {continue;}

            STZEventTapOptions options = STZRecommendedEventTapOptions[i].options;

            CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &options);
            CFDictionarySetValue(STZEventTapOptionsForApps, bundleID, (void *)(uintptr_t)options);
            CFDictionarySetValue(STZEventTapOptionsObjsForApps, bundleID, number);
            CFRelease(number);
        }
    }

    STZDebugLog("Arguments loaded from user defaults:");

    CFStringRef desc;
    STZValidateFlags(STZScrollToZoomFlags, &desc);
    STZDebugLog("\tScrollToZoomFlags set to %@", desc);
    STZDebugLog("\tScrollToZoomMagnifier set to %f", STZScrollToZoomMagnifier);
    STZDebugLog("\tScrollMomentumToZoomAttenuation set to %f", STZScrollMomentumToZoomAttenuation);
    STZDebugLog("\tScrollMinMomentumMagnification set to %f", STZScrollMinMomentumMagnification);
}
