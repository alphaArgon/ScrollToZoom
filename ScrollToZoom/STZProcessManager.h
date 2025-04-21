/*
 *  STZProcessManager.h
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/4/16.
 *  Copyright © 2025 alphaArgon.
 */

#import <CoreFoundation/CoreFoundation.h>


uint64_t STZRunningApplicationsSnapshotVersion(void);
CFStringRef __nullable STZGetBundleIdentifierForProcessID(pid_t pid);
