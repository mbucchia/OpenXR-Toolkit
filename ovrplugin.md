# Meta's OVRPlugin

Meta releases a [plugin for Unity and Unreal Engine](https://developers.meta.com/horizon/documentation/unity/unity-xr-plugin/#oculus-xr-plugin), internally called OVRPlugin (sometimes called Oculus XR Plugin in their public documentation). This plugin claims to implement OpenXR, [an open standard that promotes cross-vendor and cross-plaform](https://www.khronos.org/openxr/). (Contrary to what the name implies, OVRPlugin is not just targeted at the legacy OVR API, but is also Meta's middleware for OpenXR).

However, the OVRPlugin takes intentional precautions to exclude non-Meta platforms. This means that content developed with OVRPlugin will only work with Quest Link, and it will not work with any other runtime.
- This includes blocking applications from running with Virtual Desktop, SteamLink or ALVR, even on a Meta Quest headset.
- This includes blocking applications from running on non-Meta headsets such as Pimax, Pico, Varjo, Vive, etc.

The way OVRPlugin blocks non-Meta runtimes is by enforcing several conditions that will only be true when using Quest Link. OVRPlugin will fail to load (and preclude your application from using OpenXR) when:
- The name of the OpenXR runtime (as returned by [`xrGetInstanceProperties()`](https://registry.khronos.org/OpenXR/specs/1.0/man/html/xrGetInstanceProperties.html)) is not "Oculus".
- The OpenXR runtime does not advertise non-standard extension [`XR_META_headset_id`](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XR_META_headset_id.html).
- The plugin fails to detect legacy OVR API support through `ovr_Detect()`.

In addition to the checks above, Meta has crafted a user experience in the game engines that forces developers to select OVRPlugin while making it difficult to implement fallbacks to standard OpenXR support. In Unity for example, a developer selecting OpenXR support after having enabled OVRPlugin will be prompted with a message declaring "incompatibility" and reverting the selection of plugin to OVRPlugin exclusively.

Meta's OVRPlugin breaks the contract and assumptions built by OpenXR:
- A developer using an "OpenXR" middleware expects cross-platform and cross-vendor compatibility. Meta's OVRPlugin prevents this from happening. Meta has no incentive to solve this problem since the content will continue to work on their platform.
- A platform vendors (or runtime developer) implementing the OpenXR standard and seeking (paying) for [OpenXR conformance](https://www.khronos.org/conformance/adopters/conformant-products/openxr) expects OpenXR content to work on their platform. Meta's OVRPlugin is defining their own "conformance" and undermining the efforts of all other vendors in the industry who have proudly earned the OpenXR logo for their products.
- OpenXR conformance (CTS) contractors and contributors are implementing a test suite to help vendors create high-quality runtimes. Meta and their OVRPlugin is discrediting their work and making competitor's OpenXR implementation look incapable of running OpenXR content, while presenting their OpenXR runtime as the only adequate solution.

**This is not an accident**: this concern was reported to Meta early in 2024 via official means in the Khronos group. Meta acknowledged purposedly blocking other platforms from running OpenXR content at that time.

**This is not a technical limitation**: some runtimes (VDXR) have made great efforts to implement "compatibility" modes. There are counter-measures to unblock the content on any platform, but they are very expensive to investigate and implement.

**This is not a short-coming of OpenXR**: as proven with many applications using OVRPlugin with counter-measures enabled, these applications can run on a conformant OpenXR implementation.

For the past several years, Khronos has come up with best practices and solutions to develop OpenXR applications and maximize cross-vendor and cross-platform interoperability. Khronos has asked XR developers all over the world to follow these best practices, however - Meta - the largest vendor in Khronos is refusing to follow these best practices.

Instead of building on the work of each other to achieve success with OpenXR, Meta is reverting many of the improvements to the developers and users ecosystem that Khronos has spent time, money, energy into solving for the past 7 years.

Unfortunately, since 2024, Khronos has refused to take actions to stop Meta's OVRPlugin destructive initiative towards the PCVR ecosystem. By not taking any actions to resolve the issues created by Meta's OVRPlugin, Khronos is sending the message that OpenXR is no longer a universal solution for cross-vendor and cross-platform support, that passing the CTS and being conformant mean nothing (conformant runtimes are precluded from running OpenXR apps), and that the OpenXR logo and trademark no longer carry the same significance as before in the PCVR ecosystem.

As a developer, you must take immediate action and NOT USE OVRPLUGIN (AKA "OCULUS XR" PLUGIN) FOR ANY CONTENT THAT YOU DEVELOP.

## OVRPlugin Compatibility mode

This section is listed for reference to any platform developer who wishes to defeat OVRPlugin's platform restrictions.

- Return "Oculus" as the runtime name. You can simply make your runtime always return "Oculus" in [`xrGetInstanceProperties()`](https://registry.khronos.org/OpenXR/specs/1.0/man/html/xrGetInstanceProperties.html). A better technique is to detect OVRPlugin, as shown [here](https://github.com/mbucchia/VirtualDesktop-OpenXR/blob/510b3a2195fbfbe5b4ffe6c0c38808ceeb9e7d5a/virtualdesktop-openxr/instance.cpp#L238).

  In your `xrCreateInstance()` implementation, check for the application name passed in the [XrInstanceCreateInfo](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XrInstanceCreateInfo.html) and also check whether the DLL "OVRPlugin.dll" is loaded in the process' memory.

  ```
  m_isOculusXrPlugin =
      m_applicationName.find("Oculus VR Plugin") == 0 ||
      GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, "OVRPlugin.dll", &ovrPlugin);
  ```

- Advertise the `XR_META_headset_id` extension. This extension does not do anything useful, and it is just used by Meta to facilitate locking down applications to their platform. You can find an implementation of this extension [here](https://github.com/mbucchia/VirtualDesktop-OpenXR/commit/be9431186e4098259aff97d8c4a0033050bf2b00).

- Impersonate LibOVR to make `ovr_Detect()` pass. This one is tricky and it requires to fake the existence and state of the named event `OculusHMDConnected`. Because creating the event ourselves can be problematic (permissions, but also importantly the Oculus Services might be running in background and force the event to a specific state), you may use a Detours or hook to redirect the named event when `ovr_Detect()` (which is often statically linked into the application) calls `OpenEventW()`. You can find an implementation of this strategy [here](https://github.com/mbucchia/VirtualDesktop-OpenXR/commit/d8c306d4bb3b8622468017337895d799ab5d9101).

- Do not allow cube map swapchain unless your runtime supports [`XR_KHR_composition_layer_cube`](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XR_KHR_composition_layer_cube.html). The OVRPlugin does not check properly for this extension. If your runtime sees an `xrCreateSwapchain()` with a [`faceCount`](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XrSwapchainCreateInfo.html) of 6, you must fail to create the swapchain.

- Advertise and implement `XR_KHR_vulkan_enable`. Even when OVRPlugin will use Direct3D, the plugin will request Vulkan support for no reason, and not actually use Vulkan.

- Incorrect hand poses offset or hands are facing upward/downward - unfortunately there is no good solution to this issue at the time. This issue can be "resolved" in two manners:
  - For the hands facing upward and downward, the known workaround is for the runtime to un-advertise `XR_EXT_hand_tracking` or set `supportsHandTracking` to false when the application uses [xrGetSystemProperties()](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XrSystemHandTrackingPropertiesEXT.html).
  - For the incorrect offset - typically 10cm off the actual controller pose - the known workaround involves correcting the offset per-game, an example of it is shown for Contractors VR [here](https://github.com/mbucchia/VirtualDesktop-OpenXR/blob/36421c9ccb923d17318777049720b94108527412/virtualdesktop-openxr/instance.cpp#L291). 
