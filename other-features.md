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

The world scale override can be used to change the relative position of the eye cameras (also known as the Inter-Camera Distance, or ICD). Reducing the distance between the eye cameras (ie: a scale under 100%) will make the world appear bigger, while increasing the distance (ie: a scale above 100%) will make the world appear smaller.

## Prediction dampening

The prediction dampening allows to reduce the jitter that can be observed when head or hand tracking is predicted "too long in advance". A negative value will reduce the amount of prediction, for example -50% will cut in half the requested amount of prediction.

## Screen capture

**Note**: Screenshots currently only work with DX11 applications.

In order to activate this feature, you must check the _Enable screenshot_ box in the _OpenXR Toolkit Companion app_. You may then press Ctrl+F12 to take a screenshot of the left-eye view that is rendered in the headset. Screenshots are saved under `%LocalAppData%\OpenXR-Toolkit\screenshots`. This folder may be opened from the _OpenXR Toolkit Companion app_ by clicking the _Open screenshots folder_ button.
