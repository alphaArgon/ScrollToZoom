/*
 *  STZStateManager.h
 *  ScrollToZoom
 *
 *  Created by alpha on 2026/5/28.
 *  Copyright © 2026 alphaArgon.
 */

#pragma once
#include <CoreGraphics/CGEvent.h>
#include "STZCommon.h"

CF_IMPLICIT_BRIDGING_ENABLED
CF_ASSUME_NONNULL_BEGIN


typedef CLOSED_ENUM(uint8_t) {
    kSTZReplaceEvent,
    kSTZPrependEvent,
    kSTZAppendEvent,
} STZEventPlacement;


typedef enum {
    kSTZScroll,
    kSTZZoom,
} STZGestureType;


/// Returns a non-zero value stashed into the event, which can be passed as `fallback` to
/// `STZReadScrollDeltaFromEvent` if the stash is lost unexpectedly.
uint64_t STZStashScrollDirectionIntoEvent(CGEventRef);
double STZReadScrollDeltaFromEvent(CGEventRef event, uint64_t fallback, bool unstash);


typedef struct _STZState *STZStateRef;

STZStateRef STZStateCreate(void);
void STZStateRelease(STZStateRef);

/// Session data are an arbitrary 64-bit value that is preserved during a wheel session and
/// automatically reset to zero when the session ends. Returns whether the state is in a session.
bool STZStateGetSessionData(STZStateRef, uint64_t *outData);

/// Forces the state to be synchronized with the scroll event. The caller should inspect
/// `STZStateCanStopTransformingEvents` to determine whether forced synchronization is safe.
void STZStateReadScrollEvent(STZStateRef, CGEventRef event);

/// Optionally takes a session data value that will be associated with the session after the call.
/// However, if the session will end after the call, this value will be ignored.
CGEventRef __nullable STZStateTransformScrollEvent(STZStateRef, CGEventRef event, STZGestureType gesture,
                                                   uint64_t fallbackScrollDir, uint64_t const *sessionData,
                                                   STZEventPlacement *returnEventPlacement) CF_RETURNS_RETAINED;
CGEventRef __nullable STZStateRevertToScrollByEvent(STZStateRef, CGEventRef event) CF_RETURNS_RETAINED;
CGEventRef __nullable STZStatePeriodicallyUpdate(STZStateRef, CGEventTimestamp now) CF_RETURNS_RETAINED;

CGEventTimestamp STZStateGetNextUpdatePeriod(STZStateRef, CGEventTimestamp now);
bool STZStateCanStopTransformingEvents(STZStateRef);


CF_ASSUME_NONNULL_END
CF_IMPLICIT_BRIDGING_DISABLED
