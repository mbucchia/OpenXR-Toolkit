---
layout: default
title: Quickstart
nav_order: 1
---

# OpenXR Toolkit

This software provides a collection of useful features to customize and improve existing OpenXR applications, including render upscaling and sharpening, foveated rendering, image post-processing and other game-enhancing tweaks.

DISCLAIMER: This software is distributed as-is, without any warranties or conditions of any kind. Use at your own risks.

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

# Setup

**READ ALL THE INSTRUCTIONS BELOW - IF YOU DO NOT READ THE INSTRUCTIONS, YOU WILL NOT BE SUCCESSFUL.**<br>
**IF YOU DO NOT WISH TO READ THE INSTRUCTIONS BELOW, PLEASE DO NOT ATTEMPT TO INSTALL OR USE OPENXR TOOLKIT.**

## Downloads

Current version: **1.3.2**

[Download the latest](https://github.com/mbucchia/OpenXR-Toolkit/releases/download/1.3.2/OpenXR-Toolkit-1.3.2.msi){: .btn .btn-blue }

...or expore [all versions](https://github.com/mbucchia/OpenXR-Toolkit/releases).

## Requirements

### Supported headsets

OpenXR Toolkit is compatible with any headset with OpenXR capabilities on PC, whether through the headset's specific OpenXR support (sometimes referred to as "native OpenXR runtime") or through SteamVR OpenXR support. Please refer to the table below for possible combinations of headsets and runtimes.

| Headset brand | Supports OpenXR Toolkit? | OpenXR runtimes available |
| --- | --- | --- |
| Windows Mixed Reality (HP Reverb, Samsung Odyssey...) | Yes | OpenXR for Windows Mixed Reality, SteamVR OpenXR |
| Oculus (Rift, Quest, Quest 2, Quest 3 Quest Pro...) via Oculus Link | Yes | Oculus OpenXR, SteamVR OpenXR |
| Oculus (Rift, Quest, Quest 2, Quest 3 Quest Pro...) via Virtual Desktop | Yes | VirtualDesktopXR (VDXR), SteamVR OpenXR |
| Varjo (Aero, VR-3...) | Yes | Varjo OpenXR, SteamVR OpenXR |
| Pimax (5K, 8K...) | Yes | [PimaxXR](https://github.com/mbucchia/Pimax-OpenXR/wiki), SteamVR OpenXR |
| HTC Tier 1 (Vive original, Vive Pro) | Yes | SteamVR OpenXR |
| HTC Tier 2 (Vive Cosmos, Vive Focus) | Yes | Vive OpenXR, SteamVR OpenXR |
| Valve Index | Yes | SteamVR OpenXR |
| Pico (Neo 3, Neo 4) via Steaming Assistant | Yes | SteamVR OpenXR |
| Pico (Neo 3, Neo 4) via Virtual Desktop | Yes | VirtualDesktopXR (VDXR), SteamVR OpenXR |

#### Eye tracking feature

The eye tracking feature of OpenXR Toolkit is supported on a majority of headsets with eye tracking capabilities. However, not all OpenXR runtimes for the headset may provide eye tracking support. Please see the table below for possible combinations, and whether a separate add-on is needed for eye tracking.

| Headset brand | OpenXR runtime | Supports eye tracking in OpenXR Toolkit? |
| --- | --- | --- |
| HP Reverb G2 Omnicept | OpenXR for Windows Mixed Reality | Yes |
| HP Reverb G2 Omnicept | SteamVR OpenXR | Yes [1] |
| Oculus Quest Pro via Oculus Link | Oculus OpenXR | Yes |
| Oculus Quest Pro via Oculus Link | SteamVR OpenXR | No |
| Oculus Quest Pro via Virtual Desktop | VirtualDesktopXR (VDXR) | Yes |
| Oculus Quest Pro via Virtual Desktop | SteamVR OpenXR | Yes [1] |
| Varjo (all models) | Varjo OpenXR | Yes |
| Varjo (all models) | SteamVR OpenXR | Yes [1] |
| Pimax Crystal | PimaxXR | Yes |
| Pimax Crystal | SteamVR OpenXR | Yes [1] |
| Pimax (5K, 8K) with Droolon Pi1 | PimaxXR | Yes |
| Pimax (5K, 8K) with Droolon Pi1 | SteamVR OpenXR | No |
| HTC Vive Pro Eye, Vive Focus 3 | All | Yes [2] |
| Pico Neo 4 Pro | All | No [3] |

[1] Support for eye tracking through SteamVR OpenXR is provided through the [OpenXR-Eye-Trackers](https://github.com/mbucchia/OpenXR-Eye-Trackers/wiki) add-on.

[2] Support for eye tracking on Vive headsets is provided through the [Vive Console for SteamVR](https://store.steampowered.com/app/1635730/VIVE_Console_for_SteamVR/).

[3] **Pico devices do not support eye tracking for PC applications. Pico does not provide the necessary tools to implement it. The marketing of Pico devices is misleading for the consumers**, and hurts the developers community: a) Pico marketing claims support for eye tracking without mentioning that this feature is only supported for standalone mode (Android apps) and not available to developers on PC; b) This practice makes us developers look bad for not supporting eye tracking, in spite of the shortcoming being on Pico's side

### Supported graphics cards

OpenXR Toolkit is compatible with any graphics card supporting DirectX 11, regardless of brand.

Certain features, like Foveated Rendering, have additional requirements, per the table below. OpenXR Toolkit does not support Vulkan applications, regardless of graphics cards.

| Graphics card | Supports Foveated Rendering? | Limitations |
| --- | --- | --- |
| Nvidia RTX 4000 series | Yes | Application must use Direct3D 11 or Direct3D 12 |
| Nvidia RTX 3000 series | Yes | Application must use Direct3D 11 or Direct3D 12 |
| Nvidia RTX 2000 series | Yes | Application must use Direct3D 11 or Direct3D 12 |
| Nvidia GTX 1600 series | Yes | Application must use Direct3D 11 or Direct3D 12 |
| Nvidia GTX 1000 series (and below) | No | - |
| AMD RX 7000 series | Yes | Application must use Direct3D 12 [1] |
| AMD RX 6000 series | Yes | Application must use Direct3D 12 [1] |
| AMD RX 5000 series (and below) | No | - |
| Intel Arc | Yes | Application must use Direct3D 12 [1] |
| Intel Gen11 (Ice Lake and above) | Yes (but untested) | Application must use Direct3D 12 [1] |
| Intel (any other model) | No | - |

[1] AMD and Intel do not support foveated rendering with Direct3D applications. This is a limitation of the AMD and Intel drivers and not a limitation of OpenXR Toolkit.

### Supported applications

**OpenXR Toolkit only works with OpenXR applications**. Not all applications are built for OpenXR. Below is a table of applications known to use OpenXR, and whether they are known to work with OpenXR Toolkit.

| Game | Works with OpenXR Toolkit? | Limitations |
| --- | --- | --- |
| A Township Tale | - |
| Beat Saber | Yes | - |
| BeamNG.drive | No | - |
| Bonelab | Yes | Steam edition only - the Oculus store edition does not use OpenXR |
| Contractors | Yes | Must start the game in OpenXR mode (`-hmd=openxr` parameter) |
| DCS World | Yes | Must start the game in OpenXR mode (`--force_OpenXR` parameter)<br>On Varjo headsets, requires the use of [OpenXR-InstanceExtensionsWrapper](https://github.com/mbucchia/OpenXR-InstanceExtensionsWrapper/releases/tag/0.0.1) |
| EVERSLAUGHT | Yes | - |
| Ghosts of Tabor | Yes | Oculus store edition only - the Steam edition does not use OpenXR |
| Hubris | Yes | - |
| iRacing | Yes | Must start the game in OpenXR mode |
| Light Brigade | Yes | - |
| Microsoft Flight Simulator 2020 | Yes | - |
| Minecraft for Windows | No | - |
| Pavlov VR | Yes | - |
| Phasmophobia | Yes | - |
| Pistol Whip | Yes | - |
| PLAY'A VR Video Player | Yes | - |
| Praydog's UEVR injector | Yes | Must use the injector in OpenXR mode |
| The 7th Guest VR | Yes | - |
| War Thunder | Yes | - |
| X-Plane 12 | No | Must start the game in OpenXR mode (`--open_xr` parameter) |

#### OpenComposite

**OPENCOMPOSITE IS NOT OFFICIALLY SUPPORTED - USE IT AT YOUR OWN EXPENSE.**<br>
**REQUEST FOR TROUBLESHOOTING/SUPPORT FOR OPENCOMPOSITE WILL BE IGNORED.**

## Installation

### 1. Run the `OpenXR-Toolkit.msi` program.

![Installer file](site/installer-file.png)

> 📝 **Note:** You may be warned that Windows protected your PC because this software is not trusted. The application is built on a GitHub server hosted in the Microsoft cloud, which greatly limits the risk of contamination from viruses and malware. Additionally, we have digitally signed the software through a reputable organization (Comodo) which helps with guaranteeing that is has not been altered by any third party.

<details>
  <summary>Proceed through the "Windows protected your PC" warning...</summary>

  <p>Select <i>More info</i> then <i>Run anyway</i>.</p>

  <img alt="Warning not signed" src="site/unsigned1.png">
  <img alt="Warning not signed" src="site/unsigned2.png">

</details>

### 2. Follow the instructions to complete the installation procedure.

![Setup wizard](site/installer.png)

> 📝 **Note:** You do not need to uninstall the previous version of OpenXR Toolkit if you had one installed. The new version will overwrite the previous one.

### 3. Launch the _OpenXR Toolkit Companion app_ to confirm that the software is active.

<details>
  <summary>You can use the shorcut found on the desktop or in the Start menu...</summary>

  <img alt="Companion app shortcut" src="site/companion-start.png">
  
</details>

The _OpenXR Toolkit Companion app_ may be used sporadically to enable or disable advanced features or perform recovery (see further below). The application displays a green or red status indicating whether the software OpenXR component is active.

The _OpenXR Toolkit Companion app_ can also be used to customize the keyboard shortcuts used to invoke and navigate the menu.

![Companion app](site/hotkeys.png)

> 💡 **Tip:** You don't need to keep the companion app running in order to use the software: the actual settings for the toolkit are available from within your OpenXR application and display directly in the headset! (see below). 

# Basic usage

Once installed, please run the desired OpenXR application. A welcome message will appear and instruct you to open the menu:

![On-screen menu](site/welcome.png)

> 💡 **Troubleshooting:** Can't see the menu? Head to [Troubleshooting](troubleshooting#menu-is-not-showing) for help.

In order to navigate the menu, select options and change values:

![On-screen menu](site/menu.png)

- Use `CTRL+F2` to move to the next option (next line) in the menu.
- Use `CTRL+F1` to move selection to the left / decrease the option value.
- Use `CTRL+F3` to move selection to the right / increase the option value.
- Hold both `SHIFT` and `CTRL` together to change values fasters.

> 💡 **Tip:** Use the _OpenXR Toolkit Companion app_ to change the default shortcut keys.

## Available options

See [Features](features) for more details.

# Recovery

See [Troubleshooting](troubleshooting) for more details.

If changing some settings render the application unusable, use Ctrl+F1+F2+F3 to hard reset all settings.

**Note: if the key combinations were changed from the _OpenXR Toolkit Companion app_, please use the newly assigned keys**.

 If an application can no longer start, use the _OpenXR Toolkit Companion app_ (found on the desktop or Start menu) and select the Safe mode before starting the application, then use Ctrl+F1+F2+F3 (regardless of custom key combinations) to hard reset all settings.

# Removal

The software can be removed from Windows' _Add or remove programs_ menu.

![Add or remove programs](site/add-or-remove.png)

In the list of applications, select _OpenXR-Toolkit_, then click _Uninstall_.

![Uninstall](site/uninstall.png)
