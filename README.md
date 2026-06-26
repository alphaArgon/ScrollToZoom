<small>**English (US)**・[简体中文（大陆）](README.zh_CN.md)</small>

# <img src="images/scroll_to_zoom.png" width="128" align="right"> Scroll to Zoom

A Mac utility for precisely zooming in or out using the mouse wheel.

## Features

- Hold modifier keys or an auxiliary mouse button to zoom.
- Customizable direction, speed, and per-application configuration.
- Compatible with other mouse optimization tools.

### <img src="images/magic_zoom.png" width="64" align="right"> Bonus for Magic Mouse users

You can double tap and scroll — that is, put your finger on the surface, lift it, put it down again, and move it up or down — to zoom in and out, just like how you zoom in Apple Maps on your iPhone. This works naturally if you also enable double-tap-to-smart-zoom in System Settings.

Previously, a specialized app, Magic Zoom, was developed solely for this feature. It’s now integrated into Scroll to Zoom. If you don’t need key- or button-triggered zooming, you can disable it in the settings.

## Overview

With the specified modifier keys held or an auxiliary mouse button pressed, you can perform precise zooming in any app with your mouse wheel. Scroll to Zoom converts mouse wheel events into simulated zoom gestures. The app is lightweight (~1 MB) and runs efficiently in the background.

<img src="images/window_v2.png" width="485" title="Screenshot of the main window">

Scroll to Zoom works seamlessly with other tools like [Mos](https://github.com/Caldis/Mos), [LinearMouse](https://github.com/linearmouse/linearmouse), or others you love. If that tool provides a smooth scrolling experience, Scroll to Zoom will benefit from it, giving a smooth zooming experience.

Regardless of how these tools configure the direction of scrolling, the zooming is always based on your finger movements. You can configure whether “fingers up” means zooming in or out in the app.

You can choose which apps Scroll to Zoom is enabled for. Additionally, if modifier keys break the normal zooming behavior, you can choose to remove them from the simulated events for that specific app. For example, Firefox won’t zoom with any modifier key down, so you can enable “Erase Modifier Keys” for it in the settings.

## Localization

English, Spanish, French, German, Russian, Japanese, Simplified Chinese (the Mainland), and Traditional Chinese (Taiwan) are supported. However, the maintainer is not familiar with all languages, and UI translations took him a lot of effort, so the README is only available in English and Simplified Chinese. Contributions to localization are welcome.

## Installation

You’ll need:
- An Intel or Apple Silicon Mac,
- macOS 10.13 High Sierra or later, and
- A pointing device that supports scrolling.

Download the [latest release](https://github.com/alphaArgon/ScrollToZoom/releases) and move the unarchived content into the Applications folder. On macOS Sequoia or later, you need to run the following command in Terminal to bypass Gatekeeper if you think you trust this app:

```sh
xattr -dr com.apple.quarantine /Applications/Scroll\ to\ Zoom.app
```

Make sure the path (/Applications/Scroll to Zoom.app) matches the actual location of the app and spaces are escaped. For the first launch, right-click the app and choose Open from the context menu. You may also need to grant the required permissions as prompted.

Because this app uses some protected technologies, it cannot be sandboxed and distributed on the Mac App Store.

## Technologies

The core functionality is written in C for ultimate performance. Since this app supports down to High Sierra, where Swift ABI is not stable and an external runtime library is required, the app is written in Objective-C.

This app has a built-in logging panel. Open the settings window while holding the Option key (⌥). A ladybug button will appear in the bottom-left corner. Clicking it opens the logging panel. For performance reasons, logging is enabled only if the panel is shown, and the panel records up to a thousand recent logs.

## Known Issues

- Mozilla Firefox: Described in Overview.
- Google Chrome: Zoom events only take effect after the accumulated scale exceeds a certain threshold. This is a design decision of Chrome.
- Sketch: Zoom events with a small scale are ignored. This is a design decision of Sketch.

You are welcome to report issues and contribute to this project

## License

The source code of Scroll to Zoom is released under the MIT license. See [LICENSE](LICENSE) for details. The app icon and status bar image are created from glyphs of the font *Zapfino* shipped with macOS, which may have a different license. Other assets (images, fonts, etc.) are copyrighted by the owner of this repository, which means you cannot use them in your own distribution without permission.
