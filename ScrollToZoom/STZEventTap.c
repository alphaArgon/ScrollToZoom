/*
 *  STZEventTap.c
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/1/24.
 *  Copyright © 2025 alphaArgon.
 */

#import "STZEventTap.h"
#import "STZWheelSession.h"
#import "STZProcessManager.h"
#import <ApplicationServices/ApplicationServices.h>
#import "CGEventSPI.h"


STZFlags _scrollToZoomFlags = kSTZModifierOption;
double _scrollToZoomMagnifier = 0.0025;
double _scrollMomentumToZoomAttenuation = 0.8;
double _scrollMinMomentumMagnification = 0.001;
CFMutableDictionaryRef _eventTapOptionsForApps = NULL;
CFMutableDictionaryRef _eventTapOptionsObjsForApps = NULL;


static CFStringRef const STZScrollToZoomFlagsKey = CFSTR("STZScrollToZoomFlags");
static CFStringRef const STZScrollToZoomMagnifierKey = CFSTR("STZScrollToZoomMagnifier");
static CFStringRef const STZScrollMomentumToZoomAttenuationKey = CFSTR("STZScrollMomentumToZoomAttenuation");
static CFStringRef const STZScrollMinMomentumMagnificationKey = CFSTR("STZScrollMinMomentumMagnification");
static CFStringRef const STZEventTapOptionsForAppsKey = CFSTR("STZEventTapOptionsForApps");


static double clamp(double x, double lo, double hi) {
    //  `NaN` gives average of `lo` and `hi`;
    if (x != x) {return (lo + hi) / 2;}
    return x < lo ? lo : x > hi ? hi : x;
}


STZFlags STZGetScrollToZoomFlags(void) {
    return _scrollToZoomFlags;
}

void STZSetScrollToZoomFlags(STZFlags flags) {
    CFStringRef desc;
    _scrollToZoomFlags = STZValidateFlags(flags, &desc);

    CFStringRef appID = CFBundleGetIdentifier(CFBundleGetMainBundle());
    CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberCFIndexType, &flags);
    CFPreferencesSetAppValue(STZScrollToZoomFlagsKey, number, appID);
    CFRelease(number);

    STZDebugLog("ScrollToZoomFlags set to %@", desc);
}


double STZGetScrollToZoomMagnifier(void) {
    return _scrollToZoomMagnifier;
}

void STZSetScrollToZoomMagnifier(double magnifier) {
    _scrollToZoomMagnifier = clamp(magnifier, -1, 1);

    CFStringRef appID = CFBundleGetIdentifier(CFBundleGetMainBundle());
    CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &magnifier);
    CFPreferencesSetAppValue(STZScrollToZoomMagnifierKey, number, appID);
    CFRelease(number);

    STZDebugLog("ScrollToZoomMagnifier set to %f", magnifier);
}


double STZGetScrollMomentumToZoomAttenuation(void) {
    return _scrollMomentumToZoomAttenuation;
}

void STZSetScrollMomentumToZoomAttenuation(double attenuation) {
    _scrollMomentumToZoomAttenuation = clamp(attenuation, 0, 1);

    CFStringRef appID = CFBundleGetIdentifier(CFBundleGetMainBundle());
    CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &attenuation);
    CFPreferencesSetAppValue(STZScrollMomentumToZoomAttenuationKey, number, appID);
    CFRelease(number);

    STZDebugLog("ScrollMomentumToZoomAttenuation set to %f", attenuation);
}


double STZGetScrollMinMomentumMagnification(void) {
    return _scrollMinMomentumMagnification;
}

void STZSetScrollMinMomentumMagnification(double minMagnification) {
    _scrollMinMomentumMagnification = clamp(minMagnification, 0, 1);

    CFStringRef appID = CFBundleGetIdentifier(CFBundleGetMainBundle());
    CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &minMagnification);
    CFPreferencesSetAppValue(STZScrollMinMomentumMagnificationKey, number, appID);
    CFRelease(number);

    STZDebugLog("ScrollMinMomentumMagnification set to %f", minMagnification);
}

STZEventTapOptions STZGetEventTapOptionsForBundleIdentifier(CFStringRef bundleID) {
    if (!bundleID) {return 0;}
    if (!_eventTapOptionsForApps) {return 0;}
    return (STZEventTapOptions)(uintptr_t)CFDictionaryGetValue(_eventTapOptionsForApps, bundleID);
}


void STZSetEventTapOptionsForBundleIdentifier(CFStringRef bundleID, STZEventTapOptions options) {
    if (!_eventTapOptionsForApps) {return;}

    if (options == (uintptr_t)CFDictionaryGetValue(_eventTapOptionsForApps, bundleID)) {
        return;
    }

    if (!options) {
        CFDictionaryRemoveValue(_eventTapOptionsForApps, bundleID);
        CFDictionaryRemoveValue(_eventTapOptionsObjsForApps, bundleID);

    } else {
        CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &options);
        CFDictionarySetValue(_eventTapOptionsForApps, bundleID, (void *)(uintptr_t)options);
        CFDictionarySetValue(_eventTapOptionsObjsForApps, bundleID, number);
        CFRelease(number);
    }

    CFStringRef appID = CFBundleGetIdentifier(CFBundleGetMainBundle());
    CFPreferencesSetAppValue(STZEventTapOptionsForAppsKey, _eventTapOptionsObjsForApps, appID);

    STZDebugLog("EventTapOptions set to %u for %@", options, bundleID);
}


CFDictionaryRef STZCopyAllEventTapOptions(void) {
    return CFDictionaryCreateCopy(kCFAllocatorDefault, _eventTapOptionsForApps);
}


void STZLoadArgumentsFromUserDefaults(void) {
    CFStringRef appID = CFBundleGetIdentifier(CFBundleGetMainBundle());

    CFIndex flags = CFPreferencesGetAppIntegerValue(STZScrollToZoomFlagsKey, appID, NULL);
    if (flags != 0) {
        _scrollToZoomFlags = (STZFlags)flags;  //  Validated later.
    }

    CFStringRef desc;
    _scrollToZoomFlags = STZValidateFlags(_scrollToZoomFlags, &desc);

    CFTypeRef magnifier = CFPreferencesCopyAppValue(STZScrollToZoomMagnifierKey, appID);
    if (magnifier) {
        if (CFGetTypeID(magnifier) == CFNumberGetTypeID()) {
            CFNumberGetValue(magnifier, kCFNumberDoubleType, &_scrollToZoomMagnifier);
            _scrollToZoomMagnifier = clamp(_scrollToZoomMagnifier, -1, 1);
        }
        CFRelease(magnifier);
    }

    CFTypeRef attenuation = CFPreferencesCopyAppValue(STZScrollMomentumToZoomAttenuationKey, appID);
    if (attenuation) {
        if (CFGetTypeID(attenuation) == CFNumberGetTypeID()) {
            CFNumberGetValue(attenuation, kCFNumberDoubleType, &_scrollMomentumToZoomAttenuation);
            _scrollMomentumToZoomAttenuation = clamp(_scrollMomentumToZoomAttenuation, 0, 1);
        }
        CFRelease(attenuation);
    }

    CFTypeRef minMomentum = CFPreferencesCopyAppValue(STZScrollMinMomentumMagnificationKey, appID);
    if (minMomentum) {
        if (CFGetTypeID(minMomentum) == CFNumberGetTypeID()) {
            CFNumberGetValue(minMomentum, kCFNumberDoubleType, &_scrollMinMomentumMagnification);
            _scrollMinMomentumMagnification = clamp(_scrollMinMomentumMagnification, 0, 1);
        }
        CFRelease(minMomentum);
    }

    if (_eventTapOptionsForApps) {
        CFDictionaryRemoveAllValues(_eventTapOptionsForApps);
        CFDictionaryRemoveAllValues(_eventTapOptionsObjsForApps);
    } else {
        _eventTapOptionsForApps = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, NULL);
        _eventTapOptionsObjsForApps = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    }

    CFTypeRef optionsDict = CFPreferencesCopyAppValue(STZEventTapOptionsForAppsKey, appID);
    if (optionsDict) {
        if (CFGetTypeID(optionsDict) == CFDictionaryGetTypeID()) {
            CFIndex count = CFDictionaryGetCount(optionsDict);
            void const **keys = malloc(sizeof(void *) * count);
            void const **vals = malloc(sizeof(void *) * count);
            CFDictionaryGetKeysAndValues(optionsDict, keys, vals);

            for (CFIndex i = 0; i < count; ++i) {
                if (CFGetTypeID(vals[i]) != CFNumberGetTypeID()) {continue;}

                STZEventTapOptions options;
                CFNumberGetValue(vals[i], kCFNumberSInt32Type, &options);

                CFDictionarySetValue(_eventTapOptionsForApps, keys[i], (void *)(uintptr_t)options);
                CFDictionarySetValue(_eventTapOptionsObjsForApps, keys[i], vals[i]);
            }

            free(keys);
            free(vals);
        }
        CFRelease(optionsDict);
    }

    STZDebugLog("Arguments loaded from user defaults:");
    STZDebugLog("\tScrollToZoomFlags set to %@", desc);
    STZDebugLog("\tScrollToZoomMagnifier set to %f", _scrollToZoomMagnifier);
    STZDebugLog("\tScrollMomentumToZoomAttenuation set to %f", _scrollMomentumToZoomAttenuation);
    STZDebugLog("\tScrollMinMomentumMagnification set to %f", _scrollMinMomentumMagnification);
}


//  MARK: - Event Tap Registry

//  A main goal of *ScrollToZoom* is to integrate seamlessly with other mouse-optimizing apps. To
//  forbid the interference from other event taps, we need to record some hardware data before any
//  other tap, and emit the modified event after all other taps. However, this approach is
//  challenging because the order in which apps open can invert the intended control flow. Say we
//  already created the tap and another app is opened later, we will miss its modifications.
//  Therefore, we dynamically register and remove taps as needed.
//
//  In this file, we call a `HIDEventTap` a hard one and an `AnnotatedSessionEventTap` a soft one.
//  It’s known that *LinearMouse* intercepts events hard-ly and *Mos* intercepts soft-ly.


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


#define DECLARE_EVENT_TAP(prefix, events, location, placement, options, initialEnabled)             \
static CGEventRef prefix##Callback(CGEventTapProxy, CGEventType, CGEventRef, void *);               \
static STZEventTap prefix##Tap = {NULL, NULL, false,                                                \
                                  events, location, placement, options,                             \
                                  prefix##Callback, initialEnabled};                                \

DECLARE_EVENT_TAP(hardFlagsChanged, (1 << kCGEventFlagsChanged) | (1 << kCGEventOtherMouseDown) | (1 << kCGEventOtherMouseUp),
                  kCGHIDEventTap, kCGHeadInsertEventTap,
                  kCGEventTapOptionListenOnly, true);

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

static bool eventTapGetRegistered(STZEventTap *eventTap) {
    return eventTap->port != NULL;
}

static bool eventTapSetRegistered(STZEventTap *eventTap, bool flag) {
    if (flag == (eventTap->port != NULL)) {return true;}

    if (!flag) {
        CFRunLoopRemoveSource(CFRunLoopGetMain(), eventTap->source, kCFRunLoopCommonModes);
        CGEventTapEnable(eventTap->port, false);
        CFRelease(eventTap->source);
        CFRelease(eventTap->port);

        eventTap->port = NULL;
        eventTap->source = NULL;

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

static bool eventTapGetEnabled(STZEventTap *eventTap) {
    return eventTap->enabled;
}

static void eventTapSetEnabled(STZEventTap *eventTap, bool flag) {
    if (flag == eventTap->enabled) {return;}
    eventTap->enabled = flag;
    CGEventTapEnable(eventTap->port, flag);
}


typedef struct {
    bool                flagsIn;
    bool                needsReinsertTaps;
    signed int          supposedTapCount;
} STZGlobalContext;

#define kSTZGlobalContextEmpty (STZGlobalContext) {false, false, 0}

STZGlobalContext _globalContext = kSTZGlobalContextEmpty;

/// Returns the pointer to the global context.
static STZGlobalContext *globalContext(void) {
    return &_globalContext;
}


typedef struct {
    STZWheelSession     session;

    /// Whether a hard scroll wheel is received after the last modifier flags in. This flag will be
    /// automatically reset by the flags changes tap.
    bool                anyReceived;
    bool                activated;
    bool                excludesFlags;
    bool                lastEventChanged;
    int                 deltaSignum;
    CGEventTimestamp    momentumStart;
    uint64_t            timerToken;
} STZWheelContext;

#define kSTZWheelContextEmpty (STZWheelContext){kSTZWheelSessionEmpty, false, false, false, false, 0, 0, 0}

static STZCScanCache _wheelContexts = kSTZCScanCacheEmptyForType(STZWheelContext);


static void _logExpiredWheelContexts(uint64_t registryID, void *data, bool expired, void *refcon) {
    if (expired) {
        STZDebugLog("Marked context expired for [%llx]", registryID);
    }
}

static void _checkExpiredWheelContexts(void) {
    CGEventTimestamp const CHECK_INTERVAL = 60 * NSEC_PER_SEC;  //  1 minute.
    CGEventTimestamp const LIFETIME = 300 * NSEC_PER_SEC;  //  5 minutes.

    STZCScanCacheCheckExpired(&_wheelContexts, CHECK_INTERVAL, LIFETIME);

    if (STZIsLoggingEnabled()) {
        STZCScanCacheEnumerateData(&_wheelContexts, true, _logExpiredWheelContexts, NULL);
    }
}


/// Returns the pointer to the context for the given sender. If no context is found, a new one will
/// be allocated and returned.
static STZWheelContext *wheelContextFor(CGEventRef event) {
    _checkExpiredWheelContexts();

    uint64_t registryID = CGEventGetRegistryID(event);

    STZCScanCacheResult result;
    STZWheelContext *context = STZCScanCacheGetDataForIdentifier(&_wheelContexts, registryID, &result);

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


static void enumerateWheelContexts(void (*callback)(uint64_t registryID, STZWheelContext *data, bool expired, void *refcon), void *refcon) {
    STZCScanCacheEnumerateData(&_wheelContexts, false, (STZCacheEnumerationCallback)callback, refcon);
}


/// Enumerates all wheel contexts.
#define forEachWheelContext(var, body) {                                                            \
    CGEventTimestamp now = CGEventTimestampNow();                                                   \
    _checkExpiredWheelContexts(now);                                                                \
                                                                                                    \
    for (int i = 0; i < _wheelContexts.count; ++i) {                                                \
        if (_wheelContexts.entries[i].accessedAt == 0) {continue;}                                  \
        STZWheelContext *var = &_wheelContexts.entries[i].context;                                  \
        body;                                                                                       \
    }                                                                                               \
}                                                                                                   \


static void anyEventTapAdded(CFNotificationCenterRef center, void *observer,
                             CFNotificationName name, const void *object,
                             CFDictionaryRef userInfo) {
    uint32_t count;
    CGGetEventTapList(0, NULL, &count);

    if (count > globalContext()->supposedTapCount) {
        STZDebugLog("Foreign event tap added");
        globalContext()->needsReinsertTaps = true;
    }
}

static void anyEventTapRemoved(CFNotificationCenterRef center, void *observer,
                               CFNotificationName name, const void *object,
                               CFDictionaryRef userInfo) {
    globalContext()->supposedTapCount -= 1;  //  OK to be negative.
}


static void resetNeedsReinsertTaps(void) {
    uint32_t count;
    CGGetEventTapList(0, NULL, &count);
    globalContext()->supposedTapCount = count;
    globalContext()->needsReinsertTaps = false;
}


static bool setAllEventTapsRegistered(bool flag) {
    return eventTapSetRegistered(&hardFlagsChangedTap, flag)
        && eventTapSetRegistered(&hardScrollWheelTap, flag)
        && eventTapSetRegistered(&passiveScrollWheelTap, flag)
        && eventTapSetRegistered(&mutatingScrollWheelTap, flag);
}


bool STZGetEventTapEnabled(void) {
    return eventTapGetRegistered(&hardFlagsChangedTap);
}


bool STZSetEventTapEnabled(bool enable) {
    if (enable == eventTapGetRegistered(&hardFlagsChangedTap)) {return true;}

    if (!enable) {
        setAllEventTapsRegistered(false);
        STZDebugLog("Event tap unregistered");
        return true;

    } else if (!setAllEventTapsRegistered(true)) {
        setAllEventTapsRegistered(false);
        STZDebugLog("Event tap failed to register.");
        return false;

    } else {
        static dispatch_once_t onceToken;
        dispatch_once(&onceToken, ^{
            //  Callbacks are invoked on the the main thread. We use only one thread.
            CFNotificationCenterAddObserver(CFNotificationCenterGetDarwinNotifyCenter(),
                                            NULL, &anyEventTapAdded,
                                            CFSTR(kCGNotifyEventTapAdded), NULL,
                                            0 /* ignored for Darwin center */);
            CFNotificationCenterAddObserver(CFNotificationCenterGetDarwinNotifyCenter(),
                                            NULL, &anyEventTapRemoved,
                                            CFSTR(kCGNotifyEventTapRemoved), NULL,
                                            0 /* ignored for Darwin center */);
        });
        resetNeedsReinsertTaps();
        STZDebugLog("Event tap registered");
        return true;
    }
}


static void reinsertTapsIfNeeded(void) {
    if (!globalContext()->needsReinsertTaps) {return;}

    STZDebugLog("Re-inset event tap due to environment change");
    setAllEventTapsRegistered(false);

    if (!setAllEventTapsRegistered(true)) {
        setAllEventTapsRegistered(false);
        STZDebugLog("Event tap failed to re-enable.");
    } else {
        resetNeedsReinsertTaps();
    }
}


static void eventTapTimeout(void) {
    setAllEventTapsRegistered(false);
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


static void switchToMutatingScrollWheelTap(void) {
    if (eventTapGetEnabled(&mutatingScrollWheelTap)) {return;}

    eventTapSetEnabled(&passiveScrollWheelTap, false);
    eventTapSetEnabled(&mutatingScrollWheelTap, true);
    STZDebugLog("Switch to mutating scroll wheel tap");
}


/// Used by `tryToSwitchToPassiveScrollWheelTap`.
static void checkEachWheelContextPassive(uint64_t registryID, STZWheelContext *context, bool _, void *flag) {
    if (context->activated || context->session.state || context->lastEventChanged) {
        *(bool *)flag = false;
    }
}


static bool tryToSwitchToPassiveScrollWheelTap(void) {
    if (eventTapGetEnabled(&passiveScrollWheelTap)) {return true;}
    if (globalContext()->flagsIn) {return false;}

    bool canBePassive = true;
    enumerateWheelContexts(checkEachWheelContextPassive, &canBePassive);

    if (!canBePassive) {return false;}

    eventTapSetEnabled(&passiveScrollWheelTap, true);
    eventTapSetEnabled(&mutatingScrollWheelTap, false);
    STZDebugLog("Switch to passive scroll wheel tap");
    return true;
}


/// Used by `hardFlagsChangedCallback`.
static void setEachWheelContextReleased(uint64_t registryID, STZWheelContext *context, bool _, void *event) {
    if (!globalContext()->flagsIn && context->activated) {
        releaseSessionFromEvent(&context->session, event);
    }

    context->anyReceived = false;
    context->activated = false;
    context->timerToken += 1;
}


static CGEventRef hardFlagsChangedCallback(CGEventTapProxy proxy, CGEventType eventType, CGEventRef event, void *userInfo) {
    bool flagsIn;

    switch (eventType) {
    case kCGEventTapDisabledByTimeout:      eventTapTimeout(); CF_FALLTHROUGH;
    case kCGEventTapDisabledByUserInput:    return NULL;

    case kCGEventFlagsChanged:
        if (!(_scrollToZoomFlags & kSTZModifiersMask)) {return event;}
        flagsIn = STZValidateFlags(CGEventGetFlags(event) & kSTZModifiersMask, NULL) == _scrollToZoomFlags;
        break;

    case kCGEventOtherMouseDown:
        if (!(_scrollToZoomFlags & kSTZMouseButtonsMask)) {return event;}
        if (CGEventGetIntegerValueField(event, kCGMouseEventButtonNumber) != _scrollToZoomFlags) {return event;}
        flagsIn = true;
        break;

    case kCGEventOtherMouseUp:
        if (!(_scrollToZoomFlags & kSTZMouseButtonsMask)) {return event;}
        if (CGEventGetIntegerValueField(event, kCGMouseEventButtonNumber) != _scrollToZoomFlags) {return event;}
        flagsIn = false;
        break;

    default: assert(false); break;
    }

    STZDebugLogEvent("Received hard", event);

    if (globalContext()->flagsIn != flagsIn) {
        globalContext()->flagsIn = flagsIn;
        enumerateWheelContexts(setEachWheelContextReleased, event);
        reinsertTapsIfNeeded();

        if (flagsIn) {
            eventTapSetEnabled(&hardScrollWheelTap, true);
            STZDebugLog("Activate hard scroll wheel listening");
            switchToMutatingScrollWheelTap();

        } else {
            eventTapSetEnabled(&hardScrollWheelTap, false);
            STZDebugLog("Deactivate hard scroll wheel listening");
            tryToSwitchToPassiveScrollWheelTap();
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
    if (globalContext()->flagsIn) {
        STZWheelContext *context = wheelContextFor(event);

        if (!context->anyReceived) {
            pid_t pid = (int32_t)CGEventGetIntegerValueField(event, kCGEventTargetUnixProcessID);
            CFStringRef bundleID = STZGetBundleIdentifierForProcessID(pid);
            STZEventTapOptions options = STZGetEventTapOptionsForBundleIdentifier(bundleID);

            context->anyReceived = true;
            context->activated = !(options & kSTZEventTapDisabled);
            context->excludesFlags = !!(options & kSTZEventTapExcludeFlags);
        }

        if (context->activated) {
            context->deltaSignum = STZMarkDeltaSignumForScrollWheelEvent(event);
        }
    }

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

    STZPhase phase;
    bool byMomentum;
    STZGetPhaseFromScrollWheelEvent(event, &phase, &byMomentum);

    STZWheelType type;
    type = byMomentum ? kSTZWheelToScrollMomentum : kSTZWheelToScroll;

    STZWheelSessionAssign(&context->session, type, phase);
    return event;
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

    STZWheelType type;
    STZPhase phase;
    double data = 0;
    STZTrivalent hasSuccessor = false;

    if (context->activated) {
        bool accepted;
        STZConvertPhaseFromScrollWheelEvent(event, context->deltaSignum, &accepted,
                                            &context->momentumStart,
                                            &phase, &data, &hasSuccessor);
        type = kSTZWheelToZoom;

        if (accepted) {
            STZDebugLog("\tEvent is from unknown source");
        }

    } else {
        bool byMomentum;
        STZGetPhaseFromScrollWheelEvent(event, &phase, &byMomentum);
        type = byMomentum ? kSTZWheelToScrollMomentum : kSTZWheelToScroll;
    }

    STZWheelSession oldSession = context->session;
    STZEventAction action;
    CGEventRef outEvents[2] = {NULL, NULL};
    STZWheelSessionUpdate(&context->session, type, phase, data, event, outEvents, &action);

    switch (action) {
    case kSTZEventUnchanged:
        context->lastEventChanged = false;
        STZDebugLogSessionChange("\tSession changed", oldSession, context->session);
        tryToSwitchToPassiveScrollWheelTap();
        return event;

    case kSTZEventAdapted:
        context->lastEventChanged = true;
        STZDebugLogSessionChange("\tSession changed", oldSession, context->session);
        STZDebugLogEvent("\tEvent modified to", event);
        tryToSwitchToPassiveScrollWheelTap();
        return event;

    case kSTZEventReplaced:
        context->lastEventChanged = true;
        break;
    }

    //  The event is annotated; returning a different event may have no effect. Therefore all
    //  the out events will be newly posted and `NULL` is returnrd.

    if (context->excludesFlags) {
        for (int i = 0; i < 2; ++i) {
            CGEventRef event = outEvents[i];
            if (event) {
                CGEventFlags flags = CGEventGetFlags(event);
                flags &= ~STZGetScrollToZoomFlags();
                CGEventSetFlags(event, flags);
            }
        }
    }

    if (hasSuccessor != kSTZMaybe) {
        STZDebugLogSessionChange("\tSession changed", oldSession, context->session);
        replaceEventByConsumingEvents(outEvents);
        tryToSwitchToPassiveScrollWheelTap();

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
            tryToSwitchToPassiveScrollWheelTap();
            CFRelease(event);
        });

    } else {
        STZDebugLog("\tWaiting for more events before replacing the event");

        STZWheelSession newSession = context->session;
        context->session = oldSession;

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
            STZDebugLogSessionChange("\tSession changed", oldSession, context->session);
            replaceEventByConsumingEvents((CGEventRef const *)&tupleEvents);
            tryToSwitchToPassiveScrollWheelTap();
        });
    }

    return NULL;
}
