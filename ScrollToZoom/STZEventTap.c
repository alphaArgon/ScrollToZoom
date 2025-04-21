/*
 *  STZService.c
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


CGEventFlags STZValidateModifierFlags(CGEventFlags flags, CFStringRef *outDescription) {
    static struct {
        uint16_t        symbol;
        CFStringRef     name;
        CGEventFlags    flag;
    } const items[] = {
        {u'⌃', CFSTR("Control"), kCGEventFlagMaskControl},
        {u'⌥', CFSTR("Option"), kCGEventFlagMaskAlternate},
        {u'⇧', CFSTR("Shift"), kCGEventFlagMaskShift},
        {u'⌘', CFSTR("Command"), kCGEventFlagMaskCommand},
    };

    static const CGEventFlags allowedFlags = kCGEventFlagMaskControl | kCGEventFlagMaskAlternate | kCGEventFlagMaskShift | kCGEventFlagMaskCommand;

    if (!outDescription) {
        return flags & allowedFlags;
    }

    static const size_t itemCount = sizeof(items) / sizeof(*items);

    CGEventFlags checked = 0;
    uint16_t characters[itemCount];
    size_t characterCount = 0;

    for (size_t i = 0; i < sizeof(items) / sizeof(*items); ++i) {
        if (flags & items[i].flag) {
            checked |= items[i].flag;
            characters[characterCount] = items[i].symbol;
            characterCount += 1;
        }
    }

    CFStringRef desc = CFStringCreateWithCharacters(kCFAllocatorDefault, characters, characterCount);
    *outDescription = CFAutorelease(desc);
    return checked;
}


CGEventFlags _scrollToZoomFlags = kCGEventFlagMaskAlternate;
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


CGEventFlags STZGetScrollToZoomFlags(void) {
    return _scrollToZoomFlags;
}

void STZSetScrollToZoomFlags(CGEventFlags flags) {
    CFStringRef desc;
    _scrollToZoomFlags = STZValidateModifierFlags(flags, &desc);

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
        _scrollToZoomFlags = flags;
    }

    CFStringRef desc;
    _scrollToZoomFlags = STZValidateModifierFlags(_scrollToZoomFlags, &desc);

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
    bool enabled;
    CFMachPortRef port;
    CFRunLoopSourceRef source;
} STZEventTap;

#define kSTZEventTapEmpty (STZEventTap){false, NULL, NULL}


static STZEventTap hardEventTap = kSTZEventTapEmpty;
static STZEventTap softEventTap = kSTZEventTapEmpty;


static CGEventRef hardEventCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *userInfo);
static CGEventRef softEventCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *userInfo);


#define DECLARE_EVENT_TAP_ENABLED(name, eventTap, location, placement, option, events, callback)    \
static bool name(bool enabled) {                                                                    \
    if (enabled == eventTap.enabled) {return true;}                                                 \
                                                                                                    \
    if (!enabled) {                                                                                 \
        CFRunLoopRemoveSource(CFRunLoopGetMain(), eventTap.source, kCFRunLoopCommonModes);          \
        CGEventTapEnable(eventTap.port, false);                                                     \
                                                                                                    \
        CFRelease(eventTap.source);                                                                 \
        CFRelease(eventTap.port);                                                                   \
        eventTap = kSTZEventTapEmpty;                                                               \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    if (eventTap.source == NULL) {                                                                  \
        if (!AXIsProcessTrusted()) {return false;}                                                  \
                                                                                                    \
        CFMachPortRef port = CGEventTapCreate(location, placement, option, events, callback, NULL); \
        if (port == NULL) {return false;}                                                           \
                                                                                                    \
        eventTap.port = port;                                                                       \
        eventTap.source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, port, 0);              \
    }                                                                                               \
                                                                                                    \
    CGEventTapEnable(eventTap.port, true);                                                          \
    CFRunLoopAddSource(CFRunLoopGetMain(), eventTap.source, kCFRunLoopCommonModes);                 \
    eventTap.enabled = true;                                                                        \
    return true;                                                                                    \
}                                                                                                   \

DECLARE_EVENT_TAP_ENABLED(setHardEventTapEnabled, hardEventTap,
                          kCGHIDEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault,
                          (1 << kCGEventScrollWheel) | (1 << kCGEventFlagsChanged), hardEventCallback);

DECLARE_EVENT_TAP_ENABLED(setSoftEventTapEnabled, softEventTap,
                          kCGAnnotatedSessionEventTap, kCGTailAppendEventTap, kCGEventTapOptionDefault,
                          (1 << kCGEventScrollWheel), softEventCallback);

#undef DECLARE_EVENT_TAP_ENABLED


typedef struct {
    bool                activated;  //  Computed from `flagsIn` and `options`.
    bool                flagsIn;
    STZEventTapOptions  options;
    bool                needsReinsertTaps;
    signed int          supposedTapCount;
} STZGlobalContext;

#define kSTZGlobalContextEmpty (STZGlobalContext) {false, false, 0, false, 0}

STZGlobalContext _globalContext = kSTZGlobalContextEmpty;

/// Returns the pointer to the global context.
static STZGlobalContext *globalContext(void) {
    return &_globalContext;
}


typedef struct {
    STZWheelSession     session;
    int                 deltaSignum;
    CGEventTimestamp    momentumStart;
    uint64_t            timerToken;
} STZWheelContext;

#define kSTZWheelContextEmpty (STZWheelContext){kSTZWheelSessionEmpty, 0, 0, 0}


static struct {
    int             capacity;
    int             count;
    int             hotIndex;
    struct {
        uint64_t        senderID;
        STZWheelContext context;
    }              *entries;
} _wheelContexts = {0, 0, 0, NULL};


/// Returns the pointer to the context for the given sender. If no context is found, a new one will
/// be allocated and returned.
static STZWheelContext *wheelContextFor(CGEventRef event) {
    uint64_t senderID;
    IOHIDEventRef hidEvent = CGEventCopyIOHIDEvent(event);
    if (hidEvent) {
        senderID = IOHIDEventGetSenderID(hidEvent);
        CFRelease(hidEvent);
    } else {
        senderID = 0;
    }

    if (_wheelContexts.count && _wheelContexts.entries[_wheelContexts.hotIndex].senderID == senderID) {
        return &_wheelContexts.entries[_wheelContexts.hotIndex].context;
    }

    for (int i = 0; i < _wheelContexts.count; ++i) {
        if (_wheelContexts.entries[i].senderID == senderID) {
            _wheelContexts.hotIndex = i;
            return &_wheelContexts.entries[i].context;
        }
    }

    int i = _wheelContexts.count;
    if (i >= _wheelContexts.capacity) {
        int newCapacity = _wheelContexts.capacity ? _wheelContexts.capacity * 2 : 2;
        _wheelContexts.entries = reallocf(_wheelContexts.entries, newCapacity * sizeof(*_wheelContexts.entries));
        _wheelContexts.capacity = newCapacity;
    }

    _wheelContexts.hotIndex = i;
    _wheelContexts.entries[i].senderID = senderID;
    _wheelContexts.entries[i].context = kSTZWheelContextEmpty;
    _wheelContexts.count = i + 1;
    return &_wheelContexts.entries[i].context;
}


/// Enumerates all wheel contexts.
#define forEachWheelContext(var, body)                                                              \
for (int i = 0; i < _wheelContexts.count; ++i) {                                                    \
    STZWheelContext *var = &_wheelContexts.entries[i].context;                                      \
    body;                                                                                           \
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


bool STZGetEventTapEnabled(void) {
    return hardEventTap.enabled;
}


bool STZSetEventTapEnabled(bool enable) {
    if (enable == hardEventTap.enabled) {return true;}

    if (!enable) {
        setHardEventTapEnabled(false);
        setSoftEventTapEnabled(false);
        STZDebugLog("Event tap disabled");
        return true;

    } else if (!setHardEventTapEnabled(true) || !setSoftEventTapEnabled(true)) {
        setHardEventTapEnabled(false);
        setSoftEventTapEnabled(false);
        STZDebugLog("Event tap failed to enable.");
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
        STZDebugLog("Event tap enabled");
        return true;
    }
}


static void reinsertTapsIfNeeded(void) {
    if (!globalContext()->needsReinsertTaps) {return;}

    STZDebugLog("Re-inset event tap due to environment change");
    setHardEventTapEnabled(false);
    setSoftEventTapEnabled(false);

    if (!setHardEventTapEnabled(true) || !setSoftEventTapEnabled(true)) {
        setHardEventTapEnabled(false);
        setSoftEventTapEnabled(false);
        STZDebugLog("Event tap failed to re-enable.");
    } else {
        resetNeedsReinsertTaps();
    }
}


static void eventTapTimeout(void) {
    setHardEventTapEnabled(false);
    setSoftEventTapEnabled(false);
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
        CGEventSetTimestamp(event, clock_gettime_nsec_np(CLOCK_UPTIME_RAW_APPROX));
        CGEventPost(location, event);
        CFRelease(event);
    });
}


static void replaceEventByConsumingEvents(CGEventRef CF_CONSUMED const *events) {
    if (events[0]) {
        STZDebugLogEvent("\tEvent replaced by", events[0]);
        postAndReleaseEvent(events[0], kCGHIDEventTap, false);
    } else {
        STZDebugLog("\tEvent discarded");
    }

    if (events[1]) {
        STZDebugLogEvent("\tEvent replaced by", events[1]);
        postAndReleaseEvent(events[1], kCGHIDEventTap, true);
    }
}


static void releaseSessionFromEvent(STZWheelSession *__nonnull session, CGEventRef __nonnull event) {
    CGEventRef extraEvent = NULL;
    STZWheelSession oldSession = *session;
    STZWheelSessionDiscard(session, event, &extraEvent, NULL);
    STZDebugLogSessionChange("\tSession released", oldSession, *session);

    if (extraEvent) {
        STZDebugLogEvent("\tPost additional", extraEvent);
        postAndReleaseEvent(extraEvent, kCGHIDEventTap, false);
    }
}


static CGEventRef hardEventCallback(CGEventTapProxy proxy, CGEventType eventType, CGEventRef event, void *userInfo) {
    switch (eventType) {
    case kCGEventTapDisabledByTimeout:
        eventTapTimeout();
        return NULL;

    case kCGEventTapDisabledByUserInput:
        return NULL;

    case kCGEventScrollWheel:
        STZDebugLogEvent("Received hard", event);

        //  TODO: We should find a unique identifier for an event.
        //  This function breaks `kCGEventTapOptionListenOnly`, which may increase the CPU usage.

        //  On computers with lower performace, newer hard wheel events might be sent to the tap
        //  before the older soft events. We can’t use an accumulated value to record the signum.
        wheelContextFor(event)->deltaSignum = STZMarkDeltaSignumForScrollWheelEvent(event);
        break;

    case kCGEventFlagsChanged:
        STZDebugLogEvent("Received hard", event);

        bool flagsIn = STZValidateModifierFlags(CGEventGetFlags(event), NULL) == _scrollToZoomFlags;
        if (flagsIn == globalContext()->flagsIn) {break;}

        pid_t pid = (int32_t)CGEventGetIntegerValueField(event, kCGEventTargetUnixProcessID);
        CFStringRef bundleID = STZGetBundleIdentifierForProcessID(pid);
        STZEventTapOptions options = STZGetEventTapOptionsForBundleIdentifier(bundleID);

        bool activated = flagsIn && !(options & kSTZEventTapDisabled);
        globalContext()->flagsIn = flagsIn;
        globalContext()->options = options;
        globalContext()->activated = activated;

        forEachWheelContext(context, {
            context->timerToken += 1;
            if (!activated) {
                releaseSessionFromEvent(&context->session, event);
            }
        });

        reinsertTapsIfNeeded();
        break;
    }

    return event;
}


static CGEventRef softEventCallback(CGEventTapProxy proxy, CGEventType eventType, CGEventRef event, void *userInfo) {
    switch (eventType) {
    case kCGEventTapDisabledByTimeout:
        eventTapTimeout();
        return NULL;

    case kCGEventTapDisabledByUserInput:
        return NULL;

    default:
        assert(eventType == kCGEventScrollWheel);
        break;
    }

    STZDebugLogEvent("Received soft", event);

    STZWheelContext *context = wheelContextFor(event);
    context->timerToken += 1;

    STZWheelType type;
    STZPhase phase;
    double data = 0;
    STZTrivalent hasSuccessor = false;

    if (globalContext()->activated) {
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
        STZDebugLogSessionChange("\tSession changed", oldSession, context->session);
        return event;

    case kSTZEventAdapted:
        STZDebugLogSessionChange("\tSession changed", oldSession, context->session);
        STZDebugLogEvent("\tEvent modified to", event);
        return event;

    case kSTZEventReplaced:
        break;
    }

    //  The event is annotated; returning a different event may have no effect. Therefore all
    //  the out events will be newly posted and `NULL` is returnrd.

    if (globalContext()->options & kSTZEventTapExcludeFlags) {
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
        });
    }

    return NULL;
}
