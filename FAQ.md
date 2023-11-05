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

It works with any VR headset thanks to OpenXR. See [index#supported-headsets](supported headsets).

## Q: What GPUs does the OpenXR Toolkit work with?

Any GPU that is compatible DirectX 11.

Yes, even the NVIDIA Image Scaling (NIS) will work on non-NVIDIA cards, and AMD's FSR will work on non-AMD cards.

Foveated rendering requires additional support, see See [index#supported-graphics-cards](supported graphics cards).

## Q: Does the OpenXR Toolkit work with OpenGL or Vulkan.

No.

## Q: Will the OpenXR Toolkit work with other games like the ones from Steam?

This software only works with OpenXR applications, not OpenVR not OVR applications. See [index#supported-applications](supported applications).

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

You can use my [GitHub sponsor page](https://github.com/sponsors/mbucchia).

## Q: Is it open source? Can I contribute?

It is 100% open source, and if you would like to contribute we are happy if you get in touch with me!
