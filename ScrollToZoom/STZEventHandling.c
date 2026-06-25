/*
 *  STZEventHandling.c
 *  ScrollToZoom
 *
 *  Created by alpha on 2026/6/19.
 *  Copyright © 2026 alphaArgon.
 */

#include "STZEventHandling.h"
#include <ApplicationServices/ApplicationServices.h>
#include "CGEventSPI.h"
#include "STZMagicZoom.h"
#include "STZStateManager.h"
#include "STZProcessManager.h"


// The order of event taps reported by `CGGetEventTapList` is not documented.
// In `STZSetWorkingModes`, soft event taps are always created after the hard one;
// if it is observed that soft event taps are prepended to the list, we can assume
// that the system sorts event taps by creation time in descending order.
static bool newEventTapsPrependsToList = false;


static bool isForeignEventTapIrrelevantToWheel(CGEventTapInformation const *info) {
    if (!(info->eventsOfInterest & (1 << kCGEventScrollWheel))) {return true;}
    if (info->options & kCGEventTapOptionListenOnly) {return true;}
    return false;
}


static bool isSystemProcess(pid_t pid) {
    CFStringRef bundleID = STZGetBundleIdentifierForProcessID(pid);
    return !bundleID || CFStringHasPrefix(bundleID, CFSTR("com.apple."));
}


static bool isWheelUnderDictatorship(void) {
    uint32_t count;
    CGGetEventTapList(0, NULL, &count);

    CGEventTapInformation *infos = malloc(sizeof(CGEventTapInformation) * count);
    CGGetEventTapList(count, infos, &count);

    if (STZIsLoggingEnabled()) {
        for (uint32_t i = 0; i < count; ++i) {
            if (isForeignEventTapIrrelevantToWheel(&infos[i])) {continue;}

            char const *locName = "";
            switch (infos[i].tapPoint) {
            case kCGHIDEventTap: locName = "HID"; break;
            case kCGSessionEventTap: locName = "session"; break;
            case kCGAnnotatedSessionEventTap: locName = "annotated session"; break;
            default: locName = "unknown"; break;
            }
            CFStringRef bundleID = STZGetBundleIdentifierForProcessID(infos[i].tappingProcess);
            STZDebugLog("\ttap [%u] from %@ at %s", infos[i].eventTapID, bundleID, locName);
        }
    }

    pid_t selfPID = getpid();

    for (uint32_t i = 0; i < count; ++i) {
        if (isForeignEventTapIrrelevantToWheel(&infos[i])) {continue;}

        if (newEventTapsPrependsToList) {
            if (infos[i].tappingProcess == selfPID) {
                free(infos);
                return true;
            } else if (!isSystemProcess(infos[i].tappingProcess)) {
                free(infos);
                return false;
            }

        } else {
            if (infos[i].tappingProcess != selfPID && !isSystemProcess(infos[i].tappingProcess)) {
                free(infos);
                return false;
            }
        }
    }

    free(infos);
    return false;
}


static void checkNewSoftWheelTapPrepended(void) {
    newEventTapsPrependsToList = false;

    uint32_t count;
    CGGetEventTapList(0, NULL, &count);
    if (count <= 1) {return;}

    CGEventTapInformation *infos = malloc(sizeof(CGEventTapInformation) * count);
    CGGetEventTapList(count, infos, &count);

    if (infos[0].tappingProcess == getpid()) {
        newEventTapsPrependsToList = infos[0].tapPoint != kCGHIDEventTap;
    } else {
        newEventTapsPrependsToList = false;
    }

    if (newEventTapsPrependsToList) {
        STZDebugLog("\tnew event taps are known to be prepended to the system list");
    } else {
        STZDebugLog("\tcannot determine where new event taps are inserted into the system list");
    }
    free(infos);
    return;
}


typedef struct {
    CFMachPortRef       port;
    CFRunLoopSourceRef  source;
} STZEventTap;


static void releaseEventTap(STZEventTap *tap) {
    assert(!tap->port == !tap->source);
    if (!tap->port) {return;}
    CFRunLoopRemoveSource(CFRunLoopGetMain(), tap->source, kCFRunLoopCommonModes);
    CGEventTapEnable(tap->port, false);
    CFMachPortInvalidate(tap->port);
    CFRelease(tap->source);
    CFRelease(tap->port);
    tap->source = NULL;
    tap->port = NULL;
}


static bool createEventTap(STZEventTap *tap, CGEventTapCallBack callback, CGEventMask events, CGEventTapOptions options, CGEventTapLocation location) {
    assert(!tap->port && !tap->source);
    tap->port = CGEventTapCreate(location,
                                 location == kCGHIDEventTap ? kCGHeadInsertEventTap : kCGTailAppendEventTap,
                                 options, events, callback, NULL);
    if (!tap->port) {return false;}

    tap->source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, tap->port, 0);
    CFRunLoopAddSource(CFRunLoopGetMain(), tap->source, kCFRunLoopCommonModes);
    return true;
}


static void magicZoomActivationCallback(uint64_t registryID, bool active, void *refcon);
static CGEventRef flagsTapCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon);
static CGEventRef hardWheelTapCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon);
static CGEventRef passiveSoftWheelTapCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon);
static CGEventRef mutableSoftWheelTapCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon);
static void periodicUpdateCallback(CFRunLoopTimerRef timer, void *refcon);


typedef struct {
    STZStateRef     state;
    STZAppOptions   appOptions;
    uint64_t        hardScrollDir;
    bool            magicZoomPending;
} WheelContext;

static void wheelContextDispose(void *context) {
    STZStateRelease(((WheelContext *)context)->state);
}

static STZCacheRef wheelContexts = NULL;


static STZEventTap flagsTap = {NULL, NULL};
static STZEventTap passiveHardWheelTap = {NULL, NULL};
static STZEventTap mutableHardWheelTap = {NULL, NULL};
static STZEventTap passiveSoftWheelTap = {NULL, NULL};
static STZEventTap mutableSoftWheelTap = {NULL, NULL};


static bool stabWantsDictatorship = false;
static bool magicZooms = false;
static bool wheelTapsMutable = false;
static bool triggerFlagsDown = false;
static CFRunLoopTimerRef periodicTimer = NULL;


static bool needsReinsertTaps = false;
static void anyEventTapAddedOrRemoved(CFNotificationCenterRef center, void *observer,
                                      CFNotificationName name, const void *object,
                                      CFDictionaryRef userInfo) {
    STZDebugLog("Foreign event tap added or removed");
    needsReinsertTaps = true;
}

static WheelContext *wheelContextWithFallback(uint64_t registryID) {
    WheelContext *context;
    if (registryID != 0) {
        context = STZCacheGetValue(wheelContexts, registryID);
    } else {
        context = STZCacheGetRecentValue(wheelContexts, &registryID);
    }

    if (context != NULL) {return context;}

    WheelContext ctx_ = {
        .state = STZStateCreate(),
        .appOptions = 0,
        .hardScrollDir = 0,
        .magicZoomPending = false,
    };
    return STZCacheSetValue(wheelContexts, registryID, &ctx_);
}


CFStringRef const kSTZWorkingModesDidChangeNotification = CFSTR("STZWorkingModesDidChangeNotification");


STZModes STZGetWorkingModes(void) {
    STZModes modes = 0;

    if (magicZooms) {
        modes |= kSTZMagicZoomEnabled;
    }
    if (flagsTap.port) {
        modes |= kSTZTriggerFlagsEnabled;
    }
    if (passiveHardWheelTap.port || stabWantsDictatorship) {
        modes |= kSTZWantsDictatorship;
    }
    return modes;
}


bool STZSetWorkingModes(STZModes modes) {
    if (!(modes & kSTZPracticalModesMask)) {goto RESET;}
    if (!AXIsProcessTrusted()) {goto RESET;}

    //  An annotated session tap expects the event type won’t be altered by the callback.
    //  Use a session tap when possible so we can emit events using `CGEventTapPost` instead of
    //  `CGEventPost`, which is somewhat more efficient.
    CGEventTapLocation softTapLocation = (modes & kSTZWantsDictatorship) ? kCGAnnotatedSessionEventTap : kCGSessionEventTap;

    //  This value is like Cocoa KVO’s context pointer.
    static void const *tapAddedOrRemovedObserver = &anyEventTapAddedOrRemoved;

    if (!passiveHardWheelTap.port != !(modes & kSTZWantsDictatorship)) {
        releaseEventTap(&passiveSoftWheelTap);
        releaseEventTap(&mutableSoftWheelTap);

        if (passiveHardWheelTap.port) {
            releaseEventTap(&passiveHardWheelTap);
            releaseEventTap(&mutableHardWheelTap);
            CFNotificationCenterRemoveObserver(CFNotificationCenterGetDarwinNotifyCenter(),
                                               tapAddedOrRemovedObserver,
                                               CFSTR(kCGNotifyEventTapAdded), NULL);
        } else {
            if (!createEventTap(&passiveHardWheelTap, hardWheelTapCallback,
                                1 << kCGEventScrollWheel,
                                kCGEventTapOptionListenOnly, kCGHIDEventTap)) {
                goto RESET;
            }
            if (!createEventTap(&mutableHardWheelTap, hardWheelTapCallback,
                                1 << kCGEventScrollWheel,
                                kCGEventTapOptionDefault, kCGHIDEventTap)) {
                goto RESET;
            }
            CFNotificationCenterAddObserver(CFNotificationCenterGetDarwinNotifyCenter(),
                                            tapAddedOrRemovedObserver,
                                            anyEventTapAddedOrRemoved,
                                            CFSTR(kCGNotifyEventTapAdded), NULL,
                                            0 /* ignored for Darwin center */);
        }
    }

    if (!flagsTap.port != !(modes & kSTZTriggerFlagsEnabled)) {
        if (flagsTap.port) {
            releaseEventTap(&flagsTap);
            triggerFlagsDown = false;
        } else {
            if (!createEventTap(&flagsTap, flagsTapCallback,
                                (1 << kCGEventFlagsChanged) | (1 << kCGEventOtherMouseDown) | (1 << kCGEventOtherMouseUp),
                                kCGEventTapOptionListenOnly, kCGHIDEventTap)) {
                goto RESET;
            }
            CGEventTapEnable(flagsTap.port, true);
        }
    }

    bool anySoftEventNewlyAdded = false;

    if (!passiveSoftWheelTap.port != !(modes & (kSTZWantsDictatorship | kSTZTriggerFlagsEnabled))) {
        if (passiveSoftWheelTap.port) {
            releaseEventTap(&passiveSoftWheelTap);
        } else {
            if (!createEventTap(&passiveSoftWheelTap, passiveSoftWheelTapCallback,
                                1 << kCGEventScrollWheel,
                                kCGEventTapOptionListenOnly, softTapLocation)) {
                goto RESET;
            }
            anySoftEventNewlyAdded = true;
        }
    }

    if (!mutableSoftWheelTap.port != !modes) {
        if (mutableSoftWheelTap.port) {
            releaseEventTap(&mutableSoftWheelTap);
        } else {
            if (!createEventTap(&mutableSoftWheelTap, mutableSoftWheelTapCallback,
                                1 << kCGEventScrollWheel,
                                kCGEventTapOptionDefault, softTapLocation)) {
                goto RESET;
            }
            anySoftEventNewlyAdded = true;
        }
    }

    if (magicZooms != !!(modes & kSTZMagicZoomEnabled)) {
        if (magicZooms) {
            magicZooms = false;
            STZSetListeningMagicMice(false);
            STZMagicZoomObserveActivation(NULL, NULL);

        } else {
            magicZooms = true;
            if (!STZSetListeningMagicMice(true)) {
                goto RESET;
            }
            STZMagicZoomObserveActivation(magicZoomActivationCallback, NULL);
        }
    }

    if (!mutableSoftWheelTap.port) {
        wheelTapsMutable = false;

    } else {
        CGEventTapEnable(mutableSoftWheelTap.port, wheelTapsMutable);
        if (passiveSoftWheelTap.port) {
            CGEventTapEnable(passiveSoftWheelTap.port, !wheelTapsMutable);
        }
    }

    if (passiveHardWheelTap.port) {
        //  When toggling dictatorship, `anySoftEventNewlyAdded` must be true becauze we
        //  discard the old soft event taps and recreate new ones.
        if (anySoftEventNewlyAdded) {
            checkNewSoftWheelTapPrepended();
        }
        CGEventTapEnable(mutableHardWheelTap.port, wheelTapsMutable);
        CGEventTapEnable(passiveHardWheelTap.port, !wheelTapsMutable);
    }

    if (!wheelContexts) {
        wheelContexts = STZCacheCreate(sizeof(WheelContext), 300 * NSEC_PER_SEC, wheelContextDispose);
    }

    stabWantsDictatorship = false;
    needsReinsertTaps = false;

    CFNotificationCenterPostNotification(CFNotificationCenterGetLocalCenter(),
                                         kSTZWorkingModesDidChangeNotification,
                                         NULL, NULL, true);
    return true;

RESET:
    if (passiveHardWheelTap.port) {
        releaseEventTap(&passiveHardWheelTap);
        releaseEventTap(&mutableHardWheelTap);
        CFNotificationCenterRemoveObserver(CFNotificationCenterGetDarwinNotifyCenter(),
                                           tapAddedOrRemovedObserver,
                                           CFSTR(kCGNotifyEventTapAdded), NULL);
    }

    stabWantsDictatorship = !!(modes & kSTZWantsDictatorship);

    releaseEventTap(&flagsTap);
    triggerFlagsDown = false;

    releaseEventTap(&passiveSoftWheelTap);
    releaseEventTap(&mutableSoftWheelTap);

    if (magicZooms) {
        magicZooms = false;
        STZSetListeningMagicMice(false);
        STZMagicZoomObserveActivation(NULL, NULL);
    }

    wheelTapsMutable = false;
    if (wheelContexts) {
        STZCacheRemoveAll(wheelContexts);
    }

    if (periodicTimer) {
        CFRunLoopTimerInvalidate(periodicTimer);
        CFRelease(periodicTimer);
        periodicTimer = NULL;
    }

    CFNotificationCenterPostNotification(CFNotificationCenterGetLocalCenter(),
                                         kSTZWorkingModesDidChangeNotification,
                                         NULL, NULL, true);
    return false;
}


static void eventTapTimeout(void) {
    STZDebugLog("Event tap disabled due to timeout");
    STZSetWorkingModes(0);
}


static void reinsertTapsIfNeeded(void) {
    if (passiveHardWheelTap.port == NULL) {return;}

    if (!needsReinsertTaps) {return;}
    STZDebugLog("Checking dictatorship due to environment change");

    if (isWheelUnderDictatorship()) {
        needsReinsertTaps = false;
        return;
    }

    STZModes modes = STZGetWorkingModes();
    releaseEventTap(&passiveHardWheelTap);
    releaseEventTap(&mutableHardWheelTap);
    releaseEventTap(&passiveSoftWheelTap);
    releaseEventTap(&mutableSoftWheelTap);

    if (STZSetWorkingModes(modes)) {
        STZDebugLog("Successfully reinserted event taps");
    } else {
        STZDebugLog("Failed to reinsert event taps");
    }
}


static void clearTriggerFlagsForEvent(CGEventRef event) {
    CGEventFlags mask = ~(STZGetTriggerFlags() & kSTZModifiersMask);
    CGEventSetFlags(event, CGEventGetFlags(event) & mask);
}


static void beginWheelTapMutations(void) {
    if (wheelTapsMutable) {return;}
    CGEventTapEnable(mutableSoftWheelTap.port, true);
    if (passiveSoftWheelTap.port) {
        CGEventTapEnable(passiveSoftWheelTap.port, false);
    }
    if (passiveHardWheelTap.port) {
        CGEventTapEnable(mutableHardWheelTap.port, true);
        CGEventTapEnable(passiveHardWheelTap.port, false);
    }
    wheelTapsMutable = true;
    STZDebugLog("\tswitched to mutating scroll wheel taps");
}


typedef OPTION_FLAGS(uint64_t) {
    kStateSessionIsMagicZoom    = 1 << 0,
} StateSessionData;


typedef OPTION_FLAGS(uint8_t) {
    kTryToEndWheelTapMutations  = 1 << 0,
    kRescheduleTimer            = 1 << 1,
    kEmitPeriodicEvents         = 1 << 2,
    kDiscardTriggerFlags        = 1 << 3,
} WheelContextActions;


typedef struct {
    WheelContextActions     const actions;
    CGEventTimestamp        const now;
    CGEventRef              const event;
    bool                    canEndMutations;
    CGEventTimestamp        eventUpdatePeriod;
} WheelContextDoEnv;


static void wheelContextDo(void *addr, void *refcon) {
    WheelContext *context = addr;
    WheelContextDoEnv *env = refcon;

    if (env->actions & kEmitPeriodicEvents) {
        CGEventRef event = STZStatePeriodicallyUpdate(context->state, env->now);
        if (event != NULL) {
            if (context->appOptions & kSTZFlagsExcludedForApp) {
                clearTriggerFlagsForEvent(event);
            }
            STZDebugLogEvent("\tperiodic", event);
            CGEventPost(kCGSessionEventTap, event);
            CFRelease(event);
        }
    }

    if (env->actions & kDiscardTriggerFlags) {
        StateSessionData data;
        if (STZStateGetSessionData(context->state, &data) && !(data & kStateSessionIsMagicZoom)) {
            CGEventRef event = STZStateRevertToScrollByEvent(context->state, env->event);
            if (event != NULL) {
                STZDebugLogEvent("\tfollowed by", event);
                CGEventPost(kCGSessionEventTap, event);
                CFRelease(event);
            }
        }
    }

    if (env->actions & kTryToEndWheelTapMutations) {
        StateSessionData data;
        if (context->magicZoomPending
         || (STZStateGetSessionData(context->state, &data) && (data & kStateSessionIsMagicZoom))
         || !STZStateCanStopTransformingEvents(context->state)) {
            env->canEndMutations = false;
        }
    }

    if (env->actions & kRescheduleTimer) {
        CGEventTimestamp updatePeriod = STZStateGetNextUpdatePeriod(context->state, env->now);
        if (updatePeriod != 0) {
            if (env->eventUpdatePeriod == 0 || env->eventUpdatePeriod > updatePeriod) {
                env->eventUpdatePeriod = updatePeriod;
            }
        }
    }
}


static void forEachStateDo(WheelContextActions actions, CGEventRef event) {
    assert(!(actions & kDiscardTriggerFlags) || event);

    if (!wheelTapsMutable || triggerFlagsDown) {
        actions = actions & ~kTryToEndWheelTapMutations;
    }

    WheelContextDoEnv env = {
        .actions = actions,
        .now = CGEventTimestampNow(),
        .event = event,
        .canEndMutations = (actions & kTryToEndWheelTapMutations) != 0,
        .eventUpdatePeriod = 0,
    };

    STZCacheEnumerateValues(wheelContexts, wheelContextDo, &env);

    if (env.canEndMutations) {
        CGEventTapEnable(mutableSoftWheelTap.port, false);
        if (passiveSoftWheelTap.port) {
            CGEventTapEnable(passiveSoftWheelTap.port, true);
        }
        if (passiveHardWheelTap.port) {
            CGEventTapEnable(mutableHardWheelTap.port, false);
            CGEventTapEnable(passiveHardWheelTap.port, true);
        }
        wheelTapsMutable = false;
        STZDebugLog("\tswitched to passive scroll wheel taps");
    }

    if (actions & kRescheduleTimer) {
        if (env.eventUpdatePeriod == 0) {
            if (periodicTimer != NULL) {
                CFRunLoopTimerInvalidate(periodicTimer);
                CFRelease(periodicTimer);
                periodicTimer = NULL;
            }

        } else {
            CFAbsoluteTime fireDate = CFAbsoluteTimeGetCurrent() + (double)env.eventUpdatePeriod / NSEC_PER_SEC;

            if (periodicTimer != NULL) {
                if (CFRunLoopTimerGetNextFireDate(periodicTimer) < fireDate - (1.0 / 240)) {return;}
                CFRunLoopTimerInvalidate(periodicTimer);
                CFRelease(periodicTimer);
            }

            periodicTimer = CFRunLoopTimerCreate(kCFAllocatorDefault, fireDate, 0, 0, 0, periodicUpdateCallback, NULL);
            CFRunLoopAddTimer(CFRunLoopGetMain(), periodicTimer, kCFRunLoopCommonModes);
        }
    }
}


static void magicZoomActivationCallback(uint64_t registryID, bool active, void *refcon) {
    WheelContext *context = wheelContextWithFallback(registryID);
    context->magicZoomPending = active;

    if (active) {
        STZDebugLog("Magic zoom finger down for [%llx]", registryID);
        beginWheelTapMutations();
    } else {
        STZDebugLog("Magic zoom finger up for [%llx]", registryID);
        forEachStateDo(kTryToEndWheelTapMutations, NULL);
    }

    reinsertTapsIfNeeded();
}


static CGEventRef flagsTapCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon) {
    bool flagsDown;

    switch (type) {
    case kCGEventTapDisabledByTimeout:      eventTapTimeout(); CF_FALLTHROUGH;
    case kCGEventTapDisabledByUserInput:    return NULL;

    case kCGEventFlagsChanged:
        if (!(STZGetTriggerFlags() & kSTZModifiersMask)) {return event;}
        flagsDown = (CGEventGetFlags(event) & kSTZModifiersMask) == STZGetTriggerFlags();
        break;

    case kCGEventOtherMouseDown:
        if (!(STZGetTriggerFlags() & kSTZMouseButtonsMask)) {return event;}
        if (CGEventGetIntegerValueField(event, kCGMouseEventButtonNumber) != STZGetTriggerFlags()) {return event;}
        flagsDown = true;
        break;

    case kCGEventOtherMouseUp:
        if (!(STZGetTriggerFlags() & kSTZMouseButtonsMask)) {return event;}
        if (CGEventGetIntegerValueField(event, kCGMouseEventButtonNumber) != STZGetTriggerFlags()) {return event;}
        flagsDown = false;
        break;

    default: assert(false); break;
    }

    STZDebugLogEvent("Hard", event);

    if (triggerFlagsDown != flagsDown) {
        triggerFlagsDown = flagsDown;

        if (triggerFlagsDown) {
            beginWheelTapMutations();
            reinsertTapsIfNeeded();
        } else {
            forEachStateDo(kTryToEndWheelTapMutations | kDiscardTriggerFlags, event);
        }
    }

    return event;
}


static CGEventRef hardWheelTapCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon) {
    switch (type) {
    case kCGEventTapDisabledByTimeout:      eventTapTimeout(); CF_FALLTHROUGH;
    case kCGEventTapDisabledByUserInput:    return NULL;
    default: assert(type == kCGEventScrollWheel); break;
    }

    if (wheelTapsMutable) {
        STZDebugLogEvent("Mutable hard", event);
    } else {
        STZDebugLogEvent("Passive hard", event);
    }

    //  Stashing when not mutable is a no-op, but we can store the fallback scroll direction.
    //  When using Mos, etc., a hard event may be followed by a sequence of periodic soft events.
    //  If the user presses the trigger flags during that sequence, the zoom direction may
    //  be out-of-date without this fallback value.
    WheelContext *context = wheelContextWithFallback(CGEventGetRegistryID(event));
    context->hardScrollDir = STZStashScrollDirectionIntoEvent(event);
    return event;
}


static CGEventRef passiveSoftWheelTapCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon) {
    switch (type) {
    case kCGEventTapDisabledByTimeout:      eventTapTimeout(); CF_FALLTHROUGH;
    case kCGEventTapDisabledByUserInput:    return NULL;
    default: assert(type == kCGEventScrollWheel); break;
    }

    STZDebugLogEvent("Passive soft", event);

    WheelContext *context = wheelContextWithFallback(CGEventGetRegistryID(event));
    STZStateReadScrollEvent(context->state, event);
    return event;
}


static CGEventRef mutableSoftWheelTapCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon) {
    switch (type) {
    case kCGEventTapDisabledByTimeout:      eventTapTimeout(); CF_FALLTHROUGH;
    case kCGEventTapDisabledByUserInput:    return NULL;
    default: assert(type == kCGEventScrollWheel); break;
    }

    STZDebugLogEvent("Mutable soft", event);

    WheelContext *context = wheelContextWithFallback(CGEventGetRegistryID(event));
    context->magicZoomPending = false;

    StateSessionData data = 0;
    STZAppOptions appOptions = 0;
    STZGestureType gesture = kSTZScroll;
    bool inSession = STZStateGetSessionData(context->state, &data);

    if (inSession && (data & kStateSessionIsMagicZoom)) {
        gesture = kSTZZoom;

    } else if (STZShouldBeginMagicZoom(CGEventGetRegistryID(event))) {
        data = kStateSessionIsMagicZoom;
        gesture = kSTZZoom;

    } else if (triggerFlagsDown) {
        //  Scroll events in the same session are always posted to the same process,
        //  so we need to check the app options only once per session.
        if (!inSession) {
            pid_t pid = (int32_t)CGEventGetIntegerValueField(event, kCGEventTargetUnixProcessID);
            CFStringRef bundleID = STZGetBundleIdentifierForProcessID(pid);
            context->appOptions = STZGetAppOptionsForBundleIdentifier(bundleID);
        }

        appOptions = context->appOptions;
        if (!(appOptions & kSTZDisabledForApp)) {
            gesture = kSTZZoom;
        }
    }

    bool underDictatorship = passiveHardWheelTap.port != NULL;
    uint64_t fallbackScrollDir = underDictatorship ? context->hardScrollDir : 0;

    STZEventPlacement auxPlacement;
    CGEventRef auxEvent = STZStateTransformScrollEvent(context->state, event, gesture, fallbackScrollDir, &data, &auxPlacement);
    if (underDictatorship) {
        STZReadScrollDeltaFromEvent(event, 0, true);
    }

    CGEventRef returnValue;
#define RETURNS(x) returnValue = x; break
    if (auxEvent == NULL) {
        switch (auxPlacement) {
        case kSTZReplaceEvent:
            STZDebugLog("\tdiscarded");
            RETURNS(NULL);
        case kSTZAppendEvent:
        case kSTZPrependEvent:
            STZDebugLogEvent("\tupdated to", event);
            RETURNS(event);
        }

    } else {
        if (appOptions & kSTZFlagsExcludedForApp) {
            clearTriggerFlagsForEvent(auxEvent);
        }
        switch (auxPlacement) {
        case kSTZReplaceEvent:
            STZDebugLogEvent("\treplaced by", auxEvent);
            if (underDictatorship) {
                CGEventPost(kCGSessionEventTap, auxEvent);
            } else {
                CGEventTapPostEvent(proxy, auxEvent);
            }
            CFRelease(auxEvent);
            RETURNS(NULL);
        case kSTZAppendEvent:
            STZDebugLogEvent("\tupdated to", event);
            STZDebugLogEvent("\tfollowed by", auxEvent);
            CGEventTapPostEvent(proxy, event);
            if (underDictatorship) {
                CGEventPost(kCGSessionEventTap, auxEvent);
            } else {
                CGEventTapPostEvent(proxy, auxEvent);
            }
            CFRelease(auxEvent);
            RETURNS(NULL);
        case kSTZPrependEvent:
            STZDebugLogEvent("\tpreempted by", auxEvent);
            STZDebugLogEvent("\tupdated to", event);
            if (underDictatorship) {
                CGEventPost(kCGSessionEventTap, auxEvent);
            } else {
                CGEventTapPostEvent(proxy, auxEvent);
            }
            CFRelease(auxEvent);
            RETURNS(event);
        }
    }
#undef RETURNS
    forEachStateDo(kTryToEndWheelTapMutations | kRescheduleTimer, NULL);
    return returnValue;
}


static void periodicUpdateCallback(CFRunLoopTimerRef timer, void *refcon) {
    assert(periodicTimer == timer);
    CFRelease(periodicTimer);
    periodicTimer = NULL;

    forEachStateDo(kTryToEndWheelTapMutations | kRescheduleTimer | kEmitPeriodicEvents, NULL);
}
