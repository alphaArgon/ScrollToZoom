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


STZModes const kSTZModesAll = kSTZMagicZoomEnabled | kSTZTriggerFlagsEnabled | kSTZWantsDictatorship;


STZModes STZPreferredModes = kSTZModesAll;
STZFlags STZTriggerFlags = kSTZModifierOption;
double STZMagnificationScalar = 0.0025;
double STZMomentumZoomAttenuation = 0.8;
double STZScrollMomentumZoomMinValue = 0.001;
CFMutableDictionaryRef STZOptionsForApps = NULL;
CFMutableDictionaryRef STZOptionsObjsForApps = NULL;


static NSString *const STZModesKey = @"STZModeFlags";
static NSString *const STZTriggerFlagsKey = @"STZScrollToZoomFlags";
static NSString *const STZMagnificationScalarKey = @"STZScrollToZoomMagnifier";
static NSString *const STZMomentumZoomAttenuationKey = @"STZScrollMomentumToZoomAttenuation";
static NSString *const STZScrollMomentumZoomMinValueKey = @"STZScrollMinMomentumMagnification";
static NSString *const STZOptionsForAppsKey = @"STZEventTapOptionsForApps";

static NSString *const STZLegacyDisablesMagicZoomKey = @"STZDisableDotDashDragToZoom";


static struct {
    CFStringRef     bundleID;
    STZAppOptions   options;
} STZRecommendedAppOptions[] = {
    {CFSTR("org.mozilla.firefox"), kSTZFlagsExcludedForApp}
};


static double clamp(double x, double lo, double hi) {
    //  `NaN` gives average of `lo` and `hi`;
    if (x != x) {return (lo + hi) / 2;}
    return x < lo ? lo : x > hi ? hi : x;
}


static void _loadUserDefaultsIfNeeded(void) {
    static bool loaded = false;
    if (loaded) {return;}

    //  Concurrent loading from multiple threads is OK.
    NSUserDefaults *userDefaults = [NSUserDefaults standardUserDefaults];

    NSNumber *modes = [userDefaults objectForKey:STZModesKey];
    if (modes && [modes isKindOfClass:[NSNumber self]]) {
        STZPreferredModes = [modes intValue] & kSTZModesAll;

    } else if ([userDefaults boolForKey:STZLegacyDisablesMagicZoomKey]) {
        [userDefaults removeObjectForKey:STZLegacyDisablesMagicZoomKey];
        STZPreferredModes = kSTZModesAll & ~kSTZMagicZoomEnabled;
    }

    NSInteger flags = [userDefaults integerForKey:STZTriggerFlagsKey];
    if (flags != 0) {
        STZTriggerFlags = STZFlagsValidate((uint32_t)flags);
    }

    NSNumber *magnifier = [userDefaults objectForKey:STZMagnificationScalarKey];
    if (magnifier && [magnifier isKindOfClass:[NSNumber self]]) {
        STZMagnificationScalar = clamp([magnifier doubleValue], -1, 1);
    }

    NSNumber *attenuation = [userDefaults objectForKey:STZMomentumZoomAttenuationKey];
    if (attenuation && [attenuation isKindOfClass:[NSNumber self]]) {
        STZMomentumZoomAttenuation = clamp([attenuation doubleValue], 0, 1);
    }

    NSNumber *minMomentum = [userDefaults objectForKey:STZScrollMomentumZoomMinValueKey];
    if (minMomentum && [minMomentum isKindOfClass:[NSNumber self]]) {
        STZScrollMomentumZoomMinValue = clamp([minMomentum doubleValue], 0, 1);
    }

    if (STZOptionsForApps) {
        CFDictionaryRemoveAllValues(STZOptionsForApps);
        CFDictionaryRemoveAllValues(STZOptionsObjsForApps);
    } else {
        STZOptionsForApps = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, NULL);
        STZOptionsObjsForApps = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    }

    NSDictionary *eventTapOptions = [userDefaults objectForKey:STZOptionsForAppsKey];
    if (eventTapOptions) {
        for (NSString *key in eventTapOptions) {
            NSNumber *value = [eventTapOptions objectForKey:key];
            if ([value isKindOfClass:[NSNumber self]]) {
                CFDictionarySetValue(STZOptionsForApps, (__bridge void *)key, (void *)(uintptr_t)[value intValue]);
                CFDictionarySetValue(STZOptionsObjsForApps, (__bridge void *)key, (__bridge void *)value);
            }
        }

    } else {
        size_t count = sizeof(STZRecommendedAppOptions) / sizeof(*STZRecommendedAppOptions);
        for (size_t i = 0; i < count; ++i) {
            CFStringRef bundleID = STZRecommendedAppOptions[i].bundleID;
            if (!STZGetInstalledURLForBundleIdentifier(bundleID)) {continue;}

            STZAppOptions options = STZRecommendedAppOptions[i].options;

            CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &options);
            CFDictionarySetValue(STZOptionsForApps, bundleID, (void *)(uintptr_t)options);
            CFDictionarySetValue(STZOptionsObjsForApps, bundleID, number);
            CFRelease(number);
        }
    }

    loaded = true;
}



STZModes STZGetPreferredModes(void) {
    _loadUserDefaultsIfNeeded();
    return STZPreferredModes;
}

void STZSetPreferredModes(STZModes modes) {
    STZPreferredModes = modes & kSTZModesAll;
    [[NSUserDefaults standardUserDefaults] setInteger:STZPreferredModes
                                               forKey:STZModesKey];
}


STZFlags STZGetTriggerFlags(void) {
    _loadUserDefaultsIfNeeded();
    return STZTriggerFlags;
}

void STZSetTriggerFlags(STZFlags flags) {
    STZTriggerFlags = STZFlagsValidate(flags);
    [[NSUserDefaults standardUserDefaults] setInteger:STZTriggerFlags
                                               forKey:STZTriggerFlagsKey];
}


double STZGetMagnificationScalar(void) {
    _loadUserDefaultsIfNeeded();
    return STZMagnificationScalar;
}

void STZSetMagnificationScalar(double magnifier) {
    STZMagnificationScalar = clamp(magnifier, -1, 1);
    [[NSUserDefaults standardUserDefaults] setDouble:STZMagnificationScalar
                                              forKey:STZMagnificationScalarKey];
}


double STZGetMomentumZoomAttenuation(void) {
    _loadUserDefaultsIfNeeded();
    return STZMomentumZoomAttenuation;
}

void STZSetMomentumZoomAttenuation(double attenuation) {
    STZMomentumZoomAttenuation = clamp(attenuation, 0, 1);
    [[NSUserDefaults standardUserDefaults] setDouble:STZMomentumZoomAttenuation
                                              forKey:STZMomentumZoomAttenuationKey];
}


double STZGetMomentumZoomMinValue(void) {
    _loadUserDefaultsIfNeeded();
    return STZScrollMomentumZoomMinValue;
}

void STZSetMomentumZoomMinValue(double minMagnification) {
    STZScrollMomentumZoomMinValue = clamp(minMagnification, 0, 1);
    [[NSUserDefaults standardUserDefaults] setDouble:STZScrollMomentumZoomMinValue
                                              forKey:STZScrollMomentumZoomMinValueKey];
}


STZAppOptions STZGetAppOptionsForBundleIdentifier(CFStringRef bundleID) {
    if (!bundleID) {return 0;}
    _loadUserDefaultsIfNeeded();
    return (STZAppOptions)(uintptr_t)CFDictionaryGetValue(STZOptionsForApps, bundleID);
}

void STZSetAppOptionsForBundleIdentifier(CFStringRef bundleID, STZAppOptions options) {
    _loadUserDefaultsIfNeeded();
    if (options == (uintptr_t)CFDictionaryGetValue(STZOptionsForApps, bundleID)) {return;}

    if (!options) {
        CFDictionaryRemoveValue(STZOptionsForApps, bundleID);
        CFDictionaryRemoveValue(STZOptionsObjsForApps, bundleID);

    } else {
        CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &options);
        CFDictionarySetValue(STZOptionsForApps, bundleID, (void *)(uintptr_t)options);
        CFDictionarySetValue(STZOptionsObjsForApps, bundleID, number);
        CFRelease(number);
    }

    [[NSUserDefaults standardUserDefaults] setObject:(__bridge id)STZOptionsObjsForApps
                                              forKey:STZOptionsForAppsKey];

    CFDictionaryRef userInfo = (__bridge void *)@{@"bundleIdentifier": (__bridge id)bundleID};
    CFNotificationCenterPostNotification(CFNotificationCenterGetLocalCenter(),
                                         kSTZAppOptionsDidChangeNotification,
                                         NULL, userInfo, true);
}

CFStringRef const kSTZAppOptionsDidChangeNotification = CFSTR("STZAppOptionsDidChangeNotification");


CFDictionaryRef STZCopyOptionsForAllApps(void) {
    return CFDictionaryCreateCopy(kCFAllocatorDefault, STZOptionsForApps);
}


STZAppOptions STZGetRecommendedAppOptionsForBundleIdentifier(CFStringRef bundleID) {
    size_t count = sizeof(STZRecommendedAppOptions) / sizeof(*STZRecommendedAppOptions);
    for (size_t i = 0; i < count; ++i) {
        if (CFEqual(STZRecommendedAppOptions[i].bundleID, bundleID)) {
            return STZRecommendedAppOptions[i].options;
        }
    }
    return 0;
}
