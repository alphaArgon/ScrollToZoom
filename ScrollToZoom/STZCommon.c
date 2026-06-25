/*
 *  STZCommon.c
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/4/24.
 *  Copyright © 2025 alphaArgon.
 */

#include "STZCommon.h"
#include "CGEventSPI.h"


STZFlags STZFlagsValidate(uint32_t dirtyFlags) {
    if (dirtyFlags & kSTZMouseButtonsMask) {
        STZFlags flags = dirtyFlags & kSTZMouseButtonsMask;
        return flags == 1 ? 0 : flags;
    } else {
        return dirtyFlags & kSTZModifiersMask;
    }
}


CFStringRef STZFlagsCopyDescription(uint32_t anyFlags) {
    if (anyFlags & kSTZMouseButtonsMask) {
        if (anyFlags == kSTZMouseButtonMiddle) {
            return CFRetain(CFSTR("\U0001f5b1 Mid"));
        } else {
            return CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("\U0001f5b1 %u"), anyFlags);
        }
    }

    static struct {
        uint16_t    symbol;
        STZFlags    flag;
    } const items[] = {
        {u'⌃', kSTZModifierControl},
        {u'⌥', kSTZModifierOption},
        {u'⇧', kSTZModifierShift},
        {u'⌘', kSTZModifierCommand},
        {u'⇪', kCGEventFlagMaskAlphaShift},
    };

    static const size_t itemCount = sizeof(items) / sizeof(*items);
    uint16_t characters[itemCount + 2];
    size_t characterCount = 0;

    for (size_t i = 0; i < 5; ++i) {
        if (anyFlags & items[i].flag) {
            characters[characterCount] = items[i].symbol;
            characterCount += 1;
        }
    }

    if (anyFlags & kSTZModifierFn) {
        if (characterCount) {
            characters[characterCount++] = u'\u200a';
        }
        characters[characterCount++] = u'f';
        characters[characterCount++] = u'n';
    }

    return CFStringCreateWithCharacters(kCFAllocatorDefault, characters, characterCount);
}


//  MARK: -


static void noop(void *ptr) {}


static inline int div_ceil(int a, int b) {
    return (a + b - 1) / b;
}


struct _STZCache {
    int                 count;
    int                 recentIndex;
    CGEventTimestamp    valueLifetime;
    int                 valueSize;
    int                 entrySize;
    void               *entries;
    void (*valueDisposeCallback)(void *valueAddr);
    uint8_t             inlinePayload[80];
};


typedef struct {
    uint64_t            key;
    CGEventTimestamp    accessedAt;  ///< 0 means not used.
} _STZCacheEntryStub;


_STZCacheEntryStub *STZCacheGetEntryAtIndex(STZCacheRef cache, int i) {
    return (_STZCacheEntryStub *)(cache->entries + cache->entrySize * i);
}


STZCacheRef STZCacheCreate(size_t valueSize, CGEventTimestamp valueLifetime, void (*valueDisposeCallback)(void *valueAddr)) {
    STZCacheRef cache = malloc(sizeof(*cache));
    cache->recentIndex = 0;
    cache->valueLifetime = valueLifetime;
    cache->valueSize = (int)valueSize;
    cache->entrySize = (1 + div_ceil((int)valueSize, sizeof(_STZCacheEntryStub))) * sizeof(_STZCacheEntryStub);
    cache->entries = cache->inlinePayload;
    cache->valueDisposeCallback = valueDisposeCallback ?: noop;

    cache->count = sizeof(cache->inlinePayload) / cache->entrySize;
    for (int i = 0; i < cache->count; ++i) {
        STZCacheGetEntryAtIndex(cache, i)->accessedAt = 0;
    }

    return cache;
}


void STZCacheRelease(STZCacheRef cache) {
    for (int i = 0; i < cache->count; ++i) {
        _STZCacheEntryStub *entry = STZCacheGetEntryAtIndex(cache, i);
        if (entry->accessedAt != 0) {
            cache->valueDisposeCallback(&entry[1]);
        }
    }
    if (cache->entries != cache->inlinePayload) {
        free(cache->entries);
    }
    free(cache);
}


void *STZCacheGetValueForKey(STZCacheRef cache, uint64_t key, bool *outCreatedIfAbsent) {
    CGEventTimestamp now = CGEventTimestampNow();

    int spareIndex = -1;

    for (int h = 0; h < cache->count; ++h) {
        int i = (cache->recentIndex + h) % cache->count;
        _STZCacheEntryStub *entry = STZCacheGetEntryAtIndex(cache, i);

        if (entry->accessedAt == 0) {
            spareIndex = i;

        } else if ((now - entry->accessedAt) >= cache->valueLifetime) {
            if (spareIndex == -1) {
                spareIndex = i;
            }

        } else if (entry->key == key) {
            entry->accessedAt = now;
            cache->recentIndex = i;

            if (outCreatedIfAbsent) {
                *outCreatedIfAbsent = false;
            }
            return &entry[1];
        }
    }

    if (!outCreatedIfAbsent) {
        return NULL;
    }

    *outCreatedIfAbsent = true;

    if (spareIndex == -1) {
        int newCount = (double)cache->count * 1.5 + 1;

        if (cache->entries == cache->inlinePayload) {
            cache->entries = malloc(newCount * cache->entrySize);
            memcpy(cache->entries, cache->inlinePayload, sizeof(cache->inlinePayload));
        } else {
            cache->entries = realloc(cache->entries, newCount * cache->entrySize);
        }

        for (int i = cache->count; i < newCount; ++i) {
            STZCacheGetEntryAtIndex(cache, i)->accessedAt = 0;
        }

        spareIndex = cache->count;
        cache->count = newCount;
    }

    _STZCacheEntryStub *entry = STZCacheGetEntryAtIndex(cache, spareIndex);
    if (entry->accessedAt != 0) {
        cache->valueDisposeCallback(&entry[1]);
    }

    entry->key = key;
    entry->accessedAt = now;
    cache->recentIndex = spareIndex;
    return &entry[1];
}


void *__nullable STZCacheGetRecentValue(STZCacheRef cache, uint64_t *__nullable outKey) {
    if (cache->count == 0) {return NULL;}

    _STZCacheEntryStub *entry = STZCacheGetEntryAtIndex(cache, cache->recentIndex);
    if (entry->accessedAt == 0) {return NULL;}

    CGEventTimestamp now = CGEventTimestampNow();
    if ((now - entry->accessedAt) >= cache->valueLifetime) {return NULL;}

    if (outKey != NULL) {
        *outKey = entry->key;
    }
    return &entry[1];
}


void *STZCacheGetValue(STZCacheRef cache, uint64_t key) {
    return STZCacheGetValueForKey(cache, key, NULL);
}


void *STZCacheSetValue(STZCacheRef cache, uint64_t key, void const *valueAddr) {
    bool newlyCreated;
    void *valueDst = STZCacheGetValueForKey(cache, key, &newlyCreated);
    if (!newlyCreated) {
        cache->valueDisposeCallback(valueDst);
    }
    memcpy(valueDst, valueAddr, cache->valueSize);
    return valueDst;
}


void STZCacheRemoveAll(STZCacheRef cache) {
    for (int i = 0; i < cache->count; ++i) {
        _STZCacheEntryStub *entry = STZCacheGetEntryAtIndex(cache, i);
        if (entry->accessedAt != 0) {
            entry->accessedAt = 0;
            cache->valueDisposeCallback(&entry[1]);
        }
    }
}


void STZCacheEnumerateValues(STZCacheRef cache, void (*valueEnumerateCallback)(void *valueAddr, void *context), void *context) {
    CGEventTimestamp now = CGEventTimestampNow();

    for (int i = 0; i < cache->count; ++i) {
        _STZCacheEntryStub *entry = STZCacheGetEntryAtIndex(cache, i);
        if (entry->accessedAt == 0) {continue;}
        if ((now - entry->accessedAt) >= cache->valueLifetime) {continue;}
        valueEnumerateCallback(&entry[1], context);
    }
}


//  MARK: -

//  `CGGesturePhase` and `CGScrollPhase` are compatible.

static char const *commaPhaseName(CGGesturePhase phase) {
    switch (phase) {
    case kCGGesturePhaseNone:       return ", no phase";
    case kCGGesturePhaseBegan:      return ", gesture began";
    case kCGGesturePhaseChanged:    return ", gesture changed";
    case kCGGesturePhaseEnded:      return ", gesture ended";
    case kCGGesturePhaseCancelled:  return ", gesture cancelled";
    case kCGGesturePhaseMayBegin:   return ", gesture may begin";
    default:                        return ", phase unknown";
    }
}


static char const *commaMomentumPhaseName(CGMomentumScrollPhase phase) {
    switch (phase) {
    case kCGMomentumScrollPhaseNone:        return ", no momentum";
    case kCGMomentumScrollPhaseBegin:       return ", inertia began";
    case kCGMomentumScrollPhaseContinue:    return ", inertia changed";
    case kCGMomentumScrollPhaseEnd:         return ", inertia ended";
    default:                                return ", momentum unknown";
    }
}


void STZUnknownEnumCase(char const *type, int64_t value) {
    if (!STZIsLoggingEnabled()) {return;}
    STZDebugLog("Unknown enum %s case %lld", type, value);
}


void STZDebugLogEvent(char const *prefix, CGEventRef event) {
    if (!STZIsLoggingEnabled()) {return;}

    uint64_t senderID = CGEventGetRegistryID(event);
    CFStringRef flagDesc = STZFlagsCopyDescription(CGEventGetFlags(event) & kSTZPrintableModifiersMask);
    CFStringRef spaceFlagDesc;

    if (CFStringGetLength(flagDesc) == 0) {
        spaceFlagDesc = CFRetain(CFSTR(""));
    } else {
        spaceFlagDesc = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR(" with %@"), flagDesc);
    }

    CFRelease(flagDesc);

    int64_t i;
    double f;
    CGGesturePhase phase;
    CFStringRef buttonDesc;

    switch (CGEventGetType(event)) {
    case kCGEventFlagsChanged:
        STZDebugLog("%s flags changed [%llx]%@", prefix, senderID, spaceFlagDesc);
        break;

    case kCGEventScrollWheel:
        i = CGEventGetIntegerValueField(event, kCGScrollWheelEventPointDeltaAxis1);
        f = CGEventGetDoubleValueField(event, kCGScrollWheelEventFixedPtDeltaAxis1);

        phase = (uint32_t)CGEventGetIntegerValueField(event, kCGScrollWheelEventScrollPhase);
        CGMomentumScrollPhase mPhase = (uint32_t)CGEventGetIntegerValueField(event, kCGScrollWheelEventMomentumPhase);

        char const *commaPhase = "";
        char const *commaMPhase = "";
        if (phase && mPhase) {
            commaPhase = commaPhaseName(phase);
            commaMPhase = commaMomentumPhaseName(mPhase);
        } else if (mPhase) {
            commaMPhase = commaMomentumPhaseName(mPhase);
        } else {
            commaPhase = commaPhaseName(phase);
        }

        char const *tail = CGEventGetIntegerValueField(event, kCGScrollEventIsDirectionInverted) ? ", flipped" : "";
        STZDebugLog("%s scroll wheel [%llx]%@%s%s, by %lld or %0.1f px%s",
                    prefix, senderID, spaceFlagDesc, commaPhase, commaMPhase, i, f, tail);
        break;

    case kCGEventGesture:
        f = CGEventGetDoubleValueField(event, kCGGestureEventZoomValue);
        phase = (CGGesturePhase)CGEventGetIntegerValueField(event, kCGGestureEventPhase);
        STZDebugLog("%s zoom gesture [%llx]%@%s, scaled to %0.02f%%",
                    prefix, senderID, spaceFlagDesc, commaPhaseName(phase), (1 + f) * 100);
        break;

    case kCGEventOtherMouseDown:
        buttonDesc = STZFlagsCopyDescription((uint32_t)CGEventGetIntegerValueField(event, kCGMouseEventButtonNumber));
        STZDebugLog("%s mouse down [%llx]%@ of %@ button",
                    prefix, senderID, spaceFlagDesc, buttonDesc);
        CFRelease(buttonDesc);
        break;

    case kCGEventOtherMouseUp:
        buttonDesc = STZFlagsCopyDescription((uint32_t)CGEventGetIntegerValueField(event, kCGMouseEventButtonNumber));
        STZDebugLog("%s mouse up [%llx]%@ of %@ button",
                    prefix, senderID, spaceFlagDesc, buttonDesc);
        CFRelease(buttonDesc);
        break;

    default:
        STZDebugLog("%s unknown event [%llx]%@", prefix, senderID, spaceFlagDesc);
        break;
    }

    CFRelease(spaceFlagDesc);
}
