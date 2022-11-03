---
layout: default
title: Other
parent: Features
nav_order: 7
---

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## World scale override

The world scale override can be used to change the relative position of the eye cameras (also known as the Inter-Camera Distance, or ICD). This can affect your perception of the size of the world in VR. Reducing the scale (ie: a scale under 100%) will make the world appear smaller, while increasing the scale (ie: a scale above 100%) will make the world appear bigger.

## Shaking reduction

Previously known as "prediction dampening" this setting allows to reduce the jitter that can be observed when head or controller is predicted "too long in advance". This often manifests as "over-sensitivity" with your head movements. For example, some people have reported the view in the headset shaking with their heartbeats. That is an example of over-sensitivity. Set a negative value to reduce the amount of prediction and reduce shaking, for example -50% will cut in half the requested amount of prediction. Experimentally, best results have been achieved with values between -20% and -40%.

## Brightness, contrast and saturation

The OpenXR Toolkit applies some simple post-processing to adjust the brightness, contrast and saturation of the images displayed in the headset. The way these settings affect the image is comparable to the settings found on your TV or computer monitor. They are applied on the rendered images as a whole, and are subject to limitations due to color encoding.

Each setting has a default value of 50, which means no changes to the game's output. The saturation adjustments can be applied to all 3 color channels (red, green, blue) or individually for each channel.

## Turbo mode

**Note:** This is a highly experimental feature. If it makes your game crash or misbehave, please disable it.

Turbo mode makes the game entirely ignore any frame timing or throttling dictated by the OpenXR runtime. In other words, Turbo mode forces the game to draw and submit frames as fast as possible regardless of any recommendations or policies from the OpenXR runtime.

Turbo mode _can_ increase your frame rate under certain conditions, but there is no rule or guarantee.

Turbo mode can also have undesired side effects:
- Any statistics or performance measuring tool might now present incorrect data. For example, on WMR, when using the performance overlay, all app CPU/GPU values will read 0.
- Any feature of your platform that relies on frame timing data, such as Motion Reprojection/Motion Smoothing/ASW, will not behave correctly. For example, on WMR, the motion reprojection will not engage automatically anymore, but it can be forced via the [Lock motion reprojection](#lock-motion-reprojection) option.
- Latency is increased, in a way that may or may not visible.

**Note:** Turbo mode is offered as-is. There will be no support for issues filed while using Turbo mode.

## Motion reprojection

**Note:** This settings is only available to Windows Mixed Reality users.

The _Motion Reprojection_ setting is essentially the same as the motion reprojection setting from the [OpenXR Tools for Windows Mixed Reality](https://www.microsoft.com/en-us/p/openxr-tools-for-windows-mixed-reality/9n5cvvl23qbt). The key differences are that:

1. The OpenXR Toolkit _Motion reprojection_ setting overrides the setting from OpenXR Tools for Windows Mixed Reality.
2. The OpenXR Toolkit _Motion reprojection_ setting is saved per-application, rather than globally.
3. OpenXR Toolkit _Motion reprojection_ setting set to _On_ will enable use of the the _Lock motion reprojection_ feature.

If set to _Default_, the _Motion reprojection_ setting in OpenXR Toolkit does nothing, and the setting from OpenXR Tools for Windows Mixed Reality is used instead.

**Note:** The motion reprojection setting "Automatic" in OpenXR Tools for Windows Mixed Reality is misleading: it is equivalent to "Always on" but only for Microsoft Flight Simulator 2020. There is no equivalent in OpenXR Toolkit, using _On_ is equivalent to "Always on" from OpenXR Tools for Windows Mixed Reality.

Summary:

| OpenXR Toolkit | OpenXR Tools for Windows Mixed Reality | Result |
| --- | --- | --- |
| Default | Disabled | Disabled |
| Default | Automatic | Enabled only in FS2020 |
| Default | Always on | Enabled |
| Off | _(don't care)_ | Disabled |
| On | _(don't care)_ | Enabled |

Unless the _Lock motion reprojection_ option described below is used, Motion Reprojection will always attempt to select the best achievable frame rate. If motion reprojection is not needed, the game will freely render at 60 or 90 Hz depending on your configured refresh rate. If motion reprojection is needed, the runtime will force 1/half, 1/third or 1/quarter frame rate as appropriate.

## Lock motion reprojection

**Note:** This settings is only available to Windows Mixed Reality users.

The _Lock motion reprojection_ setting can be used to force the motion reprojection rate, rather than let the OpenXR runtime automatically choose the rate based on the current performance. This is useful if you are experiencing large fluctuations in frame rate, and prefer to lock the frame rate to a smaller (but steadier) value. The motion reprojection rate is a fraction of the headset's refresh rate, for example with 90 Hz refresh rate, the available rates are 1/half (45 FPS), 1/third (30 FPS) and 1/quarter (22.5 FPS).

## Frame rate throttling.

This option can be used to throttle down the frame rate of the application and avoid fluctuations in frame rates. This option may introduce unwanted latency, since the locked frame rate might not properly phase with the scan-out of the headset.

This option will not appear for Windows Mixed Reality users if the _Motion Reprojection_ is forced to _On_ in the _System tab_. You must use the _Lock motion reprojection_ instead (see above). This is because the Motion Reprojection rate locking is preferred, since it is latency-aware and produces better results.

## Field of view

The field of view override adjusts the pixel density per degree. A smaller field of view is covering a smaller region of the view but with the same amount of pixels, effectively increasing the perceived resolution. Two levels of controls are available: simple and advanced. The former adjusts the fied of view for both eyes simulatneously, while the latter offers individual controls per-eye.

## Screen capture

In order to activate this feature, you must check the _Enable screenshot_ box in the _OpenXR Toolkit Companion app_, and select an image format for the screenshots. The OpenXR ToolKit supports the following formats: [DDS](https://en.wikipedia.org/wiki/DirectDraw_Surface)*, [PNG](https://en.wikipedia.org/wiki/Portable_Network_Graphics), [JPG](https://en.wikipedia.org/wiki/JPEG) and [BMP](https://en.wikipedia.org/wiki/BMP_file_format). 

You may then press `Ctrl+F12` to take a screenshot of the left-eye image view. Screenshots are saved under `%LocalAppData%\OpenXR-Toolkit\screenshots`. This folder may be opened from the _OpenXR Toolkit Companion app_ by clicking the _Open screenshots folder_ button.

![Screenshots](site/screenshot.png)<br>
*Enabling and accessing screenshots*

**The DDS format is a lossless format native to DirectX but some tools might have issues opening DDS files. The tools that were confirmed to properly open them with the OpenXR Toolkit are [GIMP](https://www.gimp.org/) and [Paint.net](https://github.com/paintdotnet/release/releases)*

## Record statistics

This option can be enable to continuously record some statistics from the application to a comma-separated values (CSV) file stored under `%LocalAppData%\OpenXR-Toolkit\stats`.

When this option is toggled from off to on, a new file is created, with a file name including the date/time (eg: `stats_20220822_183013.csv` for a recording on August 22nd, 2022 at 18:30:13).

The CSV file contains rows with the average FPS, frame times, and VRAM utilization over one second. Every second, a new row is appended.
