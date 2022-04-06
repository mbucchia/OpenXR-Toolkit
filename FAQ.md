---
layout: default
title: FAQ
nav_order: 6
---

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Q: ELI5: What is OpenXR?

OpenXR is a structured set of instructions and rules for developers to create applications (such as Flight Simulator 2020) that use virtual reality or augmented reality (or XR as the industry calls it) that run on modern devices (such as the HP Reverb or Oculus Quest).

## Q: Why the OpenXR Toolkit?

The goal of the OpenXR Toolkit is to bring innovative features to the community without waiting for platform software to add those features.

## Q: What headset does the OpenXR Toolkit work with?

It should work with any VR headset thanks to OpenXR. We've seen success with Windows Mixed Reality (eg: HP Reverb), any headset going through the SteamVR runtime (Valve Index), Pimax, Oculus Quest...

## Q: What GPUs does the OpenXR Toolkit work with?

Any GPU that is compatible DirectX 11.

Yes, even the NVIDIA Image Scaling (NIS) will work on non-NVIDIA cards, and AMD's FSR will work on non-AMD cards.

Fixed foveated rendering is only supported with the following GPUs:

* NVIDIA GeForce GTX 1600 series and RTX series, both DX11 and DX12.
* AMD RX 6000 series, with DX12 only.

## Q: Does the OpenXR Toolkit work with DX11 and DX12?

It works with both, however DX12 support is considered experimental at this time.

## Q: Will the OpenXR Toolkit work with other games like the ones from Steam?

This software only works with OpenXR applications, not OpenVR applications.

Even with OpenXR applications, we cannot guarantee it will work, as I've only implemented the bare minimum for MSFS.

## Q: Do I need to uninstall the OpenXR Toolkit if a game is not compatible?

<details>
  <summary>Use The Companion App "Disable the OpenXR Toolkit" feature instead...</summary>

  <img alt="Companion app" src="site/companion.png">
  
</details>

## Q: I used your previous NIS Scaler or Hand-To-Controller software.

Please uninstall the previous software. All of the functionality is included in the new OpenXR Toolkit!

You may also notice that the upscaling factor value is now working differently. Read about it [here](upscaling).

## Q: Do I need to run any application?

No, you may just run your game as usual. No need to open the companion app.

## Q: Can the menu be invoked at any time?

Yes.

The "Press [...] to bring up the menu" banner is only displayed as a hint during the first few runs of the OpenXR Toolkit, and the menu can be called even when this text is not displayed.

## Q: Can you tell me what the best settings are?

No, we cannot. The settings depend on your hardware and your expectations.

## Q: I am not getting better performance from upscaling or fixed foveated rendering?

The upscaling and fixed foveated rendering features will help relieve your GPU, which will only yield better performance if your GPU was the limiting component. If your performance is limited by your CPU, upscaling will not help improving performance.

## Q: There’s already a NIS option in my GPU driver... Do I need this?

The NIS option in the NVIDIA drivers does not apply to VR. You need this software to use NIS with VR. It is also recommended to turn off NIS in the NVIDIA driver when using this software, otherwise the GPU will be performing extra processing for the desktop view and not for VR.

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
