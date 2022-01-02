---
layout: default
title: Upscaling
parent: Features
nav_order: 1
---

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## What is upscaling?

Upscaling allows an application to render at a resolution lower than the resolution of the headset. The resulting images are then "stretched" to fit the resolution of the headset. The simplest upscaling technique merely interpolate the missing pixels based on the adjacent ones, usually creating "blurriness".

The upscaling techniques included in the OpenXR Toolkit strive to produce better results through the use of additional filtering and sharpening techniques, while keeping a very low cost.

## Configuring upscaling

In the paragraphs below, we use the term "target display resolution" to refer to the per-eye resolution exposed by the OpenXR runtime. This resolution may be the native resolution of the headset, or it may be tweaked through the use of a "custom render scale".

There are three parameters to control upscaling:

- The type of upscaling, to chose between NIS or FSR (see below). Each type of upscaling might work better for certain applications, or their effectiveness might depend on your personal preferences. **Changing this setting requires the VR session to be restarted**.

- The upscaling factor, which represents the ratio between the target display resolution and the application rendering resolution. **Changing this setting requires the VR session to be restarted**.

- The sharpness setting, which indicates how much sharpening the algorithm performs. This setting is adjustable in real-time, without the need to restart the VR session. **The effect of the sharpening setting greatly varies between NIS and FSR**. 

There are two ways to specify the upscaling factor. The two methods are exactly equivalent, and you may choose which one to use at your preference:

- A value above 100% represents the ratio "target display resolution _over_ application rendering resolution". The higher the value, the lower the application rendering resolution. For example, a value of 200% means that the application rendering resolution will be calculated to produce a target display resolution twice higher. If the target display resolution is 2200x2200 pixels, then the application will render at a resolution of 1100x1100 pixels.

- A value below 100% represents the ratio "application rendering resolution _over_ target display resolution". This is how the upscaling factor was specified in the previous version of the NIS Scaler. Low lower the value, the lower the application rendering resolution. For example, a value of 50% means that the application rendering resolution will be half of the target display resolution. If the target display resolution is 2200x2200 pixels, then the application will render at a resolution of 1100x1100 pixels.

Note that even with an upscaling factor set to 100% (no upscaling), some benefit can be seen from adjusting the sharpening.

## NIS: NVIDIA Image Scaling

This is the spatial upscaler and sharpening algorithm developed by NVIDIA and that works on any DirectX 11 GPU (including non-NVIDIA brands). This is the same algorithm that can be enabled for non-VR applications through the NVIDIA Control Panel.

## FSR: FidelityFX Super Resolution

This is the spatial upscaler and sharpening algorithm developed by AMD and that works on any DirectX 11 GPU (including non-AMD brands). More details on [AMD's website](https://www.amd.com/en/technologies/radeon-software-fidelityfx-super-resolution).
