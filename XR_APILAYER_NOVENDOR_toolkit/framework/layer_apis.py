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
    "xrEnumerateSwapchainImages",
    "xrAcquireSwapchainImage",
    "xrPollEvent",
    "xrGetCurrentInteractionProfile",
    "xrLocateViews",
    "xrLocateSpace",
    "xrSyncActions",
    "xrGetActionStateBoolean",
    "xrGetActionStateFloat",
    "xrGetActionStatePose",
    "xrWaitFrame",
    "xrBeginFrame",
    "xrEndFrame"
]

# The list of OpenXR functions our layer will use from the runtime.
# Might repeat entries from override_functions above.
requested_functions = [
    "xrGetInstanceProperties",
    "xrEnumerateViewConfigurationViews",
    "xrEnumerateSwapchainImages",
    "xrCreateReferenceSpace",
    "xrPathToString",
    "xrStringToPath"
]
