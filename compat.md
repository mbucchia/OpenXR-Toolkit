---
layout: default
title: Compatibility
nav_order: 3
---

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Keep your software up-to-date!

For the best results and to maximize chances that your application will work, always make sure to:

- Use the latest version of OpenXR Toolkit. Compatibility with certain game is added over time.
- Use the latest version of your OpenXR runtime (typically distributed through your VR software, eg: Oculus software, Varjo base).
- If using OpenComposite (see further below), use the latest version of OpenComposite.

## Headset compatibility

| Headset brand | Supported? |
| --- | --- |
| Windows Mixed Reality (HP Reverb, Samsung Odyssey...) | Yes |
| Oculus (Rift, Quest, Quest 2, Quest Pro...) | Yes |
| Oculus (via Virtual Desktop)) | Yes |
| Varjo (Aero, VR-3...) | Yes |
| Pimax (5K, 8K...) | Yes |
| HTC Tier 1 (Vive original, Vive Pro) | Yes |
| HTC Tier 2 (Vive Cosmos, Vive Focus) | Yes |
| Valve Index | Yes |
| Pico (Neo 3, Neo 4) | Yes |

## OpenXR applications

| Game | Limitations |
| --- | --- |
| Microsoft Flight Simulator 2020 | - |
| iRacing | - |
| War Thunder | Not supported on Varjo headsets |
| Bonelab | Not supported on Oculus headsets |

Do you have a game working but it's not in the list? Please file an [Issue](https://github.com/mbucchia/OpenXR-Toolkit/issues) to let us know!

Additionally, OpenXR Toolkit has been confirmed to work with the Unity OpenXR plugin and Unreal Engine OpenXR plugin, as long as the application uses Direct3D.

## OpenVR applications

OpenXR Toolkit can also be used with certain OpenVR applications through [OpenComposite](opencomposite).

| Game | Limitations |
| --- | --- | --- |
| American Truck Simulator 2 | - |
| Assetto Corsa | - |
| Assetto Corsa Competizione | - |
| Automobilista 2 | - |
| Digital Combat Simulator | - |
| Dirt Rally 2 | Requires [dr2vrfix-openxr](https://github.com/mbucchia/dr2vrfix-openxr) when using "eye accomodation fix" |
| Elite Dangerous | - |
| Euro Truck Simulator 2 | - |
| IL-2 Sturmovik | Does not support Fixed Foveated Rendering |
| F1 2022 | - |
| Pavlov VR | - |
| Project Cars 2 | - |
| Project Cars 3 | - |
| rFactor 2 | - |
| Subnautica | - |

Do you have a game working but it's not in the list? Please file an [Issue](https://github.com/mbucchia/OpenXR-Toolkit/issues) to let us know!
