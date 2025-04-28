/*
 *  STZWheelSession.c
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/4/16.
 *  Copyright © 2025 alphaArgon.
 */

#import "STZWheelSession.h"
#import "STZSettings.h"
#import "CGEventSPI.h"


static char const *nameOfWheelType(STZWheelType type) {
    switch (type) {
    case kSTZWheelToScroll:         return "smooth scroll";
    case kSTZWheelToScrollMomentum: return "scroll momentum";
    case kSTZWheelToZoom:           return "scroll-to-zoom";
    }
}


void STZDebugLogSessionChange(char const *prefix, STZWheelSession old, STZWheelSession new) {
    if (!STZIsLoggingEnabled()) {return;}

    if (old.state == new.state && old.type == new.type) {return;}
    if (old.state == kSTZWheelFree && new.state == kSTZWheelFree) {return;}

    char const *oldArgs[2];
    int oldArgCount = 0;

    char const *newArgs[2];
    int newArgCount = 0;

    switch (old.state) {
    case kSTZWheelFree:
        oldArgs[oldArgCount++] = "wheel free";
        break;
    case kSTZWheelWillBegin:
        oldArgs[oldArgCount++] = "may";
        oldArgs[oldArgCount++] = nameOfWheelType(old.type);
        break;
    case kSTZWheelDidBegin:
        oldArgs[oldArgCount++] = nameOfWheelType(old.type);
        break;
    }

    switch (new.state) {
    case kSTZWheelFree:
        newArgs[newArgCount++] = "wheel free";
        break;
    case kSTZWheelWillBegin:
        newArgs[newArgCount++] = "may";
        newArgs[newArgCount++] = nameOfWheelType(new.type);
        break;
    case kSTZWheelDidBegin:
        newArgs[newArgCount++] = nameOfWheelType(new.type);
        break;
    }

    switch (oldArgCount * 10 + newArgCount) {
    case 11: return STZDebugLog("%s %s -> %s", prefix, oldArgs[0], newArgs[0]);
    case 12: return STZDebugLog("%s %s -> %s %s", prefix, oldArgs[0], newArgs[0], newArgs[1]);
    case 21: return STZDebugLog("%s %s %s -> %s", prefix, oldArgs[0], oldArgs[1], newArgs[0]);
    case 22: return STZDebugLog("%s %s %s -> %s %s", prefix, oldArgs[0], oldArgs[1], newArgs[0], newArgs[1]);
    default: return;
    }
}


static int64_t const kSTZZeroSignum     = (int64_t)'STZ.' << 32 | 'SIG0';
static int64_t const kSTZPositiveSignum = (int64_t)'STZ.' << 32 | 'SIG+';
static int64_t const kSTZNegativeSignum = (int64_t)'STZ.' << 32 | 'SIG-';
static int32_t const kSTZSignumField = kCGEventSourceUserData;


int STZGetDeltaSignumForScrollWheelEvent(CGEventRef event) {
    assert(CGEventGetType(event) == kCGEventScrollWheel);

    //  `hidEvent` could be `NULL`
    //  IOHIDEventRef hidEvent = CGEventCopyIOHIDEvent(event);
    //  double delta = IOHIDEventGetFloatValue(hidEvent, kIOHIDEventFieldScrollY);
    //  CFRelease(hidEvent);

    double delta = CGEventGetIntegerValueField(event, kCGScrollWheelEventDeltaAxis1);
    delta *= STZIsScrollWheelFlipped(event) ? -1 : 1;
    return delta != 0 ? delta > 0 ? 1 : -1 : 0;
}


bool STZMarkDeltaSignumForEvent(CGEventRef event, int signum) {
    if (CGEventGetIntegerValueField(event, kSTZSignumField) != 0) {return false;}

    switch (signum) {
    case 0:
        CGEventSetIntegerValueField(event, kSTZSignumField, kSTZZeroSignum);
        return true;
    case 1:
        CGEventSetIntegerValueField(event, kSTZSignumField, kSTZPositiveSignum);
        return true;
    case -1:
        CGEventSetIntegerValueField(event, kSTZSignumField, kSTZNegativeSignum);
        return true;
    default:
        return false;
    }
}


bool STZConsumeDeltaSignumForEvent(CGEventRef event, int *signum) {
    switch (CGEventGetIntegerValueField(event, kSTZSignumField)) {
    case kSTZZeroSignum:
        *signum = 0;
        return true;
    case kSTZPositiveSignum:
        *signum = 1;
        return true;
    case kSTZNegativeSignum:
        *signum = -1;
        return true;
    default:
        return false;
    }
}


STZTrivalent STZScrollWheelHasSuccessorEvent(STZPhase phase, bool byMomentum) {
    if (phase == kSTZPhaseNone || (phase == kSTZPhaseEnded && !byMomentum)) {
        return kSTZMaybe;
    } else {
        return phase != kSTZPhaseEnded && phase != kSTZPhaseCancelled;
    }
}


void STZConvertZoomFromScrollWheel(STZPhase *phase, bool byMomentum, int signum,  double delta,
                                   CGEventTimestamp now, CGEventTimestamp *momentumStart,
                                   double *outScale) {
    assert(signum == 0 || abs(signum) == 1);
    delta = signum * fabs(delta);

    switch (*phase) {
    case kSTZPhaseEnded:
    case kSTZPhaseCancelled:
        *outScale = 0;
        break;

    case kSTZPhaseMayBegin:
    case kSTZPhaseBegan:
        *outScale = delta * STZGetScrollToZoomMagnifier();
        
        if (byMomentum) {
            *momentumStart = now;
            
            if (STZGetScrollMomentumToZoomAttenuation() == 1) {
                *phase = kSTZPhaseEnded;
                *outScale = 0;
            }
        }
        break;
        
    case kSTZPhaseNone:
    case kSTZPhaseChanged:
        *outScale = delta * STZGetScrollToZoomMagnifier();
        
        if (byMomentum) {
            double k = 1 - STZGetScrollMomentumToZoomAttenuation();
            double dt = (now - *momentumStart) / (double)NSEC_PER_SEC;
            *outScale *= k != 0 ? pow(k, dt / k) : 0;
            
            if (fabs(*outScale) < STZGetScrollMinMomentumMagnification()) {
                *phase = kSTZPhaseEnded;
                *outScale = 0;
            }
        }
        break;
    }
}


void STZWheelSessionDiscard(STZWheelSession *session, CGEventRef byEvent,
                            CGEventRef *outEvent, STZEventAction *actionIfWheel) {
    assert(*outEvent == NULL);
    if (actionIfWheel) {
        assert(CGEventGetType(byEvent) == kCGEventScrollWheel);
    }

    STZPhase discardingPhase;
    switch (session->state) {
    case kSTZWheelFree:
        if (actionIfWheel) {
            *actionIfWheel = kSTZEventUnchanged;
        }
        return;
    case kSTZWheelWillBegin:
        discardingPhase = kSTZPhaseCancelled; break;
    case kSTZWheelDidBegin:
        discardingPhase = kSTZPhaseEnded; break;
    }

    session->state = kSTZWheelFree;
    switch (session->type) {
    case kSTZWheelToScroll:
        if (actionIfWheel) {
            STZAdaptScrollWheelEvent(byEvent, discardingPhase, false);
            *actionIfWheel = kSTZEventAdapted;
        } else {
            *outEvent = STZCreateScrollWheelEvent(byEvent);
            STZAdaptScrollWheelEvent(*outEvent, discardingPhase, false);
        } break;

    case kSTZWheelToScrollMomentum:
        if (actionIfWheel) {
            STZAdaptScrollWheelEvent(byEvent, discardingPhase, true);
            *actionIfWheel = kSTZEventAdapted;
        } else {
            *outEvent = STZCreateScrollWheelEvent(byEvent);
            STZAdaptScrollWheelEvent(*outEvent, discardingPhase, true);
        } break;

    case kSTZWheelToZoom:
        *outEvent = STZCreateZoomGestureEvent(byEvent);
        STZAdaptGestureEvent(*outEvent, discardingPhase, 0);
        if (actionIfWheel) {
            *actionIfWheel = kSTZEventReplaced;
        } break;
    }
}


void STZWheelSessionUpdate(STZWheelSession *session, STZWheelType type, STZPhase phase, double data,
                           CGEventRef wheelEvent, CGEventRef outEvents[2], STZEventAction *action) {
    assert(outEvents[0] == NULL && outEvents[1] == NULL);
    assert(CGEventGetType(wheelEvent) == kCGEventScrollWheel);

    //  free -> some that terminates: ignore the event
    //  free -> some not terminating: set phase to begin, send the event with phase begin
    //  some -> same that terminates: set phase to free, send the event
    //  some -> same not terminating: send the event with phase changed
    //  some -> any other: set phase to free, send the event with phase cancelled

    //  Gestures may be broken if we send `mayBegin` events. In this case we ignore the event and
    //  leave the session unchanged.
    if (type != kSTZWheelToScroll && phase == kSTZPhaseMayBegin) {
        *action = kSTZEventReplaced;  //  Discard
        return;
    }

    //  Non-continuous wheels <=> `phase == kPhaseNone`
    //  `!CGEventGetIntegerValueField(event, kCGScrollWheelEventIsContinuous)`
    if (phase == kSTZPhaseNone && type == kSTZWheelToScroll) {
        return STZWheelSessionDiscard(session, wheelEvent, outEvents, action);
    }

    //  Incompatible sessions. End previous session and discard proposed session.
    if (session->state != kSTZWheelFree && session->type != type) {
        return STZWheelSessionDiscard(session, wheelEvent, outEvents, action);
    }

    //  Now we filtered out normal scrolls. `kPhaseNone` will be treated analogously to
    //  `kPhaseChanged` which can force a session to begin.

    STZPhase proposedPhase = phase;

    switch (session->state) {
    case kSTZWheelFree:
        switch (phase) {
        case kSTZPhaseMayBegin:
            session->state = kSTZWheelWillBegin;
            break;

        case kSTZPhaseBegan:
        case kSTZPhaseNone:
        case kSTZPhaseChanged:
            session->state = kSTZWheelDidBegin;
            phase = kSTZPhaseBegan;
            break;

        case kSTZPhaseEnded:
        case kSTZPhaseCancelled:
            //  Already free, sending the event would cause unbalanced state.
            *action = kSTZEventReplaced;  //  Discard
            return;
        }
        break;

    case kSTZWheelWillBegin:
        switch (phase) {
        case kSTZPhaseMayBegin:
            *action = kSTZEventReplaced;  //  Discard
            return;

        case kSTZPhaseBegan:
        case kSTZPhaseNone:
        case kSTZPhaseChanged:
            session->state = kSTZWheelDidBegin;
            phase = kSTZPhaseBegan;
            break;

        case kSTZPhaseEnded:
            //  If not began, send a cancelled event rather than an end one.
            session->state = kSTZWheelFree;
            phase = kSTZPhaseCancelled;
            break;

        case kSTZPhaseCancelled:
            session->state = kSTZWheelFree;
            break;
        }
        break;

    case kSTZWheelDidBegin:
        switch (phase) {
        case kSTZPhaseMayBegin:
            *action = kSTZEventReplaced;  //  Discard
            return;

        case kSTZPhaseBegan:
        case kSTZPhaseNone:
        case kSTZPhaseChanged:
            phase = kSTZPhaseChanged;
            break;

        case kSTZPhaseEnded:
        case kSTZPhaseCancelled:
            session->state = kSTZWheelFree;
            break;
        }
        break;
    }

    session->type = type;
    switch (type) {
    case kSTZWheelToScroll:
        if (proposedPhase == phase) {
            *action = kSTZEventUnchanged;
        } else {
            STZAdaptScrollWheelEvent(wheelEvent, phase, false);
            *action = kSTZEventAdapted;
        }
        break;

    case kSTZWheelToScrollMomentum:
        if (proposedPhase == phase) {
            *action = kSTZEventUnchanged;
        } else {
            STZAdaptScrollWheelEvent(wheelEvent, phase, true);
            *action = kSTZEventAdapted;
        }
        break;

    case kSTZWheelToZoom:
        //  Initial scroll wheel may have no effect.
        if (phase == kSTZPhaseBegan && data != 0) {
            outEvents[0] = STZCreateZoomGestureEvent(wheelEvent);
            STZAdaptGestureEvent(outEvents[0], kSTZPhaseBegan, 0);
            outEvents[1] = STZCreateZoomGestureEvent(wheelEvent);
            STZAdaptGestureEvent(outEvents[1], kSTZPhaseChanged, data);
        } else {
            outEvents[0] = STZCreateZoomGestureEvent(wheelEvent);
            STZAdaptGestureEvent(outEvents[0], phase, data);
        }
        *action = kSTZEventReplaced;
        break;
    }
}


void STZWheelSessionAssign(STZWheelSession *session, STZWheelType type, STZPhase phase) {
    session->type = type;

    switch (phase) {
    case kSTZPhaseMayBegin:
        session->state = kSTZWheelWillBegin;
        break;
    case kSTZPhaseBegan:
    case kSTZPhaseChanged:
        session->state = kSTZWheelDidBegin;
        break;
    case kSTZPhaseNone:
    case kSTZPhaseEnded:
    case kSTZPhaseCancelled:
        session->state = kSTZWheelFree;
        break;
    }
}


bool STZWheelSessionIsEnded(STZWheelSession *const session, STZPhase byEventPhase) {
    if (session->state != kSTZWheelFree) {return false;}
    return byEventPhase == kSTZPhaseEnded
        || byEventPhase == kSTZPhaseCancelled
        || byEventPhase == kSTZPhaseNone;
}
