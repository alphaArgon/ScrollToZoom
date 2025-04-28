/*
 *  STZProcessManager.h
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/4/16.
 *  Copyright © 2025 alphaArgon.
 */

#import <CoreFoundation/CoreFoundation.h>

CF_IMPLICIT_BRIDGING_ENABLED
CF_ASSUME_NONNULL_BEGIN


uint64_t STZRunningApplicationsSnapshotVersion(void);
CFStringRef __nullable STZGetBundleIdentifierForProcessID(pid_t pid);
CFURLRef __nullable STZGetInstalledURLForBundleIdentifier(CFStringRef);


CF_ASSUME_NONNULL_END
CF_IMPLICIT_BRIDGING_DISABLED
