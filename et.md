---
layout: default
title: Eye Tracking
parent: Features
nav_order: 3
---

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Using with Eye Tracking

The use of Eye Tracking (ET) in conjunction with [Foveated Rendering](fr) (FR) can greatly improve performance with minimal loss of visual quality.

The option to enable Eye Tracking will appear in the menu if and only if your headset and headset driver can support it. The following are supported:

- Varjo devices.
- HP G2 Omnicept.
- Pimax devices with Droolon eye tracking module.

In addition to this requirement, it must be possible for OpenXR Toolkit to distinguish when the application is rendering the left and right views. This is not possible for all applications, and therefore might not be offered for all applications. The prime example is any application using OpenComposite cannot do Foveated Rendering with Eye Tracking.

### Considerations for Varjo

You must toggle the _Allow eye tracking_ option from the _System_ tab of the Varjo Base software:

![Allow all](site/varjo-et.png)<br>
*This option must be enabled for OpenXR Toolkit to access the eye tracking data for its own implementation of Foveated Rendering.*

### Considerations for HP G2 Omnicept

In order to use the eye tracker on your Omnicept device, you must first install the [HP Omnicept Runtime](https://developers.hp.com/omnicept/downloads/hp-omnicept-runtime).

Once installed, please run the HP Omnicept eye calibration application.

You must then allow applications to use the eye tracker. You can enable all applications by disabling the _Require user approval for Omnicept clients using sensor data_ option under the _Omnicept Client Approval Settings_ in the HP Omnicept software:

![Allow all](site/omnicept-allow.png)<br>
*Toggling Off the Require user approval will allow all applications to use the eye tracker.*

You may otherwise selectively allow applications, one by one. You must first run the application once, enable eye tracking in the OpenXR Toolkit menu, then open the HP Omnicept software and allow the client, under the _Clients_ tab, in the _Incoming subscriptions request_ table:

![Allow client](site/omnicept-perms.png)<br>
*You can Accept Incoming subscriptions requests to selecting allow an application, while forbidding others.*

### Considerations for Pimax+Droolon

In order to use the Droolon eye tracking module on your Pimax device, you must first install the [aSeeVR SDK](https://drive.google.com/file/d/1ELDtOnMa-MkgchmWFf7w5an-iPOFtQL8/view?usp=sharing&_ga=2.110383681.599346747.1650530138-1983392096.1642581798).

Once installed, please run the aSeeVR runtime. In the system tray bar, right-click on the runtime, and make sure to enable _deviced_ -> _Pimax_.

When using Foveated Rendering in the OpenXR Toolkit, adjust the _Eye projection distance_ from the _Foveated rendering_ menu until you reach comfortable sensitivity.
