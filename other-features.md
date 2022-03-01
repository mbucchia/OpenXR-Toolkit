---
layout: default
title: Other
parent: Features
nav_order: 4
---

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## World scale override

The world scale override can be used to change the relative position of the eye cameras (also known as the Inter-Camera Distance, or ICD). This can affect your perception of the size of the world in VR. Reducing the scale (ie: a scale under 100%) will make the world appear smaller, while increasing the scale (ie: a scale above 100%) will make the world appear bigger.

## Prediction dampening

The prediction dampening allows to reduce the jitter that can be observed when head or hand tracking is predicted "too long in advance". This often manifests as "over-sensitivity" with your head movements. For example, some people have reported the view in the headset shaking with their heartbeats. That is an example of over-sensitivity. Set a negative value to reduce the amount of prediction and reduce shaking, for example -50% will cut in half the requested amount of prediction. Experimentally, best results have been achieved with values between -20% and -40%.

**Note:** This setting is currently not available to Varjo users due to a bug in the Varjo software.

## Screen capture

In order to activate this feature, you must check the _Enable screenshot_ box in the _OpenXR Toolkit Companion app_, and select the image format you'll save the screenshots with. The OpenXR ToolKit supports the following formats: [DDS](https://en.wikipedia.org/wiki/DirectDraw_Surface)*, [PNG](https://en.wikipedia.org/wiki/Portable_Network_Graphics), [JPG](https://en.wikipedia.org/wiki/JPEG) and [BMP](https://en.wikipedia.org/wiki/BMP_file_format).

You may then press Ctrl+F12 to take a screenshot of the left-eye view that is rendered in the headset. Screenshots are saved under `%LocalAppData%\OpenXR-Toolkit\screenshots`. This folder may be opened from the _OpenXR Toolkit Companion app_ by clicking the _Open screenshots folder_ button.

**The DDS format is a lossless format native to DirectX but some tools might have issues opening DDS files. The tools that were confirmed to properly open them with the OpenXR Toolkit are [GIMP](https://www.gimp.org/) and [Paint.net](https://github.com/paintdotnet/release/releases)*
