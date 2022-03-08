---
layout: default
title: Changelog
nav_order: 6
---

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Changes in Beta #3 (Mar 2022 / v1.0.1 - )

- Add support for Fixed Foveated Rendering (FFR) (experimental, with selected GPUs only).
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
