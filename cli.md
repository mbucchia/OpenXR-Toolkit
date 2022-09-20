---
layout: default
title: Command line tool
parent: Features
nav_order: 6
---

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Why the command line tool?

The OpenXR Toolkit command line too lets you create various shortcuts to control the OpenXR Toolkit features outside of the OpenXR Toolkit on-screen menu. For example, you may use [Autohotkey](https://lexikos.github.io/v2/docs/Tutorial.htm#s12) to create keyboard shortcuts to toggle features on/off while in-game.

### Syntax

```
Usage: C:\Program Files\OpenXR-Toolkit\companion.exe
    [app <name>]
    [-sunglasses <off|light|dark|trunite>]
    [-post-process <[0,1]>]
    [-contrast <[0,100]>]
    [-brightness <[0,100]>]
    [-exposure <[0,100]>]
    [-saturation <[0,100]>]
    [-vibrance <[0,100]>]
    [-highlights <[0,100]>]
    [-shadows <[0,100]>]
    [-gain-r <[0,100]>]
    [-gain-g <[0,100]>]
    [-gain-b <[0,100]>]
    [-world-scale <[0,1000]>]
    [-zoom <[1,150]>]
    [-reprojection-rate <unlocked|1/2|1/3|1/4>]
    [-foveated-rendering <toggle>]
    [-overlay <toggle>]

When no app is specified, the currently running app is used.
Use syntax <+N> to add value N to integral and decimal values (N can be negative)
Use syntax <+N> to cycle by step N through enumeration values (with automatic wraparound)

Examples:
 companion.exe -brightness 50.5 -contrast 45.8
   Set brightness and contrast values for the currently running app
 companion.exe -sunglasses +1
   Cycle through sunglasses mode for the currently running app
 companion.exe -world-scale +-10
   Decrease world scale by 10% for the currently running app
 companion.exe -overlay toggle
   Toggle the overlay on/off for the currently running app
 companion.exe app FS2020 dump
   Dump settings for app 'FS2020' (Flight Simulator 2020)
```

The `dump` option can be used to generate a command-line based on the current option values (eg: as you set from the menu in-game), so that you can copy/paste the command directly into a script:

```
> companion.exe dump
C:\Program Files\OpenXR-Toolkit\companion.exe app HelloXR -sunglasses light -post-process 1 -contrast 50 -brightness 0.1 -exposure 50 -saturation 50 -vibrance 0 -highlights 100 -shadows 0 -gain-r 50 -gain-g 50 -gain-b 50 -world-scale 101 -reprojection-rate 1/4
```
