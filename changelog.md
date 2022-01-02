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

## Changes in Beta #2

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

## Initial OpenXR NIS Scaler

- Support for NVIDIA Image Scaling (NIS).
- Support with DX11 only.

## Initial OpenXR Hand-to-Controller

- Support Ultraleap hand tracking devices.
- Draw a basic articulated hand skeleton.
- Support with DX11 only.
- Support customization through the configuration tool.
