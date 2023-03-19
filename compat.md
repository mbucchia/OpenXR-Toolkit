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

See [Supported headsets](index#supported-headsets).

## OpenXR applications

| Game | Limitations |
| --- | --- |
| Microsoft Flight Simulator 2020 | Foveated Rendering causes blur in the main menu |
| iRacing | - |
| DCS World OpenBeta | On Varjo headsets, requires the use of [OpenXR-InstanceExtensionsWrapper](https://github.com/mbucchia/OpenXR-InstanceExtensionsWrapper/releases/tag/0.0.1) |
| War Thunder | Not supported on Varjo headsets |
| Bonelab | Steam edition only - Oculus store edition does not use OpenXR |
| Hubris | - |
| EVERSLAUGHT | - |
| Contractors | - |
| Light Brigade | - |
| A Township Tale | - |
| PLAY'A VR Video Player | - |

Do you have a game working but it's not in the list? Please file an [Issue](https://github.com/mbucchia/OpenXR-Toolkit/issues) to let us know!

Additionally, OpenXR Toolkit has been confirmed to work with the Unity OpenXR plugin and Unreal Engine OpenXR plugin, as long as the application uses Direct3D.

## OpenVR applications

OpenXR Toolkit can also be used with certain OpenVR applications through [OpenComposite](opencomposite).

| Game | Limitations |
| --- | --- |
| American Truck Simulator 2 | - |
| Assetto Corsa | Fixed Foveated Rendering requires to lower the "Glare" setting under "Video settings" to Medium or lower |
| Assetto Corsa Competizione | - |
| Automobilista 2 | - |
| DCS World [1] | - |
| Dirt Rally 2 | Requires [dr2vrfix-openxr](https://github.com/mbucchia/dr2vrfix-openxr) when using the "eye accomodation fix" |
| Elite Dangerous | - |
| Euro Truck Simulator 2 | - |
| IL-2 Sturmovik | Does not support Fixed Foveated Rendering |
| F1 2022 | - |
| Pavlov VR | - |
| Project Cars 2 | - |
| Project Cars 3 | - |
| rFactor 2 | Fixed Foveated Rendering requires to lower the "Post Effects" setting under "Video setup" to Medium or lower |
| Subnautica | - |

[1] DCS World OpenBeta supports OpenXR without OpenComposite, and should be preferred.

Do you have a game working but it's not in the list? Please file an [Issue](https://github.com/mbucchia/OpenXR-Toolkit/issues) to let us know!
