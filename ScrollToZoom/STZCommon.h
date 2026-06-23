/*
 *  STZCommon.h
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/4/24.
 *  Copyright © 2025 alphaArgon.
 */

#pragma once
#include <CoreGraphics/CGEvent.h>

CF_IMPLICIT_BRIDGING_ENABLED
CF_ASSUME_NONNULL_BEGIN


#define CLOSED_ENUM(ScalarType) enum __attribute__((enum_extensibility(closed))): ScalarType
#define OPTION_FLAGS(ScalarType) enum __attribute__((flag_enum,enum_extensibility(open))): ScalarType


static inline CGEventTimestamp CGEventTimestampNow(void) {
    return clock_gettime_nsec_np(CLOCK_UPTIME_RAW_APPROX);
}


typedef OPTION_FLAGS(uint32_t) {
    //  Modifier flags, can be combined as an option set.
    kSTZModifierShift       = NX_SHIFTMASK,
    kSTZModifierControl     = NX_CONTROLMASK,
    kSTZModifierOption      = NX_ALTERNATEMASK,
    kSTZModifierCommand     = NX_COMMANDMASK,
    kSTZModifierFn          = NX_SECONDARYFNMASK,
    kSTZModifiersMask       = NX_SHIFTMASK | NX_CONTROLMASK | NX_ALTERNATEMASK | NX_COMMANDMASK | NX_SECONDARYFNMASK,
    kSTZPrintableModifiersMask = kSTZModifiersMask | NX_ALPHASHIFTMASK,

    //  Mouse buttons, exclusive each other and with modifier flags.
    kSTZMouseButtonMiddle   = 2,
    kSTZMouseButtonFourth   = 3,
    kSTZMouseButtonFifth    = 4,
    kSTZMouseButtonSixth    = 5,
    kSTZMouseButtonSeventh  = 6,
    kSTZMouseButtonEighth   = 7,
    kSTZMouseButtonsMask    = 0b111,
} STZFlags;


STZFlags STZFlagsValidate(uint32_t dirtyFlags);
CFStringRef STZFlagsCopyDescription(uint32_t anyFlags);


typedef struct _STZCache *STZCacheRef;

STZCacheRef STZCacheCreate(size_t valueSize, CGEventTimestamp valueLifetime, void (*__nullable valueDisposeCallback)(void *valueAddr));
void STZCacheRelease(STZCacheRef);

/// Returns the address of the value. The value is valid until a mutation happens to the cache.
void *__nullable STZCacheGetValue(STZCacheRef, uint64_t key);
void *STZCacheSetValue(STZCacheRef, uint64_t key, void const *valueAddr);

void *__nullable STZCacheGetRecentValue(STZCacheRef, uint64_t *__nullable outKey);

void STZCacheRemoveAll(STZCacheRef);
void STZCacheEnumerateValues(STZCacheRef, void (*valueEnumerateCallback)(void *valueAddr, void *__nullable context), void *__nullable context);


bool STZIsLoggingEnabled(void);
void STZDebugLog(char const *message, ...) CF_FORMAT_FUNCTION(1, 2);

void STZUnknownEnumCase(char const *type, int64_t value);
void STZDebugLogEvent(char const *prefix, CGEventRef event);


CF_ASSUME_NONNULL_END
CF_IMPLICIT_BRIDGING_DISABLED
