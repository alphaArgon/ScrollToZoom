/*
 *  STZStateManager.c
 *  ScrollToZoom
 *
 *  Created by alpha on 2026/5/28.
 *  Copyright © 2026 alphaArgon.
 */

#include "STZStateManager.h"
#include "STZSettings.h"
#include "CGEventSPI.h"


typedef struct {
    CGEventRef          otherEvent;
    STZEventPlacement   otherPlacement;
} EventResult;


static inline EventResult discardEvent(void) {
    return (EventResult){NULL, kSTZReplaceEvent};
}

static inline EventResult keepEvent(void) {
    return (EventResult){NULL, kSTZAppendEvent};
}

static inline EventResult replaceEvent(CGEventRef event) {
    return (EventResult){event, kSTZReplaceEvent};
}

static inline EventResult appendEvent(CGEventRef event) {
    return (EventResult){event, kSTZAppendEvent};
}

static inline EventResult prependEvent(CGEventRef event) {
    return (EventResult){event, kSTZPrependEvent};
}


static const CGEventTimestamp kCGEventDistantFuture = UINT64_MAX;


static int64_t const kPositiveSignum   = ((int64_t)'STZ.' << 32) | 'SIG+';
static int64_t const kNegativeSignum   = ((int64_t)'STZ.' << 32) | 'SIG-';
static int64_t const kZeroSignum       = ((int64_t)'STZ.' << 32) | 'SIG0';
static int32_t const kSignumField      = kCGEventSourceUserData;


static bool isScrollFlipped(CGEventRef event) {
    return CGEventGetIntegerValueField(event, kCGScrollEventIsDirectionInverted) != 0;
}


static double primaryScrollDelta(CGEventRef event) {
    double data = CGEventGetIntegerValueField(event, kCGScrollWheelEventPointDeltaAxis1);
    if (data == 0) {
        data = CGEventGetDoubleValueField(event, kCGScrollWheelEventFixedPtDeltaAxis1);
    }
    return data;
}


uint64_t STZStashScrollDirectionIntoEvent(CGEventRef event) {
    uint64_t payload = CGEventGetIntegerValueField(event, kSignumField);
    if (payload != 0) {return payload;}

    double delta = primaryScrollDelta(event) * (isScrollFlipped(event) ? -1 : 1);
    payload = (delta > 0) ? kPositiveSignum : (delta < 0) ? kNegativeSignum : kZeroSignum;
    CGEventSetIntegerValueField(event, kSignumField, payload);
    return payload;
}


double STZReadScrollDeltaFromEvent(CGEventRef event, uint64_t fallback, bool unstash) {
    switch (CGEventGetIntegerValueField(event, kSignumField)) {
    case kPositiveSignum:
        if (unstash) {
            CGEventSetIntegerValueField(event, kSignumField, 0);
        }
        return fabs(primaryScrollDelta(event));
    case kNegativeSignum:
        if (unstash) {
            CGEventSetIntegerValueField(event, kSignumField, 0);
        }
        return -fabs(primaryScrollDelta(event));
    case kZeroSignum:
        if (unstash) {
            CGEventSetIntegerValueField(event, kSignumField, 0);
        }
        return 0;
    default:
        break;
    }

    switch (fallback) {
    case kPositiveSignum:
        return fabs(primaryScrollDelta(event));
    case kNegativeSignum:
        return -fabs(primaryScrollDelta(event));
    case kZeroSignum:
        return 0;
    default:
        return primaryScrollDelta(event) * (isScrollFlipped(event) ? -1 : 1);
    }
}


//  MARK: -


typedef enum {
    kDiscretelyScrolled,
    kContinuousScrollMayBegin,
    kContinuousScrollBegan,
    kContinuousScrollChanged,
    kContinuousScrollEnded,
    kContinuousScrollCancelled,
    kMomentumScrollBegan,
    kMomentumScrollChanged,
    kMomentumScrollEnded,
} ScrollType;


static ScrollType scrollOf(CGEventRef event);
static void setScrollOf(CGEventRef event, ScrollType scroll);
static CGEventRef createZoomEvent(CGEventRef event, CGGesturePhase phase, CGPoint center, double value);


typedef enum {
    kStateNotInSession,

    kStateScrollMayBegin,
    kStateScrollInProgress,
    kStateMomentumScrollInProgress,

    kStateZoomInProgress,
    kStateZoomToEndAfterWaiting,  ///< Same as InProgress, but emits an additional end event if idle.
    kStateZoomStoppedByAttenuation,  ///< Same as NotInSession, but discards momentum scrolls until session ends.

} StateType;

static int stateCategory(StateType type) {
    if (type == kStateNotInSession) {return 0;}
    if (type <= kStateMomentumScrollInProgress) {return 1;}
    if (type <= kStateZoomStoppedByAttenuation) {return 2;}
    __builtin_unreachable();
}


struct _STZState {
    StateType           type;
    bool                needsFixScroll;
    double              delayedZoom;
    CGEventRef          refEvent;
    CGEventTimestamp    endTimeout;
    CGEventTimestamp    momentumStart;
    CGPoint             zoomCenter;
    uint64_t            sessionData;
};


static const CGEventTimestamp kEventDelayDuration = (int64_t)(0.01667 * NSEC_PER_SEC);
static const CGEventTimestamp kDiscreteScrollTimeout = (int64_t)(0.35 * NSEC_PER_SEC);
static const CGEventTimestamp kMomentumScrollTimeout = (int64_t)(0.05 * NSEC_PER_SEC);


static EventResult updateStateNotInSession(STZStateRef state, CGEventRef event, ScrollType scroll, STZGestureType gesture, uint64_t fallbackScrollDir);
static EventResult updateStateScrollMayBegin(STZStateRef state, CGEventRef event, ScrollType scroll, STZGestureType gesture, uint64_t fallbackScrollDir);
static EventResult updateStateScrollInProgress(STZStateRef state, CGEventRef event, ScrollType scroll, STZGestureType gesture, uint64_t fallbackScrollDir);
static EventResult updateStateMomentumScrollInProgress(STZStateRef state, CGEventRef event, ScrollType scroll, STZGestureType gesture, uint64_t fallbackScrollDir);
static EventResult updateStateZoomInProgress(STZStateRef state, CGEventRef event, ScrollType scroll, STZGestureType gesture, uint64_t fallbackScrollDir);


static void discardRefEvent(STZStateRef state) {
    if (state->refEvent == NULL) {return;}
    CFRelease(state->refEvent);
    state->refEvent = NULL;
}


static void checkMomentumStart(STZStateRef state, CGEventRef event, ScrollType scroll) {
    if (scroll == kMomentumScrollBegan) {
        state->momentumStart = CGEventGetTimestamp(event);
    } else if (scroll < kMomentumScrollBegan) {
        state->momentumStart = kCGEventDistantFuture;
    }
}


STZStateRef STZStateCreate(void){
    STZStateRef state = malloc(sizeof(*state));
    state->type = kStateNotInSession;
    state->needsFixScroll = false;
    state->delayedZoom = 0;
    state->refEvent = NULL;
    state->momentumStart = kCGEventDistantFuture;
    state->sessionData = 0;
    return (STZStateRef){state};
}

void STZStateRelease(STZStateRef state) {
    discardRefEvent(state);
    free(state);
}


bool STZStateGetSessionData(STZStateRef state, uint64_t *outData) {
    if (state->type == kStateNotInSession) {
        if (outData) {*outData = 0;}
        return false;
    } else {
        if (outData) {*outData = state->sessionData;}
        return true;
    }
}


void STZStateReadScrollEvent(STZStateRef state, CGEventRef event) {
    discardRefEvent(state);
    state->needsFixScroll = false;

    StateType oldType = state->type;

    ScrollType scroll = scrollOf(event);
    checkMomentumStart(state, event, scroll);

    switch (scroll) {
    case kDiscretelyScrolled:
        state->type = kStateNotInSession;
        break;
    case kContinuousScrollMayBegin:
        state->type = kStateScrollMayBegin;
        break;
    case kContinuousScrollBegan:
    case kContinuousScrollChanged:
        state->type = kStateScrollInProgress;
        break;
    case kContinuousScrollEnded:
    case kContinuousScrollCancelled:
        state->type = kStateNotInSession;
        break;
    case kMomentumScrollBegan:
    case kMomentumScrollChanged:
        state->type = kStateMomentumScrollInProgress;
        break;
    case kMomentumScrollEnded:
        state->type = kStateNotInSession;
        break;
    }

    StateType newType = state->type;
    if (newType == kStateNotInSession || stateCategory(newType) != stateCategory(oldType)) {
        state->sessionData = 0;
    }
}


CGEventRef STZStateTransformScrollEvent(STZStateRef state, CGEventRef event, STZGestureType gesture,
                                        uint64_t fallbackScrollDir, uint64_t const *sessionData,
                                        STZEventPlacement *returnEventPlacement) {
    discardRefEvent(state);
    state->needsFixScroll = false;

    StateType oldType = state->type;
    if (gesture == kSTZZoom && oldType < kStateZoomInProgress) {
        state->zoomCenter = CGEventGetLocation(event);
    }

    ScrollType scroll = scrollOf(event);
    checkMomentumStart(state, event, scroll);

    EventResult result;
    switch (oldType) {
    case kStateNotInSession:
        result = updateStateNotInSession(state, event, scroll, gesture, fallbackScrollDir);
        break;
    case kStateScrollMayBegin:
        result = updateStateScrollMayBegin(state, event, scroll, gesture, fallbackScrollDir);
        break;
    case kStateScrollInProgress:
        result = updateStateScrollInProgress(state, event, scroll, gesture, fallbackScrollDir);
        break;
    case kStateMomentumScrollInProgress:
        result = updateStateMomentumScrollInProgress(state, event, scroll, gesture, fallbackScrollDir);
        break;
    case kStateZoomInProgress:
    case kStateZoomToEndAfterWaiting:
        result = updateStateZoomInProgress(state, event, scroll, gesture, fallbackScrollDir);
        break;
    case kStateZoomStoppedByAttenuation:
        if (scroll == kMomentumScrollChanged) {
            result = discardEvent();
        } else if (scroll == kMomentumScrollEnded) {
            state->type = kStateNotInSession;
            result = discardEvent();
        } else {
            state->type = kStateNotInSession;
            result = updateStateNotInSession(state, event, scroll, gesture, fallbackScrollDir);
        }
        break;
    }

    StateType newType = state->type;
    if (newType == kStateNotInSession) {
        state->sessionData = 0;
    } else if (sessionData != NULL) {
        state->sessionData = *sessionData;
    } else if (stateCategory(newType) != stateCategory(oldType)) {
        state->sessionData = 0;
    }

    assert((state->refEvent != NULL) == (state->type == kStateZoomToEndAfterWaiting));
    assert((state->refEvent != NULL) || (state->delayedZoom == 0));

    *returnEventPlacement = result.otherPlacement;
    return result.otherEvent;
}


CGEventRef STZStateRevertToScrollByEvent(STZStateRef state, CGEventRef event) {
    switch (state->type) {
    case kStateNotInSession:
    case kStateScrollMayBegin:
    case kStateScrollInProgress:
    case kStateMomentumScrollInProgress:
        return NULL;

    case kStateZoomInProgress:
    case kStateZoomToEndAfterWaiting:
        discardRefEvent(state);
        state->needsFixScroll = true;
        state->type = kStateNotInSession;
        state->delayedZoom = 0;
        state->sessionData = 0;
        return createZoomEvent(event, kCGGesturePhaseEnded, state->zoomCenter, 0);

    case kStateZoomStoppedByAttenuation:
        state->needsFixScroll = true;
        state->sessionData = 0;
        state->type = kStateNotInSession;
        return NULL;
    }
}


CGEventRef STZStatePeriodicallyUpdate(STZStateRef state, CGEventTimestamp now) {
    if (state->type != kStateZoomToEndAfterWaiting) {return NULL;}

    CGEventTimestamp then = CGEventGetTimestamp(state->refEvent);
    if (state->delayedZoom != 0 && now - then >= kEventDelayDuration) {
        CGEventRef event = createZoomEvent(state->refEvent, kCGGesturePhaseChanged, state->zoomCenter, state->delayedZoom);
        CGEventSetTimestamp(event, now);
        state->delayedZoom = 0;
        return event;
    }

    if (now - then >= state->endTimeout) {
        CGEventRef event = createZoomEvent(state->refEvent, kCGGesturePhaseEnded, state->zoomCenter, 0);
        CGEventSetTimestamp(event, now);
        discardRefEvent(state);
        state->sessionData = 0;
        state->type = kStateNotInSession;
        return event;
    }

    return NULL;
}


CGEventTimestamp STZStateGetNextUpdatePeriod(STZStateRef state, CGEventTimestamp now) {
    if (state->type != kStateZoomToEndAfterWaiting) {return 0;}

    CGEventTimestamp fireAt = CGEventGetTimestamp(state->refEvent);
    if (state->delayedZoom != 0) {
        fireAt += kEventDelayDuration;
    } else {
        fireAt += state->endTimeout;
    }

    if (fireAt <= now) {return 1;}
    return fireAt - now;
}


bool STZStateCanStopTransformingEvents(STZStateRef state) {
    switch (state->type) {
    case kStateNotInSession:
    case kStateScrollMayBegin:
    case kStateScrollInProgress:
    case kStateMomentumScrollInProgress:
        return !state->needsFixScroll;
    case kStateZoomInProgress:
    case kStateZoomToEndAfterWaiting:
    case kStateZoomStoppedByAttenuation:
        return false;
    }
}


//  MARK: - State Transition Routes


static double magnificationFromScroll(CGEventRef event, uint64_t fallbackScrollDir, CGEventTimestamp momentumStart) {
    double value = STZReadScrollDeltaFromEvent(event, fallbackScrollDir, false) * STZGetMagnificationScalar();

    CGEventTimestamp now = CGEventGetTimestamp(event);
    if (now >= momentumStart) {
        double k = 1 - STZGetMomentumZoomAttenuation();
        double dt = (double)(now - momentumStart) / NSEC_PER_SEC;
        value *= k != 0 ? pow(k, dt / k) : 0;

        if (fabs(value) < STZGetMomentumZoomMinValue()) {
            value = 0;
        }
    }

    return value;
}


static EventResult updateStateNotInSession(STZStateRef state, CGEventRef event, ScrollType scroll, STZGestureType gesture, uint64_t fallbackScrollDir) {
    double value;

    switch (scroll) {
    case kDiscretelyScrolled:
        switch (gesture) {
        case kSTZScroll:
            return keepEvent();

        case kSTZZoom:
            value = magnificationFromScroll(event, fallbackScrollDir, kCGEventDistantFuture);
            state->type = kStateZoomToEndAfterWaiting;
            state->refEvent = CGEventCreateCopy(event);
            state->endTimeout = kDiscreteScrollTimeout;
            state->delayedZoom += value;
            return replaceEvent(createZoomEvent(event, kCGGesturePhaseBegan, state->zoomCenter, 0));
        }

    case kContinuousScrollMayBegin:
        switch (gesture) {
        case kSTZScroll:
            state->type = kStateScrollMayBegin;
            return keepEvent();

        case kSTZZoom:
            return discardEvent();
        }

    case kContinuousScrollBegan:
    case kContinuousScrollChanged:
        switch (gesture) {
        case kSTZScroll:
            setScrollOf(event, kContinuousScrollBegan);
            state->type = kStateScrollInProgress;
            return keepEvent();

        case kSTZZoom:
            value = magnificationFromScroll(event, fallbackScrollDir, kCGEventDistantFuture);
            state->type = kStateZoomInProgress;
            return replaceEvent(createZoomEvent(event, kCGGesturePhaseBegan, state->zoomCenter, value));
        }

    case kContinuousScrollEnded:
    case kContinuousScrollCancelled:
        return discardEvent();

    case kMomentumScrollBegan:
    case kMomentumScrollChanged:
        switch (gesture) {
        case kSTZScroll:
            setScrollOf(event, kMomentumScrollBegan);
            state->type = kStateMomentumScrollInProgress;
            return keepEvent();

        case kSTZZoom:
            value = magnificationFromScroll(event, fallbackScrollDir, state->momentumStart);
            state->type = kStateZoomInProgress;
            return replaceEvent(createZoomEvent(event, kCGGesturePhaseBegan, state->zoomCenter, value));
        }

    case kMomentumScrollEnded:
        return discardEvent();
    }
}


static EventResult updateStateScrollMayBegin(STZStateRef state, CGEventRef event, ScrollType scroll, STZGestureType gesture, uint64_t fallbackScrollDir) {
    double value;

    switch (scroll) {
    case kDiscretelyScrolled:
        switch (gesture) {
        case kSTZScroll:
            setScrollOf(event, kContinuousScrollCancelled);
            state->type = kStateNotInSession;
            return keepEvent();

        case kSTZZoom:
            value = magnificationFromScroll(event, fallbackScrollDir, kCGEventDistantFuture);
            setScrollOf(event, kContinuousScrollCancelled);
            state->type = kStateZoomToEndAfterWaiting;
            state->refEvent = CGEventCreateCopy(event);
            state->endTimeout = kDiscreteScrollTimeout;
            state->delayedZoom += value;
            return appendEvent(createZoomEvent(event, kCGGesturePhaseBegan, state->zoomCenter, 0));
        }

    case kContinuousScrollMayBegin:
        return discardEvent();

    case kContinuousScrollBegan:
    case kContinuousScrollChanged:
        switch (gesture) {
        case kSTZScroll:
            setScrollOf(event, kContinuousScrollBegan);
            state->type = kStateScrollInProgress;
            return keepEvent();

        case kSTZZoom:
            value = magnificationFromScroll(event, fallbackScrollDir, kCGEventDistantFuture);
            setScrollOf(event, kContinuousScrollCancelled);
            state->type = kStateZoomInProgress;
            return appendEvent(createZoomEvent(event, kCGGesturePhaseBegan, state->zoomCenter, value));
        }

    case kContinuousScrollEnded:
    case kContinuousScrollCancelled:
        state->type = kStateNotInSession;
        return keepEvent();

    case kMomentumScrollBegan:
    case kMomentumScrollChanged:
        switch (gesture) {
        case kSTZScroll:
            //  Begin momentum scroll on the next event.
            state->needsFixScroll = true;
            setScrollOf(event, kContinuousScrollCancelled);
            state->type = kStateNotInSession;
            return keepEvent();

        case kSTZZoom:
            value = magnificationFromScroll(event, fallbackScrollDir, state->momentumStart);
            setScrollOf(event, kContinuousScrollCancelled);
            state->type = kStateZoomInProgress;
            return appendEvent(createZoomEvent(event, kCGGesturePhaseBegan, state->zoomCenter, value));
        }

    case kMomentumScrollEnded:
        setScrollOf(event, kContinuousScrollCancelled);
        state->type = kStateNotInSession;
        return keepEvent();
    }
}


static EventResult updateStateScrollInProgress(STZStateRef state, CGEventRef event, ScrollType scroll, STZGestureType gesture, uint64_t fallbackScrollDir) {
    double value;

    switch (scroll) {
    case kDiscretelyScrolled:
        switch (gesture) {
        case kSTZScroll:
            setScrollOf(event, kContinuousScrollEnded);
            state->type = kStateNotInSession;
            return keepEvent();

        case kSTZZoom:
            value = magnificationFromScroll(event, fallbackScrollDir, kCGEventDistantFuture);
            setScrollOf(event, kContinuousScrollEnded);
            state->type = kStateZoomToEndAfterWaiting;
            state->refEvent = CGEventCreateCopy(event);
            state->endTimeout = kDiscreteScrollTimeout;
            state->delayedZoom += value;
            return appendEvent(createZoomEvent(event, kCGGesturePhaseBegan, state->zoomCenter, 0));
        }

    case kContinuousScrollMayBegin:
        return discardEvent();

    case kContinuousScrollBegan:
    case kContinuousScrollChanged:
        switch (gesture) {
        case kSTZScroll:
            setScrollOf(event, kContinuousScrollChanged);
            state->type = kStateScrollInProgress;
            return keepEvent();

        case kSTZZoom:
            value = magnificationFromScroll(event, fallbackScrollDir, kCGEventDistantFuture);
            setScrollOf(event, kContinuousScrollEnded);
            state->type = kStateZoomInProgress;
            return appendEvent(createZoomEvent(event, kCGGesturePhaseBegan, state->zoomCenter, value));
        }

    case kContinuousScrollEnded:
    case kContinuousScrollCancelled:
        state->type = kStateNotInSession;
        return keepEvent();

    case kMomentumScrollBegan:
    case kMomentumScrollChanged:
        switch (gesture) {
        case kSTZScroll:
            //  Begin momentum scroll on the next event.
            state->needsFixScroll = true;
            setScrollOf(event, kContinuousScrollEnded);
            state->type = kStateNotInSession;
            return keepEvent();

        case kSTZZoom:
            value = magnificationFromScroll(event, fallbackScrollDir, state->momentumStart);
            setScrollOf(event, kContinuousScrollEnded);
            state->type = kStateZoomInProgress;
            return appendEvent(createZoomEvent(event, kCGGesturePhaseBegan, state->zoomCenter, value));
        }

    case kMomentumScrollEnded:
        setScrollOf(event, kContinuousScrollEnded);
        state->type = kStateNotInSession;
        return keepEvent();
    }
}


static EventResult updateStateMomentumScrollInProgress(STZStateRef state, CGEventRef event, ScrollType scroll, STZGestureType gesture, uint64_t fallbackScrollDir) {
    double value;

    switch (scroll) {
    case kDiscretelyScrolled:
        switch (gesture) {
        case kSTZScroll:
            setScrollOf(event, kMomentumScrollEnded);
            state->type = kStateNotInSession;
            return keepEvent();

        case kSTZZoom:
            value = magnificationFromScroll(event, fallbackScrollDir, kCGEventDistantFuture);
            setScrollOf(event, kMomentumScrollEnded);
            state->type = kStateZoomToEndAfterWaiting;
            state->refEvent = CGEventCreateCopy(event);
            state->endTimeout = kDiscreteScrollTimeout;
            state->delayedZoom += value;
            return appendEvent(createZoomEvent(event, kCGGesturePhaseBegan, state->zoomCenter, 0));
        }

    case kContinuousScrollMayBegin:
        return discardEvent();

    case kContinuousScrollBegan:
    case kContinuousScrollChanged:
        switch (gesture) {
        case kSTZScroll:
            //  Begin continuous scroll on the next event.
            state->needsFixScroll = true;
            setScrollOf(event, kMomentumScrollEnded);
            state->type = kStateNotInSession;
            return keepEvent();

        case kSTZZoom:
            value = magnificationFromScroll(event, fallbackScrollDir, kCGEventDistantFuture);
            setScrollOf(event, kMomentumScrollEnded);
            state->type = kStateZoomInProgress;
            return appendEvent(createZoomEvent(event, kCGGesturePhaseBegan, state->zoomCenter, value));
        }

    case kContinuousScrollEnded:
    case kContinuousScrollCancelled:
        setScrollOf(event, kMomentumScrollEnded);
        state->type = kStateNotInSession;
        return keepEvent();

    case kMomentumScrollBegan:
    case kMomentumScrollChanged:
        switch (gesture) {
        case kSTZScroll:
            setScrollOf(event, kMomentumScrollChanged);
            state->type = kStateMomentumScrollInProgress;
            return keepEvent();

        case kSTZZoom:
            value = magnificationFromScroll(event, fallbackScrollDir, state->momentumStart);
            setScrollOf(event, kMomentumScrollEnded);
            state->type = kStateZoomInProgress;
            return appendEvent(createZoomEvent(event, kCGGesturePhaseBegan, state->zoomCenter, value));
        }

    case kMomentumScrollEnded:
        state->type = kStateNotInSession;
        return keepEvent();
    }
}


static EventResult updateStateZoomInProgress(STZStateRef state, CGEventRef event, ScrollType scroll, STZGestureType gesture, uint64_t fallbackScrollDir) {
    double value;
    double pending = state->delayedZoom;
    state->delayedZoom = 0;

    switch (scroll) {
    case kDiscretelyScrolled:
        switch (gesture) {
        case kSTZScroll:
            state->type = kStateNotInSession;
            return prependEvent(createZoomEvent(event, kCGGesturePhaseEnded, state->zoomCenter, 0));

        case kSTZZoom:
            value = magnificationFromScroll(event, fallbackScrollDir, kCGEventDistantFuture) + pending;
            state->type = kStateZoomToEndAfterWaiting;
            state->refEvent = CGEventCreateCopy(event);
            state->endTimeout = kDiscreteScrollTimeout;
            return replaceEvent(createZoomEvent(event, kCGGesturePhaseChanged, state->zoomCenter, value));
        }

    case kContinuousScrollMayBegin:
        state->type = kStateZoomInProgress;
        return discardEvent();

    case kContinuousScrollBegan:
    case kContinuousScrollChanged:
        switch (gesture) {
        case kSTZScroll:
            setScrollOf(event, kContinuousScrollBegan);
            state->type = kStateScrollInProgress;
            return prependEvent(createZoomEvent(event, kCGGesturePhaseEnded, state->zoomCenter, 0));

        case kSTZZoom:
            value = magnificationFromScroll(event, fallbackScrollDir, kCGEventDistantFuture) + pending;
            state->type = kStateZoomInProgress;
            return replaceEvent(createZoomEvent(event, kCGGesturePhaseChanged, state->zoomCenter, value));
        }

    case kContinuousScrollEnded:
    case kContinuousScrollCancelled:
        //  Waiting for `kMomentumScrollBegan` to avoid interrupting the zoom session.
        state->type = kStateZoomToEndAfterWaiting;
        state->refEvent = CGEventCreateCopy(event);
        state->endTimeout = kMomentumScrollTimeout;
        return discardEvent();

    case kMomentumScrollBegan:
    case kMomentumScrollChanged:
        switch (gesture) {
        case kSTZScroll:
            setScrollOf(event, kMomentumScrollBegan);
            state->type = kStateMomentumScrollInProgress;
            return prependEvent(createZoomEvent(event, kCGGesturePhaseEnded, state->zoomCenter, 0));

        case kSTZZoom:
            value = magnificationFromScroll(event, fallbackScrollDir, state->momentumStart) + pending;
            if (value == 0) {
                state->type = kStateZoomStoppedByAttenuation;
                return replaceEvent(createZoomEvent(event, kCGGesturePhaseEnded, state->zoomCenter, value));
            } else {
                state->type = kStateZoomInProgress;
                return replaceEvent(createZoomEvent(event, kCGGesturePhaseChanged, state->zoomCenter, value));
            }
        }

    case kMomentumScrollEnded:
        state->type = kStateNotInSession;
        return replaceEvent(createZoomEvent(event, kCGGesturePhaseEnded, state->zoomCenter, 0));
    }
}



//  MARK: - Event Adaptation


static ScrollType scrollOf(CGEventRef event) {
    assert(CGEventGetType(event) == kCGEventScrollWheel);

    CGScrollPhase sPhase = (CGScrollPhase)CGEventGetIntegerValueField(event, kCGScrollWheelEventScrollPhase);
    CGMomentumScrollPhase pPhase = (CGMomentumScrollPhase)CGEventGetIntegerValueField(event, kCGScrollWheelEventMomentumPhase);

    if (pPhase == kCGMomentumScrollPhaseNone) {
        switch (sPhase) {
        case 0 /* Non-continuous */:            return kDiscretelyScrolled;
        case kCGScrollPhaseMayBegin:            return kContinuousScrollMayBegin;
        case kCGScrollPhaseBegan:               return kContinuousScrollBegan;
        case kCGScrollPhaseChanged:             return kContinuousScrollChanged;
        case kCGScrollPhaseEnded:               return kContinuousScrollEnded;
        case kCGScrollPhaseCancelled:           return kContinuousScrollCancelled;
        default:
            STZUnknownEnumCase("CGScrollPhase", sPhase);
            return kDiscretelyScrolled;
        }

    } else {
        switch (pPhase) {
        case kCGMomentumScrollPhaseBegin:       return kMomentumScrollBegan;
        case kCGMomentumScrollPhaseContinue:    return kMomentumScrollChanged;
        case kCGMomentumScrollPhaseEnd:         return kMomentumScrollEnded;
        default:
            STZUnknownEnumCase("CGMomentumScrollPhase", pPhase);
            return kMomentumScrollEnded;
        }
    }
}


static void setScrollOf(CGEventRef event, ScrollType scroll) {
    switch (scroll) {
    case kDiscretelyScrolled:
        CGEventSetIntegerValueField(event, kCGScrollWheelEventScrollPhase, 0);
        CGEventSetIntegerValueField(event, kCGScrollWheelEventMomentumPhase, 0);
        break;
    case kContinuousScrollMayBegin:
        CGEventSetIntegerValueField(event, kCGScrollWheelEventScrollPhase, kCGScrollPhaseMayBegin);
        CGEventSetIntegerValueField(event, kCGScrollWheelEventMomentumPhase, 0);
        break;
    case kContinuousScrollBegan:
        CGEventSetIntegerValueField(event, kCGScrollWheelEventScrollPhase, kCGScrollPhaseBegan);
        CGEventSetIntegerValueField(event, kCGScrollWheelEventMomentumPhase, 0);
        break;
    case kContinuousScrollChanged:
        CGEventSetIntegerValueField(event, kCGScrollWheelEventScrollPhase, kCGScrollPhaseChanged);
        CGEventSetIntegerValueField(event, kCGScrollWheelEventMomentumPhase, 0);
        break;
    case kContinuousScrollEnded:
        CGEventSetIntegerValueField(event, kCGScrollWheelEventScrollPhase, kCGScrollPhaseEnded);
        CGEventSetIntegerValueField(event, kCGScrollWheelEventMomentumPhase, 0);
        break;
    case kContinuousScrollCancelled:
        CGEventSetIntegerValueField(event, kCGScrollWheelEventScrollPhase, kCGScrollPhaseCancelled);
        CGEventSetIntegerValueField(event, kCGScrollWheelEventMomentumPhase, 0);
        break;
    case kMomentumScrollBegan:
        CGEventSetIntegerValueField(event, kCGScrollWheelEventScrollPhase, 0);
        CGEventSetIntegerValueField(event, kCGScrollWheelEventMomentumPhase, kCGMomentumScrollPhaseBegin);
        break;
    case kMomentumScrollChanged:
        CGEventSetIntegerValueField(event, kCGScrollWheelEventScrollPhase, 0);
        CGEventSetIntegerValueField(event, kCGScrollWheelEventMomentumPhase, kCGMomentumScrollPhaseContinue);
        break;
    case kMomentumScrollEnded:
        CGEventSetIntegerValueField(event, kCGScrollWheelEventScrollPhase, 0);
        CGEventSetIntegerValueField(event, kCGScrollWheelEventMomentumPhase, kCGMomentumScrollPhaseEnd);
        break;
    }
}


static CGEventRef createZoomEvent(CGEventRef event, CGGesturePhase phase, CGPoint center, double value) {
    CGEventSourceRef source = CGEventCreateSourceFromEvent(event);
    CGEventRef zoom = CGEventCreate(source);
    if (source) {CFRelease(source);}

    CGEventSetType(zoom, kCGEventGesture);
    CGEventSetFlags(zoom, CGEventGetFlags(event));
    CGEventSetLocation(zoom, center);
    CGEventSetTimestamp(zoom, CGEventGetTimestamp(event));
    CGEventSetIntegerValueField(zoom, kCGGestureEventHIDType, kIOHIDEventTypeZoom);
    CGEventSetIntegerValueField(zoom, kCGGestureEventPhase, phase);
    CGEventSetDoubleValueField(zoom, kCGGestureEventZoomValue, value);

    return zoom;
}
