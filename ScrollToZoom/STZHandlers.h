/*
 *  STZHandlers.h
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/1/25.
 *  Copyright © 2025 alphaArgon.
 */

#import <CoreFoundation/CFString.h>


bool STZIsLoggingEnabled(void);
void STZDebugLog(char const *message, ...) CF_FORMAT_FUNCTION(1, 2);
void STZUnknownEnumCase(char const *type, int64_t value);
