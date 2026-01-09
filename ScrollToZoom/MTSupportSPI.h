/*
 *  MTSupportSPI.h
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/4/25.
 *  Copyright © 2025 alphaArgon.
 */

#import <CoreFoundation/CoreFoundation.h>
#import <IOKit/IOKitLib.h>

CF_IMPLICIT_BRIDGING_ENABLED
CF_ASSUME_NONNULL_BEGIN


typedef struct CF_BRIDGED_TYPE(id) __MTDevice *MTDeviceRef;
MTDeviceRef __nullable MTDeviceCreateDefault(void);
CFArrayRef MTDeviceCreateList(void);

OSStatus MTDeviceStart(MTDeviceRef, int);
OSStatus MTDeviceStop(MTDeviceRef);
bool MTDeviceIsRunning(MTDeviceRef);


typedef CF_ENUM(uint32_t, MTDeviceFamilyID) {
    kMTDeviceFmailyMagicMouse = 112
};

OSStatus MTDeviceGetDeviceID(MTDeviceRef, uint64_t *);
OSStatus MTDeviceGetFamilyID(MTDeviceRef, MTDeviceFamilyID *);
bool MTDeviceIsBuiltIn(MTDeviceRef);


typedef uint32_t MTFrameID;


typedef CF_ENUM(uint32_t, MTTouchPhase) {

    /// Not used.
    kMTTouchPhaseNone,

    /// Magic Trackpad sends this as the first state; Magic Mouse sends on unknown situations.
    kMTTouchPhaseBegan,

    /// The finger is above the surface, and no tap is made yet.
    kMTTouchPhaseWillDown,

    /// The finger just tapped on the surface.
    kMTTouchPhaseDidDown,

    /// The finger is on the surface.
    kMTTouchPhaseMoved,

    /// The finger just raised from the surface.
    kMTTouchPhaseWillUp,

    /// The finger is above the surface, and any tap is made.
    kMTTouchPhaseDidUp,

    /// The finger is no longer recognized.
    kMTTouchPhaseEnded,
};

typedef struct {
    float   x, y;
} MTPoint;

typedef struct {
    MTFrameID       frame;
    uint32_t        _padding1[1];
    CFTimeInterval  timestamp;
    uint32_t        pathID;
    MTTouchPhase    phase;
    uint32_t        fingerID;
    uint32_t        _padding2[1];
    MTPoint         location;
    MTPoint         velocity;
    float           zTotal;
    uint32_t        _padding3[1];
    float           angle;
    float           majorAxis;
    float           minorAxis;
    MTPoint         locationMM;
    MTPoint         velocityMM;
    uint32_t        _padding4[2];
    float           zDensity;
} MTTouch;

typedef int (*MTContactCallback)(MTDeviceRef, MTTouch const *touches, CFIndex touchCount, CFTimeInterval, MTFrameID, void *refcon);
void MTRegisterContactFrameCallback(MTDeviceRef, MTContactCallback);
void MTRegisterContactFrameCallbackWithRefcon(MTDeviceRef, MTContactCallback, void *refcon);


MTDeviceRef __nullable MTDeviceCreateFromService(io_service_t);
io_service_t MTDeviceGetService(MTDeviceRef);

static inline kern_return_t MTDeviceGetRegistryID(MTDeviceRef device, uint64_t *registryID) {
    io_registry_entry_t entry = MTDeviceGetService(device);
    return IORegistryEntryGetRegistryEntryID(entry, registryID);
}


CF_ASSUME_NONNULL_END
CF_IMPLICIT_BRIDGING_DISABLED
