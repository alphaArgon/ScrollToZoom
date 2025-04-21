/*
 *  STZWheelSession.c
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/4/16.
 *  Copyright © 2025 alphaArgon.
 */

#import "STZWheelSession.h"
#import "STZEventTap.h"
#import "CGEventSPI.h"


static char const *nameOfWheelType(STZWheelType type) {
    switch (type) {
    case kSTZWheelToScroll:         return "smooth scroll";
    case kSTZWheelToScrollMomentum: return "scroll momentum";
    case kSTZWheelToZoom:           return "scroll-to-zoom";
    }
}


static char const *nameOfPhase(STZPhase phase) {
    switch (phase) {
    case kSTZPhaseNone:             return "none";
    case kSTZPhaseBegan:            return "began";
    case kSTZPhaseChanged:          return "changed";
    case kSTZPhaseEnded:            return "ended";
    case kSTZPhaseCancelled:        return "cancelled";
    case kSTZPhaseMayBegin:         return "may begin";
    }
}


void STZDebugLogEvent(char const *prefix, CGEventRef event) {
    if (!STZIsLoggingEnabled()) {return;}

    CFStringRef flagDesc;
    STZValidateModifierFlags(CGEventGetFlags(event), &flagDesc);

    if (CFStringGetLength(flagDesc) == 0) {
        flagDesc = CFSTR("no flags");
    }

    CGFloat data;
    STZPhase phase;
    bool byMomentum;

    switch (CGEventGetType(event)) {
    case kCGEventFlagsChanged:
        STZDebugLog("%s flags changed with %@", prefix, flagDesc);
        break;

    case kCGEventScrollWheel:
        data = CGEventGetIntegerValueField(event, kCGScrollWheelEventPointDeltaAxis1);
        STZGetPhaseFromScrollWheelEvent(event, &phase, &byMomentum);
        char const *phaseTag = byMomentum ? "momentum" : "smooth";
        char const *tail = STZIsScrollWheelFlipped(event) ? ", flipped" : "";
        STZDebugLog("%s scroll wheel with %@, %s %s, moved %0.2fpx%s",
                    prefix, flagDesc, phaseTag, nameOfPhase(phase), data, tail);
        break;

    case kCGEventGesture:
        data = CGEventGetDoubleValueField(event, kCGGestureEventZoomValue);
        STZGetPhaseFromGestureEvent(event, &phase);
        STZDebugLog("%s zoom gesture with %@, gesture %s, scaled %0.02f%%",
                    prefix, flagDesc, nameOfPhase(phase), (1 + data) * 100);
        break;

    default:
        STZDebugLog("%s unexpected event with %@", prefix, flagDesc);
        break;
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


bool STZIsScrollWheelFlipped(CGEventRef event) {
    assert(CGEventGetType(event) == kCGEventScrollWheel);

    //  `137` is reverse engineered from `-[NSEvent isDirectionInvertedFromDevice]`.
    return CGEventGetIntegerValueField(event, 137) != 0;
}


void STZGetPhaseFromScrollWheelEvent(CGEventRef event, STZPhase *outPhase, bool *outByMomentum) {
    assert(CGEventGetType(event) == kCGEventScrollWheel);

    CGScrollPhase sPhase = (CGScrollPhase)CGEventGetIntegerValueField(event, kCGScrollWheelEventScrollPhase);
    CGMomentumScrollPhase pPhase = (CGMomentumScrollPhase)CGEventGetIntegerValueField(event, kCGScrollWheelEventMomentumPhase);

    if (pPhase == kCGMomentumScrollPhaseNone) {
        *outByMomentum = false;

        switch (sPhase) {
        case 0 /* Non-continuous */:            *outPhase = kSTZPhaseNone; break;
        case kCGScrollPhaseMayBegin:            *outPhase = kSTZPhaseMayBegin; break;
        case kCGScrollPhaseBegan:               *outPhase = kSTZPhaseBegan; break;
        case kCGScrollPhaseChanged:             *outPhase = kSTZPhaseChanged; break;
        case kCGScrollPhaseEnded:               *outPhase = kSTZPhaseEnded; break;
        case kCGScrollPhaseCancelled:           *outPhase = kSTZPhaseCancelled; break;
        default:
            STZUnknownEnumCase("CGScrollPhase", sPhase);
            *outPhase = kSTZPhaseChanged; break;
        }

    } else {
        *outByMomentum = true;

        switch (pPhase) {
        case kCGMomentumScrollPhaseBegin:       *outPhase = kSTZPhaseBegan; break;
        case kCGMomentumScrollPhaseContinue:    *outPhase = kSTZPhaseChanged; break;
        case kCGMomentumScrollPhaseEnd:         *outPhase = kSTZPhaseEnded; break;
        default:
            STZUnknownEnumCase("CGMomentumScrollPhase", pPhase);
            *outPhase = kSTZPhaseChanged; break;
        }
    }
}


void STZGetPhaseFromGestureEvent(CGEventRef event, STZPhase *outPhase) {
    assert(CGEventGetType(event) == kCGEventGesture);

    CGGesturePhase phase = (CGGesturePhase)CGEventGetIntegerValueField(event, kCGGestureEventPhase);

    switch (phase) {
    case kCGGesturePhaseNone:               *outPhase = kSTZPhaseNone; break;
    case kCGGesturePhaseBegan:              *outPhase = kSTZPhaseBegan; break;
    case kCGGesturePhaseChanged:            *outPhase = kSTZPhaseChanged; break;
    case kCGGesturePhaseEnded:              *outPhase = kSTZPhaseEnded; break;
    case kCGGesturePhaseCancelled:          *outPhase = kSTZPhaseCancelled; break;
    case kCGGesturePhaseMayBegin:           *outPhase = kSTZPhaseMayBegin; break;
    default:
        STZUnknownEnumCase("CGGesturePhase", phase);
        *outPhase = kSTZPhaseChanged; break;
    }
}


void STZAdaptScrollWheelEvent(CGEventRef event, STZPhase phase, bool byMomentum) {
    assert(CGEventGetType(event) == kCGEventScrollWheel);

    CGScrollPhase sPhase;
    CGScrollPhase pPhase;

    if (byMomentum) {
        switch (phase) {
        case kSTZPhaseMayBegin:
        case kSTZPhaseNone:         pPhase = kCGMomentumScrollPhaseNone; break;
        case kSTZPhaseBegan:        pPhase = kCGMomentumScrollPhaseBegin; break;
        case kSTZPhaseChanged:      pPhase = kCGMomentumScrollPhaseContinue; break;
        case kSTZPhaseEnded:
        case kSTZPhaseCancelled:    pPhase = kCGMomentumScrollPhaseEnd; break;
        }
        sPhase = 0;

    } else {
        switch (phase) {
        case kSTZPhaseNone:         sPhase = 0; break;
        case kSTZPhaseMayBegin:     sPhase = kCGScrollPhaseMayBegin; break;
        case kSTZPhaseBegan:        sPhase = kCGScrollPhaseBegan; break;
        case kSTZPhaseChanged:      sPhase = kCGScrollPhaseChanged; break;
        case kSTZPhaseEnded:        sPhase = kCGScrollPhaseEnded; break;
        case kSTZPhaseCancelled:    sPhase = kCGScrollPhaseCancelled; break;
        }
        pPhase = kCGMomentumScrollPhaseNone;
    }


    CGEventSetIntegerValueField(event, kCGScrollWheelEventScrollPhase, sPhase);
    CGEventSetIntegerValueField(event, kCGScrollWheelEventMomentumPhase, pPhase);
}


void STZAdaptGestureEvent(CGEventRef event, STZPhase phase, double scale) {
    assert(CGEventGetType(event) == kCGEventGesture);

    CGGesturePhase gPhase;

    switch (phase) {
    case kSTZPhaseNone:         gPhase = kCGGesturePhaseNone; break;
    case kSTZPhaseMayBegin:     gPhase = kCGGesturePhaseMayBegin; break;
    case kSTZPhaseBegan:        gPhase = kCGGesturePhaseBegan; break;
    case kSTZPhaseChanged:      gPhase = kCGGesturePhaseChanged; break;
    case kSTZPhaseEnded:        gPhase = kCGGesturePhaseEnded; break;
    case kSTZPhaseCancelled:    gPhase = kCGGesturePhaseCancelled; break;
    }

    CGEventSetIntegerValueField(event, kCGGestureEventPhase, gPhase);
    CGEventSetDoubleValueField(event, kCGGestureEventZoomValue, scale);
}


CGEventRef STZCreateScrollWheelEvent(CGEventRef sample) {
    CGEventSourceRef source = CGEventCreateSourceFromEvent(sample);
    CGEventRef event = CGEventCreate(source);
    if (source) {CFRelease(source);}

    CGEventSetType(event, kCGEventScrollWheel);
    CGEventSetFlags(event, CGEventGetFlags(sample));
    CGEventSetLocation(event, CGEventGetLocation(sample));
    CGEventSetTimestamp(event, CGEventGetTimestamp(sample));

    return event;
}


CGEventRef STZCreateZoomGestureEvent(CGEventRef sample) {
    CGEventSourceRef source = CGEventCreateSourceFromEvent(sample);
    CGEventRef event = CGEventCreate(source);
    if (source) {CFRelease(source);}

    CGEventSetType(event, kCGEventGesture);
    CGEventSetFlags(event, CGEventGetFlags(sample));
    CGEventSetLocation(event, CGEventGetLocation(sample));
    CGEventSetTimestamp(event, CGEventGetTimestamp(sample));
    CGEventSetIntegerValueField(event, kCGGestureEventHIDType, kIOHIDEventTypeZoom);

    return event;
}


static int64_t const kSTZZeroSignum     = (int64_t)'STZ.' << 32 | 'SIG0';
static int64_t const kSTZPositiveSignum = (int64_t)'STZ.' << 32 | 'SIG+';
static int64_t const kSTZNegativeSignum = (int64_t)'STZ.' << 32 | 'SIG-';
static int32_t const kSTZSignumField = kCGEventSourceUserData;


int STZMarkDeltaSignumForScrollWheelEvent(CGEventRef event) {
    assert(CGEventGetType(event) == kCGEventScrollWheel);

    //  `hidEvent` could be `NULL`
    //  IOHIDEventRef hidEvent = CGEventCopyIOHIDEvent(event);
    //  double delta = IOHIDEventGetFloatValue(hidEvent, kIOHIDEventFieldScrollY);
    //  CFRelease(hidEvent);

    double delta = CGEventGetIntegerValueField(event, kCGScrollWheelEventDeltaAxis1);
    delta *= STZIsScrollWheelFlipped(event) ? -1 : 1;

    int signum;
    uint64_t userData;

    if (delta == 0) {
        signum = 0;
        userData = kSTZZeroSignum;
    } else if (delta > 0) {
        signum = +1;
        userData = kSTZPositiveSignum;
    } else {
        signum = -1;
        userData = kSTZNegativeSignum;
    }

    if (CGEventGetIntegerValueField(event, kSTZSignumField) == 0) {
        CGEventSetIntegerValueField(event, kSTZSignumField, userData);
        STZDebugLog("Mark %p %lld", event, CGEventGetIntegerValueField(event, kSTZSignumField));
    }

    return signum;
}


void STZConvertPhaseFromScrollWheelEvent(CGEventRef event, int signum, CGEventTimestamp *momentumStart,
                                         STZPhase *outPhase, double *outScale,
                                         STZTrivalent *outEventHasSuccessor) {
    assert(signum == 0 || abs(signum) == 1);

    bool caughtUserData = false;
    switch (CGEventGetIntegerValueField(event, kSTZSignumField)) {
    case kSTZZeroSignum:        signum = 0; caughtUserData = true; break;
    case kSTZPositiveSignum:    signum = +1; caughtUserData = true; break;
    case kSTZNegativeSignum:    signum = -1; caughtUserData = true; break;
    }

    if (caughtUserData) {
        CGEventSetIntegerValueField(event, kSTZSignumField, 0);
    }

    double delta = CGEventGetIntegerValueField(event, kCGScrollWheelEventPointDeltaAxis1);
    delta = signum * fabs(delta);

    bool byMomentum;
    STZGetPhaseFromScrollWheelEvent(event, outPhase, &byMomentum);

    if (*outPhase == kSTZPhaseNone) {
        *outEventHasSuccessor = kSTZMaybe;
    } else if (delta == 0 && !byMomentum && *outPhase == kSTZPhaseEnded) {
        *outEventHasSuccessor = STZGetScrollMomentumToZoomAttenuation() == 1 ? false : kSTZMaybe;
    } else {
        *outEventHasSuccessor = *outPhase != kSTZPhaseEnded && *outPhase != kSTZPhaseChanged;
    }

    switch (*outPhase) {
    case kSTZPhaseEnded:
    case kSTZPhaseCancelled:
        *outScale = 0;
        break;

    case kSTZPhaseMayBegin:
    case kSTZPhaseBegan:
        *outScale = delta * STZGetScrollToZoomMagnifier();

        if (byMomentum) {
            *momentumStart = CGEventGetTimestamp(event);

            if (STZGetScrollMomentumToZoomAttenuation() == 1) {
                *outPhase = kSTZPhaseEnded;
                *outScale = 0;
            }
        }

        break;

    case kSTZPhaseNone:
    case kSTZPhaseChanged:
        *outScale = delta * STZGetScrollToZoomMagnifier();

        if (byMomentum) {
            double k = 1 - STZGetScrollMomentumToZoomAttenuation();
            double dt = (CGEventGetTimestamp(event) - *momentumStart) / (double)NSEC_PER_SEC;
            *outScale *= k != 0 ? pow(k, dt / k) : 0;

            if (fabs(*outScale) < STZGetScrollMinMomentumMagnification()) {
                *outPhase = kSTZPhaseEnded;
                *outScale = 0;
            }
        }
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
        *action = kSTZEventReplaced;
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
            *action = kSTZEventReplaced;
            return;
        }
        break;

    case kSTZWheelWillBegin:
        switch (phase) {
        case kSTZPhaseMayBegin:
            *action = kSTZEventReplaced;
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
            *action = kSTZEventReplaced;
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
