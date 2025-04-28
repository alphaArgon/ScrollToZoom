/*
 *  STZWheelSession.h
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/4/16.
 *  Copyright © 2025 alphaArgon.
 */

#import <CoreGraphics/CGEvent.h>
#import "STZCommon.h"

CF_IMPLICIT_BRIDGING_ENABLED
CF_ASSUME_NONNULL_BEGIN


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


typedef struct {
    STZWheelState       state;
    STZWheelType        type;  //  Valid only if `state` is not `kWheelFree`.
} STZWheelSession;

#define kSTZWheelSessionEmpty (STZWheelSession){kSTZWheelFree, kSTZWheelToScroll}


void STZDebugLogSessionChange(char const *prefix, STZWheelSession from, STZWheelSession to);


/// Returns the sign of the scroll wheel delta.
int STZGetDeltaSignumForScrollWheelEvent(CGEventRef event);


/// Set the user data field of the event to the sign of the scroll wheel delta if it’s not used.
bool STZMarkDeltaSignumForEvent(CGEventRef event, int signum);
bool STZConsumeDeltaSignumForEvent(CGEventRef event, int *signum);


/// Whether the event is followed by other event events. This value is set to `maybe` if the wheel
/// is non-continuous, or the event ends a smooth scroll phase (which may start a momentum phase).
STZTrivalent STZScrollWheelHasSuccessorEvent(STZPhase phase, bool byMomentum);


/// Calculates the simulated zoom gesture phase and magnification from the scroll wheel event.
///
/// The `momentumStart` is an accumulated in-out parameter used for calculating the magnification
/// attenuation during the momentum changing. If the given event begins a momentum phase, this
/// parameter will be updated to the event timestamp; otherwise this parameter is read-only, and the
/// caller is responsible for setting it to the timestamp of the most recent event that began a
/// momentum phase, which was typically informed by this function in the former case.
void STZConvertZoomFromScrollWheel(STZPhase *phase, bool byMomentum, int signum,  double delta,
                                   CGEventTimestamp now, CGEventTimestamp *momentumStart,
                                   double *outScale);


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
///
/// The `endEnds` indicates whether the event is a terminating one, that is, whose phase is ended,
/// cancelled, or none, and trivially ends the session.
void STZWheelSessionUpdate(STZWheelSession *session, STZWheelType type, STZPhase phase, double data,
                           CGEventRef wheelEvent, CGEventRef __nonnull outEvents[__nullable CF_RETURNS_RETAINED 2],
                           STZEventAction *action);


void STZWheelSessionAssign(STZWheelSession *session, STZWheelType type, STZPhase phase);

bool STZWheelSessionIsEnded(STZWheelSession *const session, STZPhase byEventPhase);


CF_ASSUME_NONNULL_END
CF_IMPLICIT_BRIDGING_DISABLED
