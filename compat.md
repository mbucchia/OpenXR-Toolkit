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
| War Thunder | Not supported when Easy Anti-Cheat is enabled - Not supported on Varjo headsets |
| Bonelab | Steam edition only - Oculus store edition does not use OpenXR |
| Hubris | - |
| Pavlov VR (Beta) | - |
| EVERSLAUGHT | - |
| Contractors | - |
| Light Brigade | - |
| A Township Tale | - |
| PLAY'A VR Video Player | - |

Do you have a game working but it's not in the list? Please file an [Issue](https://github.com/mbucchia/OpenXR-Toolkit/issues) to let us know!

Additionally, OpenXR Toolkit has been confirmed to work with the Unity OpenXR plugin and Unreal Engine OpenXR plugin, as long as the application uses Direct3D.

### Applications using Oculus XR Plugin

Some users reported that some applications built using Oculus XR Plugin will currently default to using Oculus mode (legacy OVR). For Oculus users, it is possible to force some of these applications to use OpenXR (and therefore OpenXR Toolkit) by replacing the `OVRPlugin.dll` file inside the application folder with the OpenXR-enabled copy of the file from another application (or directly from the Oculus developer package). While this is a neat trick, the developers of OpenXR Toolkit do not recommend this hack, and will not provide support for these applications.

## OpenVR applications

OpenXR Toolkit can also be used with certain OpenVR applications through [OpenComposite](opencomposite).

| Game | Limitations |
| --- | --- |
| American Truck Simulator 2 | - |
| Assetto Corsa | Fixed Foveated Rendering requires to lower the "Glare" setting under "Video settings" to Medium or lower |
| Assetto Corsa Competizione | - |
| Automobilista 2 | Does not work with OpenComposite on Oculus (game forces Oculus mode over OpenVR) |
| DCS World [1] | - |
| Dirt Rally 2 | Requires [dr2vrfix-openxr](https://github.com/mbucchia/dr2vrfix-openxr) when using the "eye accomodation fix" |
| Elite Dangerous | - |
| Euro Truck Simulator 2 | - |
| Fallout 4 VR | - |
| IL-2 Sturmovik | Does not support Fixed Foveated Rendering |
| F1 2022 | - |
| Pavlov VR | - |
| Project Cars 2 | - |
| Project Cars 3 | - |
| rFactor 2 | Fixed Foveated Rendering requires to lower the "Post Effects" setting under "Video setup" to Medium or lower |
| Subnautica | - |
| The Elder Scrolls V: Skyrim VR | - |

[1] DCS World OpenBeta supports OpenXR without OpenComposite, and should be preferred.

Do you have a game working but it's not in the list? Please file an [Issue](https://github.com/mbucchia/OpenXR-Toolkit/issues) to let us know!
