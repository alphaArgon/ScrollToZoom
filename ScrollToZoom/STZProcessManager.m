/*
 *  STZProcessManager.m
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/4/16.
 *  Copyright © 2025 alphaArgon.
 */

#import "STZProcessManager.h"
#import <Foundation/Foundation.h>
#import <AppKit/NSRunningApplication.h>


#define CACHE_CAPACITY 32


__attribute__((objc_direct_members))
@interface STZBundleIdentifierManager : NSObject

+ (STZBundleIdentifierManager *)sharedManager;
- (uint64_t)runningApplicationsSnapshotVersion;
- (NSString *__nullable)bundleIdentifierForProcessID:(pid_t)pid;

@end


@implementation STZBundleIdentifierManager {
    //  `pid_t` on macOS is an integer and an NSNumber can be a tagged pointer, which is efficient.
    NSCache        *_caches;
    NSWorkspace    *_workspace;
    uint64_t        _snapshotVersion;
}

static void *STZRunningApplicationsKVO = &STZRunningApplicationsKVO;

- (void)dealloc {
    [_workspace removeObserver:self
                    forKeyPath:@"runningApplications"
                       context:STZRunningApplicationsKVO];
}

- (instancetype)init {
    self = [super init];
    _caches = [[NSCache alloc] init];
    [_caches setCountLimit:CACHE_CAPACITY];

    _workspace = [NSWorkspace sharedWorkspace];
    [_workspace addObserver:self
                forKeyPath:@"runningApplications"
                   options:NSKeyValueObservingOptionOld
                   context:STZRunningApplicationsKVO];

    return self;
}

+ (instancetype)sharedManager {
    static STZBundleIdentifierManager *shared = nil;
    if (!shared) {
        shared = [[STZBundleIdentifierManager alloc] init];
    }
    return shared;
}

- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary *)change
                       context:(void *)context {
    if (context != STZRunningApplicationsKVO) {
        return [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
    }

    _snapshotVersion += 1;

    if (![[change valueForKey:NSKeyValueChangeKindKey] isEqualToNumber:@(NSKeyValueChangeRemoval)]) {
        return;
    }

    for (NSRunningApplication *app in [change valueForKey:NSKeyValueChangeOldKey]) {
        NSNumber *key = @([app processIdentifier]);
        [_caches removeObjectForKey:key];
    }
}

- (uint64_t)runningApplicationsSnapshotVersion {
    return _snapshotVersion;
}

- (NSString *)bundleIdentifierForProcessID:(pid_t)pid {
    NSNumber *key = @(pid);

    NSString *bundleID = [_caches objectForKey:key];
    if (bundleID) {return bundleID;}

    NSRunningApplication *app = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
    bundleID = [app bundleIdentifier];
    if (!bundleID) {return nil;}

    [_caches setObject:bundleID forKey:key];
    return bundleID;
}

@end


uint64_t STZRunningApplicationsSnapshotVersion(void) {
    return [[STZBundleIdentifierManager sharedManager] runningApplicationsSnapshotVersion];
}


CFStringRef __nullable STZGetBundleIdentifierForProcessID(pid_t pid) {
    return (__bridge void *)[[STZBundleIdentifierManager sharedManager] bundleIdentifierForProcessID:pid];
}
