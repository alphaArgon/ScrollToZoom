/*
 *  STZWheelSession.h
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/4/16.
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


typedef CLOSED_ENUM: uint8_t {
    kSTZEventUnchanged,
    kSTZEventAdapted,
    kSTZEventReplaced,      //  The event should be a sequence of events.
} STZEventAction;


typedef CLOSED_ENUM: uint8_t {
    kSTZWheelToScroll,
    kSTZWheelToScrollMomentum,
    kSTZWheelToZoom,
} STZWheelType;


typedef CLOSED_ENUM: uint8_t {
    kSTZWheelFree,          //  Not in a session
    kSTZWheelWillBegin,     //  `kSTZPhaseMayBegin` sent
    kSTZWheelDidBegin,      //  `kSTZPhaseBegan` sent
} STZWheelState;


#undef CLOSED_ENUM


typedef struct {
    STZWheelState       state;
    STZWheelType        type;  //  Valid only if `state` is not `kWheelFree`.
} STZWheelSession;

#define kSTZWheelSessionEmpty (STZWheelSession){kSTZWheelFree, kSTZWheelToScroll}


/// Returns a unique identifier for the sender of the event, or 0 if not available.
uint64_t STZSenderIDForEvent(CGEventRef event);


void STZDebugLogEvent(char const *prefix, CGEventRef event);
void STZDebugLogSessionChange(char const *prefix, STZWheelSession from, STZWheelSession to);


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


/// Set the user data field of the event to the sign of the scroll wheel delta if it’s not used.
int STZMarkDeltaSignumForScrollWheelEvent(CGEventRef event);


/// Calculates the simulated zoom gesture phase and magnification from the scroll wheel event.
///
/// The `momentumStart` is an accumulated in-out parameter used for calculating the magnification
/// attenuation during the momentum changing. If the given event begins a momentum phase, this
/// parameter will be updated to the event timestamp; otherwise this parameter is read-only, and the
/// caller is responsible for setting it to the timestamp of the most recent event that began a
/// momentum phase, which was typically informed by this function in the former case.
///
/// The `outEventHasSuccessor` is a trinary-logic value indicates whether the event is followed by
/// other event events. This value is set to `maybe` if the wheel is non-continuous, or the event
/// ends a smooth scroll phase (which may start a smooth momentum phase).
///
/// If the event is previously marked with a signum, the marked value will be used, rather than the
/// passed value. the signum is used to override the sign of the magnificant.
void STZConvertPhaseFromScrollWheelEvent(CGEventRef event, int suggestedSigum, bool *outAccepped,
                                         CGEventTimestamp *momentumStart,
                                         STZPhase *outPhase, double *outScale,
                                         STZTrivalent *outEventHasSuccessor);


/// Creates a scroll wheel event to discard the current `session` if it’s active.
///
/// If `actionIfWheel` is provided, the event is assumed to be a scroll wheel event, and
/// modifications will be applied to it; otherwise, the new event is returned as `outEvent` if
/// needed, leaving the `byEvent` untouched.
void STZWheelSessionDiscard(STZWheelSession *session, CGEventRef byEvent,
                            CGEventRef __nullable CF_RETURNS_RETAINED *__nonnull outEvent,
                            STZEventAction *__nullable actionIfWheel);


/// Updates the scroll wheel session to match the given type and phase.
///
/// The `session` parameter is an in-out parameter. The caller is responsible for setting it to the
/// current session, and the function will update it to the new state after emitting the event.
///
/// The `outEvents` should have at least the capacity of two and be initially filled with `NULL`.
/// When `action` is set to `kEventAdapted`, new events will be filled sequentially in the array. If
/// no events is filled, the original event should be discarded.
void STZWheelSessionUpdate(STZWheelSession *session, STZWheelType type, STZPhase phase, double data,
                           CGEventRef wheelEvent, CGEventRef __nonnull outEvents[__nullable CF_RETURNS_RETAINED 2],
                           STZEventAction *action);


void STZWheelSessionAssign(STZWheelSession *session, STZWheelType type, STZPhase phase);


CF_ASSUME_NONNULL_END
CF_IMPLICIT_BRIDGING_DISABLED
