---
layout: default
title: FAQ
nav_order: 7
---

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Q: ELI5: What is OpenXR?

OpenXR is standard for developers to create applications (such as Flight Simulator 2020) that use virtual reality or augmented reality (or XR as the industry calls it) that run on modern devices (such as the HP Reverb or Oculus Quest).

## Q: Why the OpenXR Toolkit?

The goal of the OpenXR Toolkit is to bring innovative features to the community without waiting for platform software to add those features.

## Q: What headset does the OpenXR Toolkit work with?

It should work with any VR headset thanks to OpenXR. The following headsets have been confirmed to work: Windows Mixed Reality (eg: HP Reverb), Oculus Quest & Quest 2, Pimax 5K & 8KX, Varjo Aero, Valve Index, HTC Vive, Pico Neo 3 & 4.

## Q: What GPUs does the OpenXR Toolkit work with?

Any GPU that is compatible DirectX 11.

Yes, even the NVIDIA Image Scaling (NIS) will work on non-NVIDIA cards, and AMD's FSR will work on non-AMD cards.

Foveated rendering is only supported with the following GPUs:

* NVIDIA GeForce GTX 1600 series and RTX series, both DX11 and DX12 applications.
* AMD RX 6000 series, with DX12 applications only.

## Q: Does the OpenXR Toolkit work with DX11 and DX12?

It works with both, however DX12 support is considered experimental at this time.

## Q: Will the OpenXR Toolkit work with other games like the ones from Steam?

This software only works with OpenXR applications, not OpenVR applications.

Some OpenVR applications are supported through the use of [OpenComposite](opencomposite).

Whether it is OpenXR applications or OpenVR applications, we cannot guarantee it will work.

## Q: Do I need to uninstall the OpenXR Toolkit if a game is not compatible?

See [Troubleshooting](troubleshooting#disabling-the-openxr-toolkit-with-incompatible-applications).

## Q: Do I need to uninstall the previous version?

No, the new version will overwrite the old one.

## Q: Do I need to run any application?

No, you may just run your game as usual. No need to open the companion app.

## Q: Can the menu be invoked at any time?

Yes.

## Q: Can you tell me what the best settings are?

No, we cannot. The settings depend on your hardware and your expectations.

## Q: I am not getting better performance from upscaling or fixed foveated rendering?

The upscaling and fixed foveated rendering features will help relieve your GPU, which will only yield better performance if your GPU was the limiting component. If your performance is limited by your CPU, upscaling will not help improving performance.

See [Troubleshooting](troubleshooting#checking-if-you-are-cpu-or-gpu-limited).

## Q: There’s already a NIS/FSR option in my GPU driver... Do I need this?

The NIS/FSR options in the NVIDIA/AMD drivers does not apply to VR. You need OpenXR Toolkit to use NIS/FSR with VR applications. It is also recommended to turn off NIS/FSR in the NVIDIA/AMD driver when using this software, otherwise the GPU will be performing extra processing for the desktop view and not for VR.

## Q: How can I compare the image quality?

You can use the companion app to enable the screenshot mode. See [Other Features](other-features). You may then use a tool such as the [Image Comparison Analysis Tool (ICAT)](https://www.nvidia.com/en-us/geforce/technologies/icat/) from NVIDIA to compare images.

## Q: Is this affiliated with Microsoft?

While Matthieu is a Microsoft employee working on OpenXR, please note that this project is not affiliated with Microsoft.

## Q: How can I support this project?

You can support this project in two ways:

- Report issues and/or success (especially if you have exotic configurations), be active on the [Discord server](https://discord.gg/WXFshwMnke) to share you experience and help others.

- Tell others about the OpenXR Toolkit, post about it in other communities where it might be useful, and redirect them to this site!

## Q: Is it open source? Can I contribute?

It is 100% open source, and if you would like to contribute we are happy if you get in touch with me!
