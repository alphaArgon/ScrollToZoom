/*
 *  STZService.c
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/1/24.
 *  Copyright © 2025 alphaArgon.
 */

#import "STZEventTap.h"
#import "STZHandlers.h"
#import <ApplicationServices/ApplicationServices.h>


CGEventFlags STZScrollToZoomFlags = kCGEventFlagMaskCommand;

double STZScrollToZoomMagnifier = 0.0025;
double STZScrollMomentumToZoomAttenuation = 0.8;
double STZScrollMinMomentumMagnification = 0.001;


typedef enum {
    WheelFree,  //  Not in a wheel session.
    WheelToScroll,
    WheelToScrollMomentum,
    WheelToZoom,
} WheelState;


typedef enum {
    UnifiedPhaseMayBegin,
    UnifiedPhaseBegan,
    UnifiedPhaseChanged,
    UnifiedPhaseEnded,
    UnifiedPhaseCancelled,
} UnifiedPhase;


typedef enum {
    EventContinueTransform,
    EventStopTransform,
    EventDiscard,
} EventAction;


static char const *nameOfUnifiedPhase(UnifiedPhase phase) {
    switch (phase) {
    case UnifiedPhaseBegan:     return "began";
    case UnifiedPhaseChanged:   return "changed";
    case UnifiedPhaseEnded:     return "ended";
    case UnifiedPhaseCancelled: return "cancelled";
    case UnifiedPhaseMayBegin:  return "may begin";
    }
}


static void getUnifiedScrollPhase(CGEventRef event, UnifiedPhase *outPhase, bool *outByMomentum) {
    CGScrollPhase sPhase = (CGScrollPhase)CGEventGetIntegerValueField(event, kCGScrollWheelEventScrollPhase);
    CGMomentumScrollPhase pPhase = (CGMomentumScrollPhase)CGEventGetIntegerValueField(event, kCGScrollWheelEventMomentumPhase);

    if (pPhase == kCGMomentumScrollPhaseNone) {
        *outByMomentum = false;

        switch (sPhase) {
        case kCGScrollPhaseBegan:       *outPhase = UnifiedPhaseBegan; break;
        case kCGScrollPhaseChanged:     *outPhase = UnifiedPhaseChanged; break;
        case kCGScrollPhaseEnded:       *outPhase = UnifiedPhaseEnded; break;
        case kCGScrollPhaseCancelled:   *outPhase = UnifiedPhaseCancelled; break;
        case kCGScrollPhaseMayBegin:    *outPhase = UnifiedPhaseMayBegin; break;
        default:
            STZUnknownEnumCase("CGScrollPhase", sPhase);
            *outPhase = UnifiedPhaseChanged; break;
        }

    } else {
        *outByMomentum = true;

        switch (pPhase) {
        case kCGMomentumScrollPhaseBegin:       *outPhase = UnifiedPhaseBegan; break;
        case kCGMomentumScrollPhaseContinue:    *outPhase = UnifiedPhaseChanged; break;
        case kCGMomentumScrollPhaseEnd:         *outPhase = UnifiedPhaseEnded; break;
        default:
            STZUnknownEnumCase("CGMomentumScrollPhase", pPhase);
            *outPhase = UnifiedPhaseChanged; break;
        }
    }
}


static void adaptScrollWheelEvent(CGEventRef event, UnifiedPhase phase) {
    CGScrollPhase sPhase;

    switch (phase) {
    case UnifiedPhaseMayBegin:  sPhase = kCGScrollPhaseMayBegin; break;
    case UnifiedPhaseBegan:     sPhase = kCGScrollPhaseBegan; break;
    case UnifiedPhaseChanged:   sPhase = kCGScrollPhaseChanged; break;
    case UnifiedPhaseEnded:     sPhase = kCGScrollPhaseEnded; break;
    case UnifiedPhaseCancelled: sPhase = kCGScrollPhaseCancelled; break;
    }

    CGEventSetIntegerValueField(event, kCGScrollWheelEventScrollPhase, sPhase);
    CGEventSetIntegerValueField(event, kCGScrollWheelEventMomentumPhase, kCGMomentumScrollPhaseNone);
}


static void adaptMomentumScrollWheelEvent(CGEventRef event, UnifiedPhase phase) {
    CGMomentumScrollPhase pPhase;

    switch (phase) {
    case UnifiedPhaseMayBegin:
    case UnifiedPhaseBegan:     pPhase = kCGMomentumScrollPhaseBegin; break;
    case UnifiedPhaseChanged:   pPhase = kCGMomentumScrollPhaseContinue; break;
    case UnifiedPhaseEnded:
    case UnifiedPhaseCancelled: pPhase = kCGMomentumScrollPhaseEnd; break;
    }

    CGEventSetIntegerValueField(event, kCGScrollWheelEventScrollPhase, 0);
    CGEventSetIntegerValueField(event, kCGScrollWheelEventMomentumPhase, pPhase);
}


static CGEventRef createScrollWheelEvent(CGEventRef source) {
    CGEventRef event = CGEventCreate(NULL);

    CGEventSetType(event, kCGEventScrollWheel);
    CGEventSetFlags(event, CGEventGetFlags(event));
    CGEventSetLocation(event, CGEventGetLocation(event));
    CGEventSetTimestamp(event, CGEventGetTimestamp(event));

    return event;
}


static CGEventRef createZoomGestureEvent(CGEventRef source, UnifiedPhase phase, double scale) {
    CGGesturePhase gPhase;

    switch (phase) {
    case UnifiedPhaseMayBegin:  gPhase = kCGGesturePhaseMayBegin; break;
    case UnifiedPhaseBegan:     gPhase = kCGGesturePhaseBegan; break;
    case UnifiedPhaseChanged:   gPhase = kCGGesturePhaseChanged; break;
    case UnifiedPhaseEnded:     gPhase = kCGGesturePhaseEnded; break;
    case UnifiedPhaseCancelled: gPhase = kCGGesturePhaseCancelled; break;
    }

    CGEventRef event = CGEventCreate(NULL);

    CGEventSetType(event, kCGEventGesture);
    CGEventSetFlags(event, CGEventGetFlags(event));
    CGEventSetLocation(event, CGEventGetLocation(event));
    CGEventSetTimestamp(event, CGEventGetTimestamp(event));

    CGEventSetIntegerValueField(event, kCGGestureEventHIDType, kIOHIDEventTypeZoom);
    CGEventSetIntegerValueField(event, kCGGestureEventPhase, gPhase);
    CGEventSetDoubleValueField(event, kCGGestureEventZoomValue, scale);

    return event;
}


static void getConvertedZoomPhase(CGEventRef event, CGEventTimestamp *momentumStart, UnifiedPhase *outPhase, double *outScale) {
    IOHIDEventRef hidEvent = CGEventCopyIOHIDEvent(event);
    int signum = IOHIDEventGetFloatValue(hidEvent, kIOHIDEventFieldScrollY) < 0 ? -1 : 1;
    CFRelease(hidEvent);

    double delta = signum * labs(CGEventGetIntegerValueField(event, kCGScrollWheelEventPointDeltaAxis1));

    bool byMomentum;
    getUnifiedScrollPhase(event, outPhase, &byMomentum);

    switch (*outPhase) {
    case UnifiedPhaseEnded:
    case UnifiedPhaseCancelled:
        *outScale = 0;
        break;

    case UnifiedPhaseMayBegin:
    case UnifiedPhaseBegan:
        *outScale = delta * STZScrollToZoomMagnifier;

        if (byMomentum) {
            *momentumStart = CGEventGetTimestamp(event);

            if (STZScrollMomentumToZoomAttenuation == 1) {
                *outPhase = UnifiedPhaseEnded;
                *outScale = 0;
            }
        }

        break;

    case UnifiedPhaseChanged:
        *outScale = delta * STZScrollToZoomMagnifier;

        if (byMomentum) {
            double k = 1 - STZScrollMomentumToZoomAttenuation;
            double dt = (CGEventGetTimestamp(event) - *momentumStart) * 1.0e-9;
            *outScale *= k != 0 ? pow(k, dt / k) : 0;

            if (fabs(*outScale) < STZScrollMinMomentumMagnification) {
                *outPhase = UnifiedPhaseEnded;
                *outScale = 0;
            }
        }
    }
}


static CGEventRef createWheelSessionEndEvent(WheelState session, CGEventRef source) {
    CGEventRef event = NULL;

    switch (session) {
    case WheelFree:
        break;

    case WheelToScroll:
        event = createScrollWheelEvent(event);
        adaptScrollWheelEvent(event, UnifiedPhaseEnded);
        break;

    case WheelToScrollMomentum:
        event = createScrollWheelEvent(event);
        adaptMomentumScrollWheelEvent(event, UnifiedPhaseEnded);
        break;

    case WheelToZoom:
        event = createZoomGestureEvent(source, UnifiedPhaseEnded, 0);
        break;
    }

    return event;
}


static void updateWheelSession(WheelState *session, WheelState wheel, UnifiedPhase phase, double data,
                               CGEventRef event, CGEventRef *outEvent, EventAction *action) {
    assert(wheel != WheelFree);

    //  free -> some that terminates: ignore the event
    //  free -> some not terminating: set phase to begin, send the event with phase begin
    //  some -> same that terminates: set phase to free, send the event
    //  some -> same not terminating: send the event with phase changed
    //  some -> any other: set phase to free, send the event with phase cancelled
    //  Additionally, coarse scroll won’t create a wheel session.

    //  Gesture may be broken if we send `mayBegin` events.
    if (wheel != WheelToScroll && phase == UnifiedPhaseMayBegin) {
        *action = EventDiscard;
        return;
    }

    //  If coarse, we assume phase is `changed` and scroll momentum is impossible.
    if (wheel == WheelToScroll && !CGEventGetIntegerValueField(event, kCGScrollWheelEventIsContinuous)) {
        WheelState session_ = *session;
        *session = WheelFree;

        switch (session_) {
        case WheelFree:
            *action = EventContinueTransform;
            return;

        case WheelToScroll:
            adaptScrollWheelEvent(event, UnifiedPhaseEnded);
            *action = EventContinueTransform;
            return;

        case WheelToScrollMomentum:
            adaptMomentumScrollWheelEvent(event, UnifiedPhaseEnded);
            *action = EventContinueTransform;
            return;

        case WheelToZoom:
            *outEvent = createZoomGestureEvent(event, UnifiedPhaseCancelled, 0);
            *action = EventStopTransform;
            return;
        }
    }

    //  Incompatible sessions, end previous session and discard current session.
    if (*session != WheelFree && *session != wheel) {
        WheelState session_ = *session;
        *session = WheelFree;
        *action = EventStopTransform;

        switch (session_) {
        case WheelFree:
            __builtin_unreachable();

        case WheelToScroll:
            adaptScrollWheelEvent(event, UnifiedPhaseCancelled);
            return;

        case WheelToScrollMomentum:
            adaptMomentumScrollWheelEvent(event, UnifiedPhaseCancelled);
            return;

        case WheelToZoom:
            *outEvent = createZoomGestureEvent(event, UnifiedPhaseCancelled, 0);
            return;
        }
    }

    bool misphase = false;

    //  If no wheel session, start a session, or do nothing if the event terminates.
    if (*session == WheelFree) {
        switch (phase) {
        case UnifiedPhaseMayBegin:
        case UnifiedPhaseBegan:
            *session = wheel;
            break;

        case UnifiedPhaseChanged:
            *session = wheel;
            phase = UnifiedPhaseBegan;
            misphase = true;
            break;

        case UnifiedPhaseEnded:
        case UnifiedPhaseCancelled:
            *action = EventDiscard;
            return;
        }

    } else {
        switch (phase) {
        case UnifiedPhaseMayBegin:
        case UnifiedPhaseBegan:
            //  phase = UnifiedPhaseChanged;
            //  misphase = true;
            break;

        case UnifiedPhaseChanged:
            break;

        case UnifiedPhaseEnded:
        case UnifiedPhaseCancelled:
            *session = WheelFree;
            break;
        }
    }

    switch (wheel) {
    case WheelFree:
        __builtin_unreachable();

    case WheelToScroll:
        if (misphase) {adaptScrollWheelEvent(event, phase);}
        *action = EventContinueTransform;
        return;

    case WheelToScrollMomentum:
        if (misphase) {adaptMomentumScrollWheelEvent(event, phase);}
        *action = EventContinueTransform;
        return;

    case WheelToZoom:
        *outEvent = createZoomGestureEvent(event, phase, data);
        *action = EventStopTransform;
        return;
    }
}

//  MARK: -


static WheelState wheelSession = WheelFree;
static CGEventTimestamp momentumStart = 0;


static void transformScrollToZoom(CGEventRef event, CGEventRef *outEvent, EventAction *action) {
    WheelState wheel;
    UnifiedPhase phase;
    double data = 0;

    bool flagsIn = (CGEventGetFlags(event) & STZScrollToZoomFlags) == STZScrollToZoomFlags;

    if (flagsIn) {
        getConvertedZoomPhase(event, &momentumStart, &phase, &data);
        wheel = WheelToZoom;

    } else {
        bool byMomentum;
        getUnifiedScrollPhase(event, &phase, &byMomentum);
        wheel = byMomentum ? WheelToScrollMomentum : WheelToScroll;
    }

    updateWheelSession(&wheelSession, wheel, phase, data, event, outEvent, action);
}


static void releaseZoomSession(CGEventRef event) {
    bool flagsIn = (CGEventGetFlags(event) & STZScrollToZoomFlags) == STZScrollToZoomFlags;
    if (flagsIn) {return;}

    switch (wheelSession) {
    case WheelFree:
    case WheelToScroll:
        return;

    case WheelToScrollMomentum:
    case WheelToZoom:
        break;
    }

    CGEventRef endEvent = createWheelSessionEndEvent(wheelSession, event);
    CFRunLoopPerformBlock(CFRunLoopGetCurrent(), kCFRunLoopCommonModes, ^{
        //  The created event is not some we would tap, so update the session directly.
        wheelSession = WheelFree;
        CGEventPost(kCGSessionEventTap, endEvent);
        CFRelease(endEvent);
    });
}


static CGEventRef transformEvent(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *userInfo) {
    if (type == kCGEventScrollWheel) {
        CGEventRef outEvent = NULL;
        EventAction action = EventContinueTransform;
        transformScrollToZoom(event, &outEvent, &action);

        switch (action) {
        case EventContinueTransform:    return outEvent ?: event;
        case EventStopTransform:        return outEvent ?: event;
        case EventDiscard:              return NULL;
        }
    }

    if (type == kCGEventFlagsChanged) {
        releaseZoomSession(event);
    }

    return event;
}


static CFMachPortRef eventTap = NULL;
static CFRunLoopSourceRef eventTapSource = NULL;


bool STZGetEventTapEnabled(void) {
    return eventTap != NULL;
}


bool STZSetEventTapEnabled(bool enable) {
    bool enabled = eventTap != NULL;
    if (enable == enabled) {return true;}

    if (!enable) {
        CGEventTapEnable(eventTap, false);
        CFRelease(eventTap);
        eventTap = NULL;

        CFRunLoopRemoveSource(CFRunLoopGetMain(), eventTapSource, kCFRunLoopCommonModes);
        CFRelease(eventTapSource);
        eventTapSource = NULL;
        return true;

    } else {
        bool trusted = AXIsProcessTrusted();
        if (!trusted) {return false;}

        CGEventMask events = (1 << kCGEventScrollWheel) | (1 << kCGEventFlagsChanged);
        eventTap = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault, events, transformEvent, NULL);
        if (!eventTap) {return false;}
        
        eventTapSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, eventTap, 0);
        CFRunLoopAddSource(CFRunLoopGetMain(), eventTapSource, kCFRunLoopCommonModes);

        CGEventTapEnable(eventTap, true);
        return true;
    }
}
