/*
 *  main.m
 *  ScrollToZoomHelper
 *
 *  Created by alpha on 2025/2/12.
 *  Copyright © 2025 alphaArgon.
 */

#import <AppKit/AppKit.h>


NSString *const mainBundleIdentifier = @"red.argon.scrolltozoom";


int main(int argc, char const **argv) {
    NSString *mainPath = [[NSBundle mainBundle] bundlePath];

    do {
        mainPath = [mainPath stringByDeletingLastPathComponent];
    } while ([mainPath length] > 1 && ![mainPath hasSuffix:@".app"]);

    if ([mainPath length] <= 1) {
        NSLog(@"Cannot found the main application bundle.");
        return 1;
    }

    NSBundle *mainBundle = [NSBundle bundleWithPath:mainPath];
    if (!mainBundle) {
        NSLog(@"No permission to load the main application bundle.");
        return 1;
    }

    if (![[mainBundle bundleIdentifier] isEqualToString:mainBundleIdentifier]) {
        NSLog(@"Main application bundle identifier doesn’t match");
        return 1;
    }

    NSError *error;
    [[NSWorkspace sharedWorkspace] launchApplicationAtURL:[mainBundle bundleURL] options:0 configuration:@{} error:&error];

    if (error) {
        NSLog(@"Failed to launch the main application");
        return 1;
    }

    return 0;
}
