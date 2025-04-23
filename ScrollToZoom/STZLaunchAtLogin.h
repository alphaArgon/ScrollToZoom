/*
 *  STZLaunchAtLogin.h
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/4/19.
 *  Copyright © 2025 alphaArgon.
 */

#import <CoreFoundation/CoreFoundation.h>


bool STZShouldEnableLaunchAtLogin(void);

bool STZGetLaunchAtLoginEnabled(void);
bool STZSetLaunchAtLoginEnabled(bool);  ///< Returns whether the given value equals the state after calling this function.
