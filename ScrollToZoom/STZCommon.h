/*
 *  STZCommon.h
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/4/24.
 *  Copyright © 2025 alphaArgon.
 */

#import <CoreGraphics/CGEvent.h>
#import "STZHandlers.h"

CF_IMPLICIT_BRIDGING_ENABLED
CF_ASSUME_NONNULL_BEGIN


#define CLOSED_ENUM enum __attribute__((enum_extensibility(closed)))

#define FIELD_OFFSET(type, property) ((size_t)&((type *)NULL)->property)


typedef CLOSED_ENUM: int8_t {
    kSTZMaybe               = -1,
    __STZTrivalent_false    = false,
    __STZTrivalent_true     = true,
} STZTrivalent;


static inline CGEventTimestamp CGEventTimestampNow(void) {
    return clock_gettime_nsec_np(CLOCK_UPTIME_RAW_APPROX);
}


//  MARK: - STZCache


typedef struct {
    int                 count, hotIndex;
    CGEventTimestamp    dataLifetime;
    CGEventTimestamp    checkInterval;
    CGEventTimestamp    checkedAt;
    size_t              entrySize, dataOffset;
    void               *__nullable entries;
} STZCScanCache;

#define _STZCacheEntryType(DataType)                    \
struct {                                                \
    uint64_t            identifier;                     \
    CGEventTimestamp    accessedAt;                     \
    DataType            data;                           \
}                                                       \

#define kSTZCScanCacheEmptyForType(DataType)            \
(STZCScanCache){                                        \
    0, 0, 0, 0, 0,                                      \
    sizeof(_STZCacheEntryType(DataType)),               \
    FIELD_OFFSET(_STZCacheEntryType(DataType), data),   \
    NULL                                                \
}                                                       \

typedef CLOSED_ENUM: uint8_t {
    kSTZCScanCacheFound,
    kSTZCScanCacheNewCreated,
    kSTZCScanCacheExpiredReused,
    kSTZCScanCacheExpiredRestored,
} STZCScanCacheResult;


/// Whether the cache is once accessed.
bool STZCScanCacheIsInUse(STZCScanCache *);

/// Sets the parameter of expiration checking parameters. If you pass a zero `dataLifetime` to the
/// function, which are the default values, the cache are never cleaned.
void STZCScanCacheSetDataLifetime(STZCScanCache *, CGEventTimestamp dataLifetime, CGEventTimestamp autoCheckInterval);

/// Marks expired data. Expired data won’t be cleaned immediately, but might be overwritten to
/// prevent reallocating memory, in which case the `outResult` of `STZCScanCachedDataForIdentifier`
/// will be set to `kSTZCScanCacheExpiredReused`.
void STZCScanCacheCheckExpired(STZCScanCache *, bool forceCheck);

/// Returns the pointer to the data of the given identifier. If the identifier is not found, a new
/// entry will be created if `createIfNeeded`, or `NULL` will be returned. `outResult` reflects how
/// the entry is created.
void *__nullable STZCScanCacheGetDataForIdentifier(STZCScanCache *, uint64_t identifier, bool createIfNeeded, STZCScanCacheResult *__nullable outResult);

/// Removes all data and release the memory.
void STZCScanCacheRemoveAll(STZCScanCache *);


typedef struct {
    int                 index;
    bool                includeExpired;
    STZCScanCache      *cache;
    CGEventTimestamp    now;
} STZCacheIterator;

void STZCScanCacheIteratorInitialize(STZCacheIterator *, STZCScanCache *, bool includeExpired);
void *__nullable STZCScanCacheIteratorGetNextData(STZCacheIterator *, uint64_t *__nullable outIdentifier, bool *__nullable outExpired);


//  MARK: - CGEvent


typedef CLOSED_ENUM: uint8_t {
    kSTZPhaseNone,
    kSTZPhaseMayBegin,
    kSTZPhaseBegan,
    kSTZPhaseChanged,
    kSTZPhaseEnded,
    kSTZPhaseCancelled,
} STZPhase;


typedef enum __attribute__((flag_enum, enum_extensibility(open))): uint32_t {
    //  Modifier flags, can be combined as an option set.
    kSTZModifierShift       = NX_SHIFTMASK,
    kSTZModifierControl     = NX_CONTROLMASK,
    kSTZModifierOption      = NX_ALTERNATEMASK,
    kSTZModifierCommand     = NX_COMMANDMASK,
    kSTZModifiersMask       = NX_SHIFTMASK | NX_CONTROLMASK | NX_ALTERNATEMASK | NX_COMMANDMASK,

    //  Mouse buttons, exclusive each other and with modifier flags.
    kSTZMouseButtonMiddle   = 2,
    kSTZMouseButtonFourth   = 3,
    kSTZMouseButtonFifth    = 4,
    kSTZMouseButtonSixth    = 5,
    kSTZMouseButtonSeventh  = 6,
    kSTZMouseButtonEighth   = 7,
    kSTZMouseButtonsMask    = 0b111,
} STZFlags;


/// Returns a valid set of flags by extracting the given value. If `getDescription` is provided, a
/// textual representation of the valid flags will be indirectly returned. This description is not
/// retained.
STZFlags STZValidateFlags(uint32_t dirtyFlags, CFStringRef __nonnull CF_RETURNS_NOT_RETAINED *__nullable outDescription);


void STZDebugLogEvent(char const *prefix, CGEventRef event);


/// Whether natural scrolling is enabled for this event.
bool STZIsScrollWheelFlipped(CGEventRef);


/// Converts the phase of the scroll wheel event to the unified phase.
///
/// A scroll wheel event may have two periods: the scroll phase and the momentum phase. The momentum
/// phase is optional but exclusive to the scroll phase. In this case, `outByMomentum` will be set
/// to `true` and the returned value will reflect the momentum phase.
STZPhase STZGetPhaseFromScrollWheelEvent(CGEventRef event, bool *outByMomentum);
STZPhase STZGetPhaseFromGestureEvent(CGEventRef event);


/// Enforces the event to have the given phase.
void STZAdaptScrollWheelEvent(CGEventRef event, STZPhase phase, bool byMomentum);
void STZAdaptGestureEvent(CGEventRef event, STZPhase phase, double scale);


CF_RETURNS_RETAINED CGEventRef STZCreateScrollWheelEvent(CGEventRef sample);
CF_RETURNS_RETAINED CGEventRef STZCreateZoomGestureEvent(CGEventRef sample);


CF_ASSUME_NONNULL_END
CF_IMPLICIT_BRIDGING_DISABLED
