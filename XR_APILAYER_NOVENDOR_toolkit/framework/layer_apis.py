# The list of OpenXR functions our layer will override.
override_functions = [
    "xrGetSystem",
    "xrEnumerateViewConfigurationViews",
    "xrCreateSession",
    "xrDestroySession",
    "xrCreateSwapchain",
    "xrDestroySwapchain",
    "xrSuggestInteractionProfileBindings",
    "xrCreateAction",
    "xrDestroyAction",
    "xrCreateActionSpace",
    "xrDestroySpace",
    "xrAttachSessionActionSets",
    "xrEnumerateSwapchainImages",
    "xrAcquireSwapchainImage",
    "xrReleaseSwapchainImage",
    "xrPollEvent",
    "xrGetCurrentInteractionProfile",
    "xrLocateViews",
    "xrLocateSpace",
    "xrSyncActions",
    "xrGetActionStateBoolean",
    "xrGetActionStateFloat",
    "xrGetActionStatePose",
    "xrApplyHapticFeedback",
    "xrStopHapticFeedback",
    "xrWaitFrame",
    "xrBeginFrame",
    "xrEndFrame"
]

# The list of OpenXR functions our layer will use from the runtime.
# Might repeat entries from override_functions above.
requested_functions = [
    "xrGetSystem",
    "xrGetInstanceProperties",
    "xrGetSystemProperties",
    "xrEnumerateViewConfigurationViews",
    "xrEnumerateSwapchainImages",
    "xrCreateActionSet",
    "xrDestroyActionSet",
    "xrCreateAction",
    "xrDestroyAction",
    "xrCreateActionSpace",
    "xrCreateReferenceSpace",
    "xrDestroySpace",
    "xrLocateSpace",
    "xrSyncActions",
    "xrGetActionStatePose",
    "xrPathToString",
    "xrStringToPath"
]
