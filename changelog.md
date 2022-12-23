---
layout: default
title: Changelog
nav_order: 7
---

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Changes in v1.2 (Sep 2022 / v1.2.0 -)

- Fix support for iRacing, War Thunder and F1 2022.
- Fix support for X-Plane 11 and X-Plane 12 (only in conjunction with [OpenXR-Vk-D3D12](https://github.com/mbucchia/OpenXR-Vk-D3D12) on Windows Mixed Reality only, and no support for foveated rendering).
- Fix support for Unreal Engine OpenXR plugin and Unity OpenXR plugin.
- Fix crash with DX12 and MSFS/SU10.
- Fix Eye-tracked Foveated Rendering with MSFS/SU10.
- Fix WMR motion reprojection on/off setting not sticking across leaving/re-entering VR.
- Fix WMR motion reprojection issues when used with Shaking Reduction.
- Fix "invisible menu" issue with DX12 and Varjo.
- Fix incompatibility between eye tracking and controller/hand tracking on Varjo.
- Fix detection of hand tracking with PimaxXR.
- Fix issue when RGB color correction is not applied when post-processing is off.
- Fix issue with hand tracking in DCS.
- Fix upside-down image issue with certain OpenComposite games.
- Add support for both eye-tracked foveated rendering and fixed foveated rendering in Assetto Corsa Competizione.
- Add new "tutorial menu" for first time-use, will help debugging issues with users not able to bring up the menu.
- Add option to alter filtering policy with foveated rendering (possibly eliminate unwanted glitches with certain games).
- Support writing FPS/frame times to a CSV file (saved under `%LocalAppData%\OpenXR-Toolkit\stats`).
- Add frame rate throttling for all headsets.
- Added VRAM statistics in the overlay.
- Add "legacy menu" mode that reverts the menu to pre-1.1.2, for people not able to bring the menu or having performance issues with the menu.
- Add visual warning in the menu when HAGS is turned on.
- Add option to disable hidden area mesh (only supported on certain applications).
- Add left/right blind eye option (only supported on certain applications).
- Add clock to the overlay.
- Include overlay/menu in screenshots.
- Removed Pimax WFOV hack.
- Make safe mode even safer.
- Hotfix #1: Add ["Turbo mode"](other-features#turbo-mode).
- Hotfix #1: Add support for AMD FidelityFX Contrast Adaptive Sharpening (CAS).
- Hotfix #1: Fix support for HTC Vive Cosmos and Vive Focus (to use native OpenXR runtime rather than SteamVR).
- Hotfix #1: Fix app CPU frame times measurements.
- Hotfix #1: Fix crash with Bonelab on Oculus devices.
- Hotfix #1: Fix visual glitches with foveated rendering in certain games (eg: Assetto Corsa).
- Hotfix #1: Improve the frame throttling option to reduce frame latency.
- Hotfix #1: Fix Pimax eye tracker module support to work with newer eye tracker software.
- Hotfix #1: Fix issue with World Scale when application submits depth information.
- Hotfix #1: Display the "tutorial" menu after changing hotkeys and/or reinstalling OpenXR Toolkit.
- Hotfix #1: Fix crash in DCS when hand tracking is enabled.
- Hotfix #1: Add an option to display and record statistics a higher rate (10 Hz vs 1 Hz).
- Hotfix #1: Display frame times in milliseconds.
- Hotfix #1: Add a way to toggle statistics recording from the command-line tool.
- Hotfix #1: Add an indicator in the overlay that statistics recording is on.
- Hotfix #2: Delete bogus registry keys that can break the OpenXR Loader (eg: caused by XRNeckSafer installer).
- Hotfix #2: Remove resolution cap on Varjo.
- Hotfix #3: Fix Turbo Mode crashes in Flight Simulator 2020.
- Hotfix #3: Add a foveated rendering option to cull hidden area mesh (mostly for iRacing, only supported on certain applications).
- Hotfix #3: Workaround to OpenComposite issue with DCS on WMR (timeout error).
- Hotfix #3: Workaround to Oculus OpenXR issue with OpenComposite and ACC (call out of order).
- Hotfix #3: Do not display CPU frame time in the overlay when it cannot be determined reliably.
- Hotfix #3: Add a "target frame rate" option to have the overlay give headroom based on a desired frame rate.
- Hotfix #3: Fix screenshot showing both eyes with certain applications (eg: iRacing).

## Changes in v1.1 (Apr 2022 / v1.1.0 - v1.1.4)

- Add support for Foveated Rendering with eye tracking (sometimes called ETFR or DFR) on Varjo devices, HP G2 Omnicept, and Pimax devices with extension modules.
- Add support for adjusting exposure, vibrance, highlights, and shadows (post-processing)
- Add quick access presets (post-processing): _Sun Glasses_ _(light/dark)_ and _TruNite_ _(night flying)_
- Add support to disable image post-processing entirely.
- Improve the sharpening of AMD FidelityFX Super Resolution (FSR).
- Add CPU-bound indicator and CPU headroom measurements in the overlay.
- Add support for left/right eye biasing with Fixed Foveated Rendering (allow for lower resolution in one eye).
- Properly handle Fixed Foveated Rendering with lower in-game render scale (no lower than 51%).
- Add trigger-on-haptics to the hand tracking support (simulate a button press upon game haptics + programmed gesture).
- Add an option to override the OpenXR target resolution for each application.
- Add an option to enable/disable Motion Reprojection for each application (Windows Mixed Reality only).
- Add an option to disable the toolkit for each application (from the Companion app).
- Add an option to disable timeout in the menu.
- Add detailed traces capture from the Companion app (for troubleshooting).
- Save screenshot for both eyes.
- Hotfix #1: Add command-line tool to modify settings on-the-fly.
- Hotfix #1: Add zoom feature.
- Hotfix #1: Add option to select which eye to take a screenshot of.
- Hotfix #1: Make the overlay position customizable.
- Hotfix #1: Fix the menu display on Pimax.
- Hotfix #1: Fix eye tracking detection on G2 Omnicept.
- Hotfix #1: Fix various crashes with OpenComposite (DCS, ACC, IL2).
- Hotfix #1: Fix issue with OpenComposite and Oculus devices.
- Hotfix #1: Fix issue with OpenComposite and missing menu/loading screens.
- Hotfix #1: Fix issue with OpenComposite and upside-down menu.
- Hotfix #2: Fix a crash with OpenXR Tools for Windows Mixed Reality.
- Hotfix #3: Fix visual issues introduced in 1.1.2 for WMR users.
- Hotfix #3: Fix NIS/FSR support with OpenComposite.
- Hotfix #3: Add an option to change opacity of the menu.
- Hotfix #4: Fix Pimax WFOV hack issue introduced in 1.1.1.

## Changes in Beta #3 (Mar 2022 / v1.0.1 - 1.0.5)

- Add support for Fixed Foveated Rendering (FFR) (with selected GPUs only).
- Add support for anamorphic upscaling (independent upscaling of horizontal and vertical resolution).
- Add support for adjusting brightness, contrast and color saturation.
- Add an option to lock the motion reprojection rate (Windows Mixed Reality only).
- Add an option to only display the menu in one eye.
- Add an option to save screenshots to different file formats.
- Add a configurable hotkey to move up in the menu.
- Add mip-map Level Of Detail (LOD) biasing when using upscaling (changes texture details slightly).
- Hand tracking is confirmed to work with Oculus Quest 2 (see [instructions](hand-tracking)).
- Re-design the in-headset, on-screen menu.
- Move the Field Of View (FOV) override feature out of experimental mode.
- Display the effective Field Of View (FOV) value in the menu.
- Upgrade to NIS 1.0.2.
- Fix crash with Unity applications.
- Fix issue with the sharpening settings not being applied correctly.
- Fix incorrectly computed menu placement and menu eye offset.
- Fix installer bug that prevented upgrading the Companion app without uninstalling first.
- Fix issue where loss of hand tracking would not properly release the virtual controller buttons.
- Fix heavy flickering issue with SteamVR runtime and when using Motion Reprojection with WMR runtime.
- Fix bugs with the Hand-to-Controller configuration tool and international locales.
- Hotfix #1: Fix issue with Oculus runtime and DirectX 11.
- Hotfix #1: Greatly improve image quality of FFR when using a resolution of 1/2x and/or 1/8x.
- Hotfix #1: Fix the sharpening effect when using FSR (was a regression from Beta #2).
- Hotfix #1: Display the Motion Reprojection rate in FPS instead of a fraction of the refresh rate.
- Hotfix #2: Fix a regression introduced in Hotfix #1 and causing the FFR mask to be incorrectly applied.
- Hotfix #2: Allow finer adjustment of the contrast (0.01 step).
- Hotfix #2: Fix position of the FPS overlay.
- Hotfix #2: Disable the OpenXR Toolkit when the application is Edge/Chrome (eg: 360 videos).
- Hotfix #3: Fix menu display with certain applications, such as OpenComposite-ACC.
- Hotfix #4: Fix issue caused by EVGA software.
- Hotfix #4: Introduce Pimax culling workaround ("WFOV Hack") for Flight Simulator 2020.
- Hotfix #4: Add override for each field of view angles independently.
- Hotfix #4: Unlock Motion Reprojection when exiting.

## Changes in Beta #2 (Jan 2022 / v0.9.4 - 0.9.7)

- Add support for AMD FidelityFX Super Resolution (FSR).
- Add support for DX12 (experimental).
- Add configuration via in-headset, on-screen menu.
- Add FPS counter and performance overlay.
- Add World Scale override.
- Add Prediction Dampening (reduce shaking).
- Make in-game hotkeys customizable.
- Integrate new project logo.
- Add an option to disable the layer without having to uninstall it.
- Upgrade to NIS 1.0.1.
- Configure the NIS shader to use optimal compute scheduling based on the GPU brand.
- Add debug message to investigate swapchain issues with certain OpenXR runtimes.
- Moved logs and screenshot into subfolders in `%LocalAppData%\OpenXR-Toolkit` (no filesystem clobbering).
- Try to load hand mappings configuration file from `%LocalAppData%\OpenXR-Toolkit\configs` (which is user writable).
- Fix "index tip tap" hand gesture.
- Allow multiple gestures to be bound to the same controller input.
- Tentatively fix loss of tracking/loss of synchronization with the emulated controller.
- Add controller timeout to simulate shutdown of the controller after loss of hand tracking.
- Fix frequent `XR_ERROR_LIMIT_REACHED` error when using the OpenXR Developer Tools for Windows Mixed Reality.
- Hotfix #1: Fix issue with AMD graphic cards.
- Hotfix #2: Fix issue with Varjo Aero headset.
- Hotfix #3: Fix issue with Vive Pro 2 and Vive Cosmos headsets.
- Hotfix #3: Changed world scale setting to follow what SteamVR does (lower percentage means smaller scaler and vice-versa).
- Hotfix #3: Fixed Ctrl+Alt custom key support.
- Hotfix #3: Improvements to the menu (highlights, display).
- Hotfix #3: Display the current version in the menu and the Companion app.
- Hotfix #3: Disable the use of Prediction Dampening with Varjo heasets (Varjo runtime bug).
- Hotfix #3: Change the menu offset range from +/- 500 to +/- 3000.
- Hotfix #3: Fallback to Arial font when Segoe UI Symbol is not installed.
- Hotfix #3: Installer is now digitally signed.

## Initial OpenXR NIS Scaler (Dec 2021)

- Support for NVIDIA Image Scaling (NIS).
- Support with DX11 only.

## Initial OpenXR Hand-to-Controller (Dec 2021)

- Support Ultraleap hand tracking devices.
- Draw a basic articulated hand skeleton.
- Support with DX11 only.
- Support customization through the configuration tool.
