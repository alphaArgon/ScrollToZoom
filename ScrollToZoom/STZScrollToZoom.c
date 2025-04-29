/*
 *  STZScrollToZoom.c
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/1/24.
 *  Copyright © 2025 alphaArgon.
 */

#import "STZScrollToZoom.h"
#import "STZSettings.h"
#import "STZDotDashDrag.h"
#import "STZWheelSession.h"
#import "STZProcessManager.h"
#import <ApplicationServices/ApplicationServices.h>
#import "CGEventSPI.h"


//  A main goal of *ScrollToZoom* is to integrate seamlessly with other mouse-optimizing apps. To
//  forbid the interference from other event taps, we need to record some hardware data before any
//  other tap, and emit the modified event after all other taps. However, this approach is
//  challenging because the order in which apps open can invert the intended control flow. Say we
//  already created the tap and another app is opened later, we will miss its modifications.
//  Therefore, we dynamically register and remove taps as needed.
//
//  In this file, we call a `HIDEventTap` a hard one and an `AnnotatedSessionEventTap` a soft one.
//  It’s known that *LinearMouse* intercepts events hard-ly and *Mos* intercepts soft-ly.


//  MARK: - Event Tap


typedef struct {
    CFMachPortRef       port;
    CFRunLoopSourceRef  source;
    bool                enabled;

    struct {
        CGEventMask         events;
        CGEventTapLocation  location;
        CGEventTapPlacement placement;
        CGEventTapOptions   options;
        CGEventTapCallBack  callback;
        bool                initialEnabled;
    } const;
} STZEventTap;


static bool STZEventTapGetRegistered(STZEventTap *eventTap) {
    return eventTap->port != NULL;
}

static bool STZEventTapSetRegistered(STZEventTap *eventTap, bool flag) {
    if (flag == (eventTap->port != NULL)) {return true;}

    if (!flag) {
        CFRunLoopRemoveSource(CFRunLoopGetMain(), eventTap->source, kCFRunLoopCommonModes);
        CGEventTapEnable(eventTap->port, false);
        CFRelease(eventTap->source);
        CFRelease(eventTap->port);

        eventTap->port = NULL;
        eventTap->source = NULL;
        eventTap->enabled = false;

    } else {
        if (!AXIsProcessTrusted()) {return false;}

        CFMachPortRef port = CGEventTapCreate(eventTap->location, eventTap->placement,
                                              eventTap->options, eventTap->events,
                                              eventTap->callback, NULL);
        if (port == NULL) {return false;}

        eventTap->port = port;
        eventTap->source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, port, 0);
        eventTap->enabled = eventTap->initialEnabled;

        CGEventTapEnable(eventTap->port, eventTap->enabled);
        CFRunLoopAddSource(CFRunLoopGetMain(), eventTap->source, kCFRunLoopCommonModes);
    }

    return true;
}


static bool STZEventTapGetEnabled(STZEventTap *eventTap) {
    return eventTap->enabled;
}

static void STZEventTapSetEnabled(STZEventTap *eventTap, bool flag) {
    if (flag == eventTap->enabled) {return;}
    eventTap->enabled = flag;
    CGEventTapEnable(eventTap->port, flag);
}


#define DECLARE_EVENT_TAP(prefix, events, location, placement, options, initialEnabled)             \
static CGEventRef prefix##Callback(CGEventTapProxy, CGEventType, CGEventRef, void *);               \
static STZEventTap prefix##Tap = {NULL, NULL, false,                                                \
                                  events, location, placement, options,                             \
                                  prefix##Callback, initialEnabled};                                \

static void dotDashDragActivationCallback(uint64_t registryID, bool active, void *refcon);

DECLARE_EVENT_TAP(hardFlagsChanged, (1 << kCGEventFlagsChanged) | (1 << kCGEventOtherMouseDown) | (1 << kCGEventOtherMouseUp),
                  kCGHIDEventTap, kCGHeadInsertEventTap,
                  kCGEventTapOptionListenOnly, true);

//  This tap might be disabled if controlled exclusively.
DECLARE_EVENT_TAP(hardScrollWheel, 1 << kCGEventScrollWheel,
                  kCGHIDEventTap, kCGHeadInsertEventTap,
                  kCGEventTapOptionDefault, false);

//  One and only one of the following two should be enabled.

DECLARE_EVENT_TAP(passiveScrollWheel, 1 << kCGEventScrollWheel,
                  kCGHIDEventTap, kCGHeadInsertEventTap,
                  kCGEventTapOptionListenOnly, true);

DECLARE_EVENT_TAP(mutatingScrollWheel, 1 << kCGEventScrollWheel,
                  kCGAnnotatedSessionEventTap, kCGTailAppendEventTap,
                  kCGEventTapOptionDefault, false);

#undef DECLARE_EVENT_TAP


//  MARK: - Contexts


/// Whether `STZScrollToZoomFlags` are pressed.
static bool globalFlagsIn = false;


typedef struct {
    STZWheelSession     session;
    bool                sessionEnded;

    bool                dotDashDragging;
    bool                recognizedByDotDash;    ///< Should be manually reset when phase ends.
    bool                recognizedByFlags;      ///< Automatically reset when flags changes.
    STZEventTapOptions  byFlagsOptions;

    CGPoint             lockedMouseLocation;  ///< Updated when recognized.

    int                 hardignum;
    CGEventTimestamp    momentumStart;
    uint64_t            timerToken;
} STZWheelContext;

#define kSTZWheelContextEmpty (STZWheelContext){kSTZWheelSessionEmpty, false, false, false, false, 0, CGPointZero, 0, 0, 0}

static STZCScanCache _wheelContexts = kSTZCScanCacheEmptyForType(STZWheelContext);


static STZWheelContext *wheelContextAt(uint64_t registryID) {
    if (!STZCScanCacheIsInUse(&_wheelContexts)) {
        CGEventTimestamp const DATA_LIFETIME = 300 * NSEC_PER_SEC;  //  5 minutes.
        CGEventTimestamp const CHECK_INTERVAL = 60 * NSEC_PER_SEC;  //  1 minute.
        STZCScanCacheSetDataLifetime(&_wheelContexts, DATA_LIFETIME, CHECK_INTERVAL);
    }

    if (STZIsLoggingEnabled()) {
        //  The cache will still be cleaned up without this call. Only for logging.
        STZCScanCacheCheckExpired(&_wheelContexts, false);

        STZCacheIterator iterator;
        STZCScanCacheIteratorInitialize(&iterator, &_wheelContexts, true);

        uint64_t registryID;
        bool expired;
        while ((STZCScanCacheIteratorGetNextData(&iterator, &registryID, &expired))) {
            if (expired) {
                STZDebugLog("Marked context expired for [%llx]", registryID);
            }
        }
    }

    STZCScanCacheResult result;
    STZWheelContext *context = STZCScanCacheGetDataForIdentifier(&_wheelContexts, registryID, true, &result);

    switch (result) {
    case kSTZCScanCacheFound:
        break;

    case kSTZCScanCacheExpiredRestored:
        STZDebugLog("Recovered expired context for [%llx]", registryID);
        break;

    case kSTZCScanCacheNewCreated:
    case kSTZCScanCacheExpiredReused:
        STZDebugLog("Created context for [%llx]", registryID);
        *context = kSTZWheelContextEmpty;
        break;
    }

    return context;
}


static STZWheelContext *wheelContextFor(CGEventRef event) {
    uint64_t registryID = CGEventGetRegistryID(event);
    return wheelContextAt(registryID);
}


static void removeAllWheelContexts(void) {
    STZCScanCacheRemoveAll(&_wheelContexts);
}


/// Enumerates all wheel contexts.
#define forEachWheelContext(var)                                                                    \
    STZCacheIterator __iterator;                                                                    \
    STZCScanCacheIteratorInitialize(&__iterator, &_wheelContexts, false);                           \
                                                                                                    \
    STZWheelContext *var;                                                                           \
    while ((var = STZCScanCacheIteratorGetNextData(&__iterator, NULL, NULL)))                       \


//  MARK: - Tap Monitoring


static bool         needsReinsertTaps   = false;
static signed int   expectedTapCount    = 0;


static void anyEventTapAdded(CFNotificationCenterRef center, void *observer,
                             CFNotificationName name, const void *object,
                             CFDictionaryRef userInfo) {
    uint32_t count;
    CGGetEventTapList(0, NULL, &count);

    if (count > expectedTapCount) {
        STZDebugLog("Foreign event tap added");
        needsReinsertTaps = true;
    }
}

static void anyEventTapRemoved(CFNotificationCenterRef center, void *observer,
                               CFNotificationName name, const void *object,
                               CFDictionaryRef userInfo) {
    expectedTapCount -= 1;  //  OK to be negative.
}


bool STZIsEventTapExclusive(void) {
    uint32_t count;
    CGGetEventTapList(0, NULL, &count);

    CGEventTapInformation *infos = malloc(sizeof(CGEventTapInformation) * count);
    CGGetEventTapList(count, infos, &count);

    for (uint32_t i = 0; i < count; ++i) {
        if (!(infos[i].eventsOfInterest & (1 << kCGEventScrollWheel))) {continue;}
        if (infos[i].options & kCGEventTapOptionListenOnly) {continue;}
        if (infos[i].processBeingTapped != 0) {continue;}
        if (infos[i].tappingProcess == getpid()) {continue;}

        CFStringRef bundleID = STZGetBundleIdentifierForProcessID(infos[i].tappingProcess);
        if (!bundleID || CFStringHasPrefix(bundleID, CFSTR("com.apple."))) {continue;}

        free(infos);
        return false;
    }

    free(infos);
    return true;
}


static bool setAllEventTapsRegistered(bool flag, bool useHardScrollWheel) {
    return STZEventTapSetRegistered(&hardFlagsChangedTap, flag)
        && STZEventTapSetRegistered(&hardScrollWheelTap, flag && useHardScrollWheel)
        && STZEventTapSetRegistered(&passiveScrollWheelTap, flag)
        && STZEventTapSetRegistered(&mutatingScrollWheelTap, flag);
}


static bool registerEventTapsOrCleanUp(void) {
    bool exclusive = STZIsEventTapExclusive();

    if (!setAllEventTapsRegistered(true, !exclusive)) {
        setAllEventTapsRegistered(false, false);
        STZSetListeningMultitouchDevices(false);
        removeAllWheelContexts();
        globalFlagsIn = false;
        return false;
    }

    uint32_t count;
    CGGetEventTapList(0, NULL, &count);

    needsReinsertTaps = false;
    expectedTapCount = count;
    return true;
}


static void dotDashEnabledToggled(CFNotificationCenterRef center, void *observer,
                                  CFNotificationName name, const void *object,
                                  CFDictionaryRef userInfo) {
    if (STZGetDotDashDragToZoomEnabled()) {
        STZSetListeningMultitouchDevices(true);
    } else {
        STZSetListeningMultitouchDevices(false);
        forEachWheelContext(context) {
            context->dotDashDragging = false;
        }
    }
}


bool STZGetScrollToZoomEnabled(void) {
    return STZEventTapGetRegistered(&hardFlagsChangedTap);
}


bool STZSetScrollToZoomEnabled(bool enable) {
    if (enable == STZEventTapGetRegistered(&hardFlagsChangedTap)) {
        return true;
    }

    if (!enable) {
        setAllEventTapsRegistered(false, false);
        STZSetListeningMultitouchDevices(false);
        removeAllWheelContexts();
        globalFlagsIn = false;
        STZDebugLog("Event tap unregistered");
        return true;

    } else if (!registerEventTapsOrCleanUp()) {
        STZDebugLog("Event tap failed to register.");
        return false;

    } else {
        static bool listened = false;
        if (!listened) {
            //  Callbacks are invoked on the the main thread. We use only one thread.
            CFNotificationCenterAddObserver(CFNotificationCenterGetDarwinNotifyCenter(),
                                            NULL, anyEventTapAdded,
                                            CFSTR(kCGNotifyEventTapAdded), NULL,
                                            0 /* ignored for Darwin center */);
            CFNotificationCenterAddObserver(CFNotificationCenterGetDarwinNotifyCenter(),
                                            NULL, anyEventTapRemoved,
                                            CFSTR(kCGNotifyEventTapRemoved), NULL,
                                            0 /* ignored for Darwin center */);
            CFNotificationCenterAddObserver(CFNotificationCenterGetLocalCenter(),
                                            NULL, dotDashEnabledToggled,
                                            kSTZDotDashDragToZoomEnabledDidChangeNotificationName, NULL,
                                            CFNotificationSuspensionBehaviorDeliverImmediately);
            STZDotDashDragObserveActivation(dotDashDragActivationCallback, NULL);
        };

        if (STZGetDotDashDragToZoomEnabled()) {
            STZSetListeningMultitouchDevices(true);
        }

        STZDebugLog("Event tap registered");
        return true;
    }
}


static void reinsertTapsIfNeeded(void) {
    assert(STZEventTapGetRegistered(&hardFlagsChangedTap));
    if (!needsReinsertTaps) {return;}

    STZDebugLog("Re-inset event tap due to environment change");
    setAllEventTapsRegistered(false, false);

    if (!registerEventTapsOrCleanUp()) {
        STZDebugLog("Event tap failed to re-inset.");
    }
}


static void eventTapTimeout(void) {
    setAllEventTapsRegistered(false, false);
    STZSetListeningMultitouchDevices(false);
    STZDebugLog("Event tap disabled due to timeout");
}


static void postAndReleaseEvent(CGEventRef CF_CONSUMED event, CGEventTapLocation location, bool async) {
    if (!async) {
        CGEventPost(location, event);
        CFRelease(event);
        return;
    }

    //  This delay is not elegant. However, without this WebKit may ignore the evnt.
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.02 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        CGEventSetTimestamp(event, CGEventTimestampNow());
        CGEventPost(location, event);
        CFRelease(event);
    });
}


static void replaceEventByConsumingEvents(CGEventRef CF_CONSUMED const *events) {
    if (events[0]) {
        STZDebugLogEvent("\tEvent replaced by", events[0]);
        postAndReleaseEvent(events[0], kCGSessionEventTap, false);
    } else {
        STZDebugLog("\tEvent discarded");
    }

    if (events[1]) {
        STZDebugLogEvent("\tEvent replaced by", events[1]);
        postAndReleaseEvent(events[1], kCGSessionEventTap, true);
    }
}


static void releaseSessionFromEvent(STZWheelSession *__nonnull session, CGEventRef __nonnull event) {
    CGEventRef extraEvent = NULL;
    STZWheelSession oldSession = *session;
    STZWheelSessionDiscard(session, event, &extraEvent, NULL);
    STZDebugLogSessionChange("\tSession released", oldSession, *session);

    if (extraEvent) {
        STZDebugLogEvent("\tPost additional", extraEvent);
        postAndReleaseEvent(extraEvent, kCGSessionEventTap, false);
    }
}


static void beginEventMutations(void) {
    if (STZEventTapGetEnabled(&mutatingScrollWheelTap)) {return;}

    if (STZEventTapGetRegistered(&hardScrollWheelTap)) {
        STZEventTapSetEnabled(&hardScrollWheelTap, true);
        STZDebugLog("Activate hard scroll wheel listening");
    }

    STZEventTapSetEnabled(&passiveScrollWheelTap, false);
    STZEventTapSetEnabled(&mutatingScrollWheelTap, true);
    STZDebugLog("Switch to mutating scroll wheel tap");
}


static bool tryToEndEventMutations(void) {
    if (STZEventTapGetEnabled(&passiveScrollWheelTap)) {return true;}

    forEachWheelContext(context) {
        if (context->recognizedByFlags || context->recognizedByDotDash || !context->sessionEnded) {
            return false;
        }
    }

    if (STZEventTapGetRegistered(&hardScrollWheelTap)) {
        STZEventTapSetEnabled(&hardScrollWheelTap, false);
        STZDebugLog("Deactivate hard scroll wheel listening");
    }

    STZEventTapSetEnabled(&passiveScrollWheelTap, true);
    STZEventTapSetEnabled(&mutatingScrollWheelTap, false);
    STZDebugLog("Switch to passive scroll wheel tap");
    return true;
}


static void dotDashDragActivationCallback(uint64_t registryID, bool active, void *refcon) {
    reinsertTapsIfNeeded();
    wheelContextAt(registryID)->dotDashDragging = active;

    if (active) {
        STZDebugLog("Dot dash drag activated for [%llx]", registryID);
        beginEventMutations();
    } else {
        STZDebugLog("Dot dash drag deactivated for [%llx]", registryID);
        tryToEndEventMutations();
    }
}


static CGEventRef hardFlagsChangedCallback(CGEventTapProxy proxy, CGEventType eventType, CGEventRef event, void *userInfo) {
    bool flagsIn;

    switch (eventType) {
    case kCGEventTapDisabledByTimeout:      eventTapTimeout(); CF_FALLTHROUGH;
    case kCGEventTapDisabledByUserInput:    return NULL;

    case kCGEventFlagsChanged:
        if (!(STZGetScrollToZoomFlags() & kSTZModifiersMask)) {return event;}
        flagsIn = STZValidateFlags(CGEventGetFlags(event) & kSTZModifiersMask, NULL) == STZGetScrollToZoomFlags();
        break;

    case kCGEventOtherMouseDown:
        if (!(STZGetScrollToZoomFlags() & kSTZMouseButtonsMask)) {return event;}
        if (CGEventGetIntegerValueField(event, kCGMouseEventButtonNumber) != STZGetScrollToZoomFlags()) {return event;}
        flagsIn = true;
        break;

    case kCGEventOtherMouseUp:
        if (!(STZGetScrollToZoomFlags() & kSTZMouseButtonsMask)) {return event;}
        if (CGEventGetIntegerValueField(event, kCGMouseEventButtonNumber) != STZGetScrollToZoomFlags()) {return event;}
        flagsIn = false;
        break;

    default: assert(false); break;
    }

    STZDebugLogEvent("Received hard", event);

    if (globalFlagsIn != flagsIn) {
        globalFlagsIn = flagsIn;

        reinsertTapsIfNeeded();

        forEachWheelContext(context) {
            if (!globalFlagsIn && context->recognizedByFlags && !context->recognizedByDotDash) {
                releaseSessionFromEvent(&context->session, event);
            }

            context->recognizedByFlags = false;
            context->timerToken += 1;
        }

        if (flagsIn) {
            beginEventMutations();
        } else {
            tryToEndEventMutations();
        }
    }

    return event;
}


static CGEventRef hardScrollWheelCallback(CGEventTapProxy proxy, CGEventType eventType, CGEventRef event, void *userInfo) {
    switch (eventType) {
    case kCGEventTapDisabledByTimeout:      eventTapTimeout(); CF_FALLTHROUGH;
    case kCGEventTapDisabledByUserInput:    return NULL;
    default: assert(eventType == kCGEventScrollWheel); break;
    }

    STZDebugLogEvent("Received hard", event);

    //  TODO: We should find a unique identifier for an event.
    //  This function breaks `kCGEventTapOptionListenOnly`, which may increase the CPU usage.

    //  On computers with lower performace, newer hard wheel events might be sent to the tap
    //  before the older soft events. We can’t use an accumulated value to record the signum.

    int signum = STZGetDeltaSignumForScrollWheelEvent(event);
    wheelContextFor(event)->hardignum = signum;
    STZMarkDeltaSignumForEvent(event, signum);

    return event;
}


static CGEventRef passiveScrollWheelCallback(CGEventTapProxy proxy, CGEventType eventType, CGEventRef event, void *userInfo) {
    switch (eventType) {
    case kCGEventTapDisabledByTimeout:      eventTapTimeout(); CF_FALLTHROUGH;
    case kCGEventTapDisabledByUserInput:    return NULL;
    default: assert(eventType == kCGEventScrollWheel); break;
    }

    STZDebugLogEvent("Received readonly", event);

    STZWheelContext *context = wheelContextFor(event);
    context->timerToken += 1;

    bool byMomentum;
    STZPhase phase = STZGetPhaseFromScrollWheelEvent(event, &byMomentum);

    STZWheelType type;
    type = byMomentum ? kSTZWheelToScrollMomentum : kSTZWheelToScroll;

    STZWheelSessionAssign(&context->session, type, phase);
    return event;
}


static bool isWheelContextRecognizedActivated(STZWheelContext *context) {
    if (context->recognizedByDotDash) {return true;}
    if (context->byFlagsOptions & kSTZEventTapDisabled) {return false;}
    return context->recognizedByFlags;
}


static CGEventRef mutatingScrollWheelCallback(CGEventTapProxy proxy, CGEventType eventType, CGEventRef event, void *userInfo) {
    switch (eventType) {
    case kCGEventTapDisabledByTimeout:      eventTapTimeout(); CF_FALLTHROUGH;
    case kCGEventTapDisabledByUserInput:    return NULL;
    default: assert(eventType == kCGEventScrollWheel); break;
    }

    STZDebugLogEvent("Received mutable", event);

    STZWheelContext *context = wheelContextFor(event);
    context->timerToken += 1;

    bool byMomentum;
    STZPhase eventPhase = STZGetPhaseFromScrollWheelEvent(event, &byMomentum);
    STZPhase desiredPhase = eventPhase;

    bool wasActivated = isWheelContextRecognizedActivated(context);

    if (!context->recognizedByFlags && globalFlagsIn) {
        context->recognizedByFlags = true;

        pid_t pid = (int32_t)CGEventGetIntegerValueField(event, kCGEventTargetUnixProcessID);
        CFStringRef bundleID = STZGetBundleIdentifierForProcessID(pid);
        context->byFlagsOptions = STZGetEventTapOptionsForBundleIdentifier(bundleID);
    }

    if (eventPhase == kSTZPhaseBegan && !byMomentum) {
        if (context->dotDashDragging) {
            uint64_t registryID = CGEventGetRegistryID(event);
            context->recognizedByDotDash = STZDotDashDragIsActiveWithinTimeout(registryID, 0.75 * NSEC_PER_SEC);
        } else {
            context->recognizedByDotDash = false;
        }
    }

    bool isActivated = isWheelContextRecognizedActivated(context);

    if (isActivated && !wasActivated) {
        context->lockedMouseLocation = CGEventGetLocation(event);
    }

    STZWheelType type;
    double data = 0;

    if (isActivated) {
        int signum;

        if (STZEventTapGetEnabled(&hardScrollWheelTap)) {
            if (!STZConsumeDeltaSignumForEvent(event, &signum)) {
                signum = context->hardignum;
                STZDebugLog("\tEvent is from unknown source");
            }
        } else {
            signum = STZGetDeltaSignumForScrollWheelEvent(event);
        }

        double delta = CGEventGetDoubleValueField(event, kCGScrollWheelEventPointDeltaAxis1);
        CGEventTimestamp timestamp = CGEventGetTimestamp(event);
        STZConvertZoomFromScrollWheel(&desiredPhase, byMomentum, signum, delta,
                                      timestamp, &context->momentumStart, &data);
        type = kSTZWheelToZoom;

    } else {
        type = byMomentum ? kSTZWheelToScrollMomentum : kSTZWheelToScroll;
    }

    STZWheelSession oldSession = context->session;
    bool oldSessionEnded = context->sessionEnded;
    bool oldRecognizedByDotDash = context->recognizedByDotDash;

    STZEventAction action;
    CGEventRef outEvents[2] = {NULL, NULL};
    STZWheelSessionUpdate(&context->session, type, desiredPhase, data, event, outEvents, &action);
    context->sessionEnded = STZWheelSessionIsEnded(&context->session, eventPhase);
    context->recognizedByDotDash = oldRecognizedByDotDash && eventPhase != kSTZPhaseEnded;

    switch (action) {
    case kSTZEventUnchanged:
        STZDebugLogSessionChange("\tSession changed", oldSession, context->session);
        tryToEndEventMutations();
        return event;

    case kSTZEventAdapted:
        STZDebugLogSessionChange("\tSession changed", oldSession, context->session);
        STZDebugLogEvent("\tEvent modified to", event);
        tryToEndEventMutations();
        return event;

    case kSTZEventReplaced:
        break;
    }

    //  The event is annotated; returning a different event may have no effect. Therefore all
    //  the out events will be newly posted and `NULL` is returnrd.

    bool excludeFlags = context->recognizedByFlags && (context->byFlagsOptions & kSTZEventTapExcludeFlags);

    for (int i = 0; i < 2; ++i) {
        CGEventRef event = outEvents[i];
        if (event) {
            CGEventSetLocation(event, context->lockedMouseLocation);

            if (excludeFlags) {
                CGEventFlags flags = CGEventGetFlags(event);
                flags &= ~STZGetScrollToZoomFlags();
                CGEventSetFlags(event, flags);
            }
        }
    }

    if (STZScrollWheelHasSuccessorEvent(eventPhase, byMomentum) != kSTZMaybe) {
        STZDebugLogSessionChange("\tSession changed", oldSession, context->session);
        replaceEventByConsumingEvents(outEvents);
        tryToEndEventMutations();

    } else if (context->session.state != kSTZWheelFree) {
        STZDebugLogSessionChange("\tSession changed", oldSession, context->session);
        replaceEventByConsumingEvents(outEvents);

        STZDebugLog("\tWaiting for more events before releasing wheel session");
        CFRetain(event);

        uint64_t token = context->timerToken;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.35 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            if (token != context->timerToken) {
                return CFRelease(event);
            }

            STZDebugLog("\tNo events received while awaiting");

            releaseSessionFromEvent(&context->session, event);
            context->sessionEnded = STZWheelSessionIsEnded(&context->session, eventPhase);

            tryToEndEventMutations();
            CFRelease(event);
        });

    } else {
        STZDebugLog("\tWaiting for more events before replacing the event");

        STZWheelSession newSession = context->session;
        bool newSessionEnded = context->sessionEnded;
        bool newRecognizedByDotDash = context->recognizedByDotDash;

        context->session = oldSession;
        context->sessionEnded = oldSessionEnded;
        context->recognizedByDotDash = oldRecognizedByDotDash;

        struct {
            CGEventRef first, last;
        } tupleEvents = {outEvents[0], outEvents[1]};

        uint64_t token = context->timerToken;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.05 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            if (token != context->timerToken) {
                if (tupleEvents.first) {CFRelease(tupleEvents.first);}
                if (tupleEvents.last) {CFRelease(tupleEvents.last);}
                return;
            }

            STZDebugLog("\tNo events received while awaiting");

            context->session = newSession;
            context->sessionEnded = newSessionEnded;
            context->recognizedByDotDash = newRecognizedByDotDash;

            STZDebugLogSessionChange("\tSession changed", oldSession, context->session);
            replaceEventByConsumingEvents((CGEventRef const *)&tupleEvents);
            tryToEndEventMutations();
        });
    }

    return NULL;
}
