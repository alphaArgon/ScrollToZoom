/*
 *  STZEventCommon.h
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


typedef CLOSED_ENUM: int8_t {
    kSTZMaybe               = -1,
    __STZTrivalent_false    = false,
    __STZTrivalent_true     = true,
} STZTrivalent;


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


/// Returns a unique identifier for the sender of the event, or 0 if not available.
uint64_t STZSenderIDForEvent(CGEventRef event);


void STZDebugLogEvent(char const *prefix, CGEventRef event);


/// Whether natural scrolling is enabled for this event.
bool STZIsScrollWheelFlipped(CGEventRef);


/// Converts the phase of the scroll wheel event to the unified phase.
///
/// A scroll wheel event may have two periods: the scroll phase and the momentum phase. The momentum
/// phase is optional but exclusive to the scroll phase. In this case, `outByMomentum` will be set
/// to `true` and the returned value will reflect the momentum phase.
void STZGetPhaseFromScrollWheelEvent(CGEventRef event, STZPhase *outPhase, bool *outByMomentum);
void STZGetPhaseFromGestureEvent(CGEventRef event, STZPhase *outPhase);


/// Enforces the event to have the given phase.
void STZAdaptScrollWheelEvent(CGEventRef event, STZPhase phase, bool byMomentum);
void STZAdaptGestureEvent(CGEventRef event, STZPhase phase, double scale);


CF_RETURNS_RETAINED CGEventRef STZCreateScrollWheelEvent(CGEventRef sample);
CF_RETURNS_RETAINED CGEventRef STZCreateZoomGestureEvent(CGEventRef sample);


CF_ASSUME_NONNULL_END
CF_IMPLICIT_BRIDGING_DISABLED
