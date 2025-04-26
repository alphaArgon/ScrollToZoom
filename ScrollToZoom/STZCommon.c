/*
 *  STZCommon.c
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/4/24.
 *  Copyright © 2025 alphaArgon.
 */

#import "STZCommon.h"
#import "CGEventSPI.h"


//  MARK: - STZCache

#define kSTZCacheExpired       ((CGEventTimestamp)0)
#define kSTZCacheNew           ((CGEventTimestamp)UINT64_MAX)


typedef struct {
    uint64_t            identifier;
    CGEventTimestamp    accessedAt;
} _STZCacheEntryStub;

#define STZCacheEntryAtIndex(cache, i) ((_STZCacheEntryStub *)(cache->entries + (cache->entrySize * i)))


void STZCScanCacheCheckExpired(STZCScanCache *cache, CGEventTimestamp checkInterval, CGEventTimestamp lifetime) {
    CGEventTimestamp now = CGEventTimestampNow();

    if (now - cache->checkedAt < checkInterval) {return;}
    cache->checkedAt = now;

    for (int i = 0; i < cache->count; ++i) {
        _STZCacheEntryStub *entry = STZCacheEntryAtIndex(cache, i);

        if (entry->accessedAt == kSTZCacheNew) {continue;}
        if (entry->accessedAt == kSTZCacheExpired) {continue;}

        if (now - entry->accessedAt > lifetime) {
            entry->accessedAt = kSTZCacheExpired;
        }
    }
}


void *STZCScanCacheGetDataForIdentifier(STZCScanCache *cache, uint64_t identifier, STZCScanCacheResult *outResult) {
    CGEventTimestamp now = CGEventTimestampNow();
    int spareIndex = -1;

    for (int h = 0; h < cache->count; ++h) {
        int i = (cache->hotIndex + h) % cache->count;
        _STZCacheEntryStub *entry = STZCacheEntryAtIndex(cache, i);

        if (entry->identifier == identifier) {
            *outResult = entry->accessedAt == kSTZCacheExpired
                ? kSTZCScanCacheExpiredRestored
                : kSTZCScanCacheFound;
            entry->accessedAt = now;
            cache->hotIndex = i;
            return (void *)entry + cache->dataOffset;
        }

        //  Prefer to use new entries instead of expired ones.

        if (entry->accessedAt == kSTZCacheNew) {
            spareIndex = i;
            break;
        }

        if (spareIndex == -1 && entry->accessedAt == kSTZCacheExpired) {
            spareIndex = i;
        }
    }

    if (spareIndex == -1) {
        int newCount = cache->count ? cache->count * 2 : 2;
        cache->entries = reallocf(cache->entries, newCount * cache->entrySize);

        for (int j = cache->count; j < newCount; ++j) {
            STZCacheEntryAtIndex(cache, j)->accessedAt = kSTZCacheNew;
        }

        spareIndex = cache->count;
        cache->count = newCount;
    }

    _STZCacheEntryStub *entry = STZCacheEntryAtIndex(cache, spareIndex);
    *outResult = entry->accessedAt == kSTZCacheExpired
        ? kSTZCScanCacheExpiredReused
        : kSTZCScanCacheNewCreated;
    entry->identifier = identifier;
    entry->accessedAt = now;
    cache->hotIndex = spareIndex;
    return (void *)entry + cache->dataOffset;
}


void STZCScanCacheRemoveAll(STZCScanCache *cache) {
    free(cache->entries);
    cache->entries = NULL;
    cache->count = 0;
    cache->hotIndex = 0;
    cache->checkedAt = 0;
}


void STZCScanCacheEnumerateData(STZCScanCache *cache, bool includeExpired, STZCacheEnumerationCallback callback, void *refcon) {
    for (int i = 0; i < cache->count; ++i) {
        _STZCacheEntryStub *entry = STZCacheEntryAtIndex(cache, i);

        if (entry->accessedAt == kSTZCacheNew) {continue;}

        bool expired = entry->accessedAt == kSTZCacheExpired;
        if (!includeExpired && expired) {continue;}

        void *data = (void *)entry + cache->dataOffset;
        callback(entry->identifier, data, expired, refcon);
    }
}


//  MARK: - CGEvent


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


STZFlags STZValidateFlags(uint32_t dirtyFlags, CFStringRef *outDescription) {
    if (dirtyFlags & kSTZMouseButtonsMask) {
       STZFlags flags = dirtyFlags & kSTZMouseButtonsMask;
       if (flags == 1) {flags = 0;}

       if (outDescription) {
           if (flags == kSTZMouseButtonMiddle) {
               *outDescription = CFSTR("\U0001f5b1 Mid");
           } else {
               static CFStringRef const format = CFSTR("\U0001f5b1 %u");
               CFStringRef desc = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, format, flags);
               *outDescription = CFAutorelease(desc);
           }
       }

       return flags;

    } else {
        STZFlags flags = dirtyFlags & ~kSTZMouseButtonsMask;

        if (outDescription) {
            static struct {
                uint16_t        symbol;
                STZFlags        flag;
            } const items[] = {
                {u'⌃', kSTZModifierControl},
                {u'⌥', kSTZModifierOption},
                {u'⇧', kSTZModifierShift},
                {u'⌘', kSTZModifierCommand},
            };

            static const size_t itemCount = sizeof(items) / sizeof(*items);

            uint16_t characters[itemCount];
            size_t characterCount = 0;

            for (size_t i = 0; i < itemCount; ++i) {
                if (flags & items[i].flag) {
                    characters[characterCount] = items[i].symbol;
                    characterCount += 1;
                }
            }

            CFStringRef desc = CFStringCreateWithCharacters(kCFAllocatorDefault, characters, characterCount);
            *outDescription = CFAutorelease(desc);
        }

        return flags;
    }
}


uint64_t STZSenderIDForEvent(CGEventRef event) {
    IOHIDEventRef hidEvent = CGEventCopyIOHIDEvent(event);
    if (hidEvent) {
        uint64_t senderID = IOHIDEventGetSenderID(hidEvent);
        CFRelease(hidEvent);
        return senderID;
    } else {
        return 0;
    }
}


void STZDebugLogEvent(char const *prefix, CGEventRef event) {
    if (!STZIsLoggingEnabled()) {return;}

    uint64_t senderID = STZSenderIDForEvent(event);
    CFStringRef flagDesc;
    STZValidateFlags(CGEventGetFlags(event) & kSTZModifiersMask, &flagDesc);

    if (CFStringGetLength(flagDesc) == 0) {
        flagDesc = CFSTR("no flags");
    }

    CGFloat data;
    STZPhase phase;
    bool byMomentum;
    CFStringRef desc;

    switch (CGEventGetType(event)) {
    case kCGEventFlagsChanged:
        STZDebugLog("%s flags changed from [%llx] with %@", prefix, senderID, flagDesc);
        break;

    case kCGEventScrollWheel:
        data = CGEventGetIntegerValueField(event, kCGScrollWheelEventPointDeltaAxis1);
        STZGetPhaseFromScrollWheelEvent(event, &phase, &byMomentum);
        char const *phaseTag = byMomentum ? "momentum" : "smooth";
        char const *tail = STZIsScrollWheelFlipped(event) ? ", flipped" : "";
        STZDebugLog("%s scroll wheel from [%llx] with %@, %s %s, moved %0.2fpx%s",
                    prefix, senderID, flagDesc, phaseTag, nameOfPhase(phase), data, tail);
        break;

    case kCGEventGesture:
        data = CGEventGetDoubleValueField(event, kCGGestureEventZoomValue);
        STZGetPhaseFromGestureEvent(event, &phase);
        STZDebugLog("%s zoom gesture from [%llx] with %@, gesture %s, scaled %0.02f%%",
                    prefix, senderID, flagDesc, nameOfPhase(phase), (1 + data) * 100);
        break;

    case kCGEventOtherMouseDown:
        STZValidateFlags((uint32_t)CGEventGetIntegerValueField(event, kCGMouseEventButtonNumber), &desc);
        STZDebugLog("%s mouse down from [%llx] of %@ button",
                    prefix, senderID, desc);

    case kCGEventOtherMouseUp:
        STZValidateFlags((uint32_t)CGEventGetIntegerValueField(event, kCGMouseEventButtonNumber), &desc);
        STZDebugLog("%s mouse up from [%llx] of %@ button",
                    prefix, senderID, desc);

    default:
        STZDebugLog("%s unexpected event from [%llx] with %@", prefix, senderID, flagDesc);
        break;
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
