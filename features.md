---
layout: default
title: Features
nav_order: 2
has_children: true
---

Here is a short description of all the options available through the OpenXR Toolkit in-game menu.

You can access more detailed pages through the menu bar on the left.

**Performance** tab:
- **Overlay**: Enables the FPS display or advanced timings display in the top-right corner of the view. _Please note that the overlay may reduce performance_. A fourth option - "_Developer_" - is available in experimental mode and may be used for troubleshooting with the developers. See [Overlay](overlay) for more details.
- **Upscaling**: Enables the use of an upscaler such as NIS or FSR to perform rendering at a lower resolution, and upscale and/or sharpen the image. Requires to restart the VR session. See [Upscaling](upscaling) for more details.
  - **Anamorphic**: When _Disabled_, the _Size_ scales both the width and the height propotionally. When _Enabled_, both sizes can be adjusted independently.
  - **Size**: The upscaling factor (ie: the percentage of magnification of the rendering resolution). The resolution displayed next to the percentage is the effective resolution that the application sees. Requires to restart the VR session.
  - **Width/Height**: This displays the actual in-game render width and height, that is the actual number of pixels the game is rendering per eye.
  - **Sharpness**: The sharpness factor. Has a different scale/effect between NIS and FSR.
  - **Mip-map bias** (_Expert_ setting): This settings changes how the game is rendering some of the textures in order to reveal a little bit more details when used with FSR/NIS upscalers.
- **(Fixed) Foveated rendering** (on supported GPUs only): These settings adjust the [VRS](glossary#vrs) parameters in order to balance out peripheral visual details with rendering performance. See [Foveated Rendering](fr) for more details.
  - **Eye tracking** (on supported headsets only): Enable the use of eye tracking to control the position of the center of the foveated region. See [Eye Tracking](et) for more details.
  - **Eye projection distance** (only with Pimax headsets): Calibrate the sensitivy of eye gaze movements.

  [_Preset_ _mode_ ](fr#preset-mode)
  - **Mode**: Whether to prefer performance over quality.
  - **Pattern**: The size of the foveated regions.
  
  [_Custom_ _mode_](fr#custom-mode)
  - **Inner resolution** (_Expert_ _setting_): The resolution inside the inner ring of foveation. Should be left at full resolution (1x).
  - **Inner ring size**: The size of the inner ring of foveation, in percent of the height of the image.
  - **Middle resolution**: The resolution inside the middle ring of foveation.
  - **Outer ring size**: The size of the outer ring of foveation, in percent of the height of the image.
  - **Outer resolution**: The resolution inside the outer ring of foveation.
  - **Horizontal scale** (_Expert_ _setting_): The rings for foveation can be configured as ellipses. This setting controls the scale of the horizontal radius (or semi-major axis) based on the vertical radius (or semi-minor axis). A value of 100% means that the rings are circles. A value larger than 100% will result in flattened, oval-shaped rings.
  - **Horizontal offset** (_Expert_ _setting_): Add a horizontal offset to the center of the foveation rings. The offset is expressed relative to the left eye, and its opposite value will be applied to the right eye.
  - **Vertical offset** (_Expert_ _setting_): Add a vertical offset to the center of the foveation rings.
  - **Left/Right bias** (_Expert_ _setting_): Lower the resolution of all the regions at once, either for the left or the right eye only at a time.
  - **Scale filter** (_Expert_ _setting_): Tune the minimum size for a render pass to be considered for foveated rendering. The higher this value, the less chances there are that visual glitches occur, however the performance may be lower.
- **Frame rate throttling**: Throttle down the frame rate of the application. WARNING: This can introduce unwanted latency. This option will not appear on Windows Mixed Reality if the _Motion Reprojection_ is forced to _On_ in the _System tab_, and you must use the _Lock motion reprojection instead_.
- **Record statistics**: Enable recording basic frame statistics to a comma-separated values (CSV) file stored under `%LocalAppData%\OpenXR-Toolkit\stats`.

**Appearance** tab:
- **Post-processing**: Allows adjusting the image displaying in the headset. When _Enabled_, the following additional controls are available.

  **Sun Glasses Presets**
  
  These are finely tuned quick-access presets, applying on top of the individual post-processing settings, effectively augmenting but not replacing them:
  - **Light** and **Dark**: Two levels of sun glasses, adjusted to reduce exposure and low light details while preserving perceived contrast.
  - **TruNite**: Wear these glasses exclusively when flying at night and feel the lights popping up in a sea of darkness surrounding you.

  **Individual Controls**
  
  These are adjusting individual enhancements settings and they allow fine tuning the image displaying in the headset:
  - **Contrast**: Adjusts the difference between bright and dark pixels _(Neutral: 50)_.
  - **Brightness**: Adjusts those pixels that are not already extremely bright (preserves the highlights) and adjusts midtones, aka gamma _(Neutral: 50)_.
  - **Exposure**: Adjusts the brightness or darkness of the entire image _(Neutral: 50)_.
  - **Saturation**: Adjusts the colorfullness of the entire image. It affects all colors and pixels in the image equally, regardless of how saturated they already are _(Neutral: 50)_.
  - **Vibrance**: Adjusts the intensity of the more muted colors while leaving the saturated colors untouched _(Neutral: 0)_.
  - **Highlights**: Adjusts the highlight details in reducing the intensity of the brightest pixels _(Neutral: 100)_.
  - **Shadows**: Adjusts the details that appear in shadows in brightening the darkest pixels _(Neutral: 0)_.
  
- **World scale**: The Inter-Camera Distance override, which can be used to alter the world scale.

**Inputs** tab:
- **Shaking reduction**, formerly **Prediction dampening** (only when supported by the system): The prediction override, which can be use to dampen the prediction for head, controllers, and hand movements.
- **Controller emulation** (only when hand tracking is supported by the system): Enable the use of hand tracking in place of the VR controller. Requires a compatible device, such a Leap Motion controller or an Oculus Quest 2 headset. Either or both hands can be enabled at a time. Requires to restart the VR session when toggling on or off. See [Hand tracking](hand-tracking) for more details.
  - **Hands skeleton**: Whether the hands are displayed and what color tone to use.
  - **Hand occlusion** (on supported apps only): Whether the hands can be occluded by geometry from the application.
  - **Controller timeout**: The amount of time after losing track of the hands before simulator shutdown of the simulated VR controller.

**System** tab:
- **Override resolution**: Enable overriding the OpenXR target resolution (same as what the "custom render scale" in OpenXR Tools for WMR does).
  - **Display resolution (per-eye)**: The resolution to use for each eye.
- **Motion reprojection** (only with Windows Mixed Reality): Enable overriding the Motion Reprojection mode. _Default_ means to use the system settings (from the _OpenXR Tools for Windows Mixed Reality_).
  - **Lock motion reprojection** (only with Windows Mixed Reality, when _Motion Reprojection_ is forced to _On_): Disable automatic motion reprojection adjustment, and lock the frame rate to the desired fraction of the refresh rate.
- **Color Gains**: Adjusts the Red, Green and Blue channels gains individually _(Neutral: 50)_.
- **Field of view**: Adjust the pixel density per degree. A smaller field of view is covering a smaller region of the view but with the same amount of pixels, effectively increasing the perceived resolution.
  - **Adjustement** (in _Simple_ mode): Override all 4 angles (up/down/left/right) equally.
  - **Up** (in _Advanced_ mode): Override the "up" angle for both eyes.
  - **Down** (in _Advanced_ mode): Override the "down" angle for both eyes.
  - **Left/Left** (in _Advanced_ mode): Override the "left" angle for the left eye.
  - **Left/Right** (in _Advanced_ mode): Override the "right" angle for the left eye.
  - **Right/Left** (in _Advanced_ mode): Override the "left" angle for the right eye.
  - **Right/Right** (in _Advanced_ mode): Override the "right" angle for the right eye.
- **Blind eye** (on supported applications only): Disable rendering the left or right view. This might require restarting the VR session.
- **Disable mask (HAM)** (_Expert_ _setting_, on supported applications only): Disable the hidden area mesh (HAM). This might require restarting the VR session.

**Menu** tab:
- **Show expert settings**: Show all settings. *This can be pretty overwhelming for certain features*.
- **Font size**: The size of the text for the menu.
- **Menu timeout**: The duration after which the menu automatically disappears when there is no input.
- **Menu distance**: Adjust the viewing distance of the menu. This option can help with comfort when looking at the menu and reading text.
- **Menu opacity**: Adjust the opacity of the menu.
- **Menu eye offset** (in _legacy_ mode): Adjust rendering of the menu until the text appears clear.
- **Show clock in overlay**: Whether to show the clock in the FPS overlay.
- **Overlay horizontal offset** (_Expert_ _setting_): Adjust the horizontal position of the FPS overlay.
- **Overlay vertical offset** (_Expert_ _setting_): Adjust the vertical position of the FPS overlay.
- **Use legacy meny**: Restore the menu to how it worked before OpenXR Toolkit 1.1.2. This can be useful if you are having issues with the menu (eg: not displaying) or performance (eg: the menu causes loss of FPS).
