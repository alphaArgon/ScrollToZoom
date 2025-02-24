/*
 *  STZHandlers.m
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/1/25.
 *  Copyright © 2025 alphaArgon.
 */

#import "STZHandlers.h"
#import <Foundation/Foundation.h>


void STZDebugLog(char const *message, ...) {
    va_list args;
    va_start(args, message);
    NSLogv([NSString stringWithCString:message encoding:NSASCIIStringEncoding], args);
    va_end(args);
}


void STZUnknownEnumCase(char const *type, int64_t value) {
    //  TODO: Alert the user to update the app.
}
