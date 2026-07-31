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


STZModes const kSTZModesAll = kSTZMagicZoomEnabled | kSTZTriggerFlagsEnabled | kSTZWantsDictatorship | kSTZRevertsToScrollImmediately;
STZModes const kSTZModesDefault = kSTZMagicZoomEnabled | kSTZTriggerFlagsEnabled | kSTZWantsDictatorship;

STZModes STZPreferredModes = kSTZModesDefault;
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

static NSString *const STZAppOptionsVersionKey = @"STZAppOptionsVersion";
enum {
    kSTZAppOptionsVersionInitial = 0,
    kSTZAppOptionsVersionWithChromiumZoomFixes,
    kSTZAppOptionsVersionLatest = kSTZAppOptionsVersionWithChromiumZoomFixes,
};


CFDictionaryRef STZDefaultOptionsForApps = NULL;

static struct {
    CFStringRef     bundleID;
    STZAppOptions   options;
} STZDefaultAppOptionsList[] = {
    {CFSTR("org.mozilla.firefox"), kSTZFlagsExcludedForApp},

    //  There’re plenty of Chromium-based apps; we can’t list all of them.
    //  Most Electron apps don’t support zooming.
    {CFSTR("org.chromium.Chromium"), kSTZFixesZoomForChromiumApp},
    {CFSTR("org.chromium.Thorium"), kSTZFixesZoomForChromiumApp},
    {CFSTR("com.google.Chrome"), kSTZFixesZoomForChromiumApp},
    {CFSTR("com.google.Chrome.beta"), kSTZFixesZoomForChromiumApp},
    {CFSTR("com.google.Chrome.canary"), kSTZFixesZoomForChromiumApp},
    {CFSTR("com.operasoftware.Opera"), kSTZFixesZoomForChromiumApp},
    {CFSTR("com.microsoft.edgemac"), kSTZFixesZoomForChromiumApp},
    {CFSTR("com.microsoft.edgemac.Beta"), kSTZFixesZoomForChromiumApp},
    {CFSTR("com.microsoft.edgemac.Canary"), kSTZFixesZoomForChromiumApp},
    {CFSTR("com.microsoft.edgemac.Dev"), kSTZFixesZoomForChromiumApp},
    {CFSTR("com.operasoftware.OperaNext"), kSTZFixesZoomForChromiumApp},
    {CFSTR("com.operasoftware.OperaDeveloper"), kSTZFixesZoomForChromiumApp},
    {CFSTR("com.operasoftware.OperaGX"), kSTZFixesZoomForChromiumApp},
    {CFSTR("com.vivaldi.Vivaldi"), kSTZFixesZoomForChromiumApp},
    {CFSTR("com.vivaldi.Vivaldi.snapshot"), kSTZFixesZoomForChromiumApp},
    {CFSTR("company.thebrowser.Browser"), kSTZFixesZoomForChromiumApp},
    {CFSTR("company.thebrowser.dia"), kSTZFixesZoomForChromiumApp},
    {CFSTR("com.brave.Browser"), kSTZFixesZoomForChromiumApp},
    {CFSTR("ai.perplexity.comet"), kSTZFixesZoomForChromiumApp},
};


static double clamp(double x, double lo, double hi) {
    //  `NaN` gives average of `lo` and `hi`;
    if (x != x) {return (lo + hi) / 2;}
    return x < lo ? lo : x > hi ? hi : x;
}


static void _loadUserDefaultsIfNeeded(void) {
    static bool loaded = false;
    if (loaded) {return;}

    NSUserDefaults *userDefaults = [NSUserDefaults standardUserDefaults];

    NSNumber *modes = [userDefaults objectForKey:STZModesKey];
    if (modes && [modes isKindOfClass:[NSNumber self]]) {
        STZPreferredModes = [modes integerValue] & kSTZModesAll;
    } else if ([userDefaults boolForKey:STZLegacyDisablesMagicZoomKey]) {
        [userDefaults removeObjectForKey:STZLegacyDisablesMagicZoomKey];
        STZPreferredModes = kSTZModesDefault & ~kSTZMagicZoomEnabled;
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

    if (!STZDefaultOptionsForApps) {
        size_t count = sizeof(STZDefaultAppOptionsList) / sizeof(*STZDefaultAppOptionsList);
        CFMutableDictionaryRef dict = CFDictionaryCreateMutable(kCFAllocatorDefault, count, &kCFTypeDictionaryKeyCallBacks, NULL);
        for (size_t i = 0; i < count; ++i) {
            CFStringRef bundleID = STZDefaultAppOptionsList[i].bundleID;
            STZAppOptions options = STZDefaultAppOptionsList[i].options;
            CFDictionarySetValue(dict, bundleID, (void *)(uintptr_t)options);
        }
        STZDefaultOptionsForApps = dict;
    }

    if (STZOptionsForApps) {
        CFDictionaryRemoveAllValues(STZOptionsForApps);
        CFDictionaryRemoveAllValues(STZOptionsObjsForApps);
    } else {
        STZOptionsForApps = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, NULL);
        STZOptionsObjsForApps = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    }
    NSDictionary *appOptions = [userDefaults objectForKey:STZOptionsForAppsKey];
    if ([appOptions isKindOfClass:[NSDictionary self]]) {
        NSInteger version = [userDefaults integerForKey:STZAppOptionsVersionKey];

        for (NSString *key in appOptions) {
            NSNumber *number = [appOptions objectForKey:key];
            if (![number isKindOfClass:[NSNumber self]]) {continue;}

            NSInteger value = [number integerValue];
            uintptr_t defaultValue = (uintptr_t)CFDictionaryGetValue(STZDefaultOptionsForApps, (__bridge void *)key);

            if (version < kSTZAppOptionsVersionWithChromiumZoomFixes && (defaultValue & kSTZFixesZoomForChromiumApp)) {
                value |= kSTZFixesZoomForChromiumApp;
                number = [NSNumber numberWithInteger:value];
            }

            if (value == defaultValue) {continue;}
            CFDictionarySetValue(STZOptionsForApps, (__bridge void *)key, (void *)value);
            CFDictionarySetValue(STZOptionsObjsForApps, (__bridge void *)key, (__bridge void *)number);
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

    void const *value;
    if (!CFDictionaryGetValueIfPresent(STZOptionsForApps, bundleID, &value)) {
        value = CFDictionaryGetValue(STZDefaultOptionsForApps, bundleID);
    }
    return (STZAppOptions)(uintptr_t)value;
}

void STZSetAppOptionsForBundleIdentifier(CFStringRef bundleID, STZAppOptions options) {
    _loadUserDefaultsIfNeeded();

    void const *value;
    if (CFDictionaryGetValueIfPresent(STZOptionsForApps, bundleID, &value)
     && (options == (uintptr_t)value)) {return;}

    if (options == (uintptr_t)CFDictionaryGetValue(STZDefaultOptionsForApps, bundleID)) {
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
    [[NSUserDefaults standardUserDefaults] setInteger:kSTZAppOptionsVersionLatest
                                               forKey:STZAppOptionsVersionKey];

    CFDictionaryRef userInfo = (__bridge void *)@{@"bundleIdentifier": (__bridge id)bundleID};
    CFNotificationCenterPostNotification(CFNotificationCenterGetLocalCenter(),
                                         kSTZAppOptionsDidChangeNotification,
                                         NULL, userInfo, true);
}

CFStringRef const kSTZAppOptionsDidChangeNotification = CFSTR("STZAppOptionsDidChangeNotification");


CFDictionaryRef STZCopyOptionsForAllApps(void) {
    _loadUserDefaultsIfNeeded();
    return CFDictionaryCreateCopy(kCFAllocatorDefault, STZOptionsForApps);
}


STZAppOptions STZGetRecommendedAppOptionsForBundleIdentifier(CFStringRef bundleID) {
    return (STZAppOptions)(uintptr_t)CFDictionaryGetValue(STZDefaultOptionsForApps, bundleID);
}
