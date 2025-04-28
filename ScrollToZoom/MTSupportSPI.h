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

typedef struct {
    float   x, y;
} MTPoint;

typedef struct {
    MTFrameID       frame;
    CFTimeInterval  timestamp;
    uint32_t        unknown[4];
    MTPoint         location;
    MTPoint         velocity;
} MTTouch;

typedef int (*MTContactCallback)(MTDeviceRef, MTTouch const *touch, CFIndex touchCount, CFTimeInterval, MTFrameID);
void MTRegisterContactFrameCallback(MTDeviceRef, MTContactCallback);


MTDeviceRef __nullable MTDeviceCreateFromService(io_service_t);
io_service_t MTDeviceGetService(MTDeviceRef);

static inline kern_return_t MTDeviceGetRegistryID(MTDeviceRef device, uint64_t *registryID) {
    io_registry_entry_t entry = MTDeviceGetService(device);
    return IORegistryEntryGetRegistryEntryID(entry, registryID);
}


CF_ASSUME_NONNULL_END
CF_IMPLICIT_BRIDGING_DISABLED
