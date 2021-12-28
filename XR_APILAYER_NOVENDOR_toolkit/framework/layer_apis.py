# /1\  Need to Rebuild the project for the Pre-build step to invoke the generator  /1\

# The list of OpenXR functions our layer will override.
override_functions = [
    "xrGetSystem",
    "xrEnumerateViewConfigurationViews",
    "xrCreateSession",
    "xrDestroySession",
    "xrCreateSwapchain",
    "xrDestroySwapchain",
    "xrEnumerateSwapchainImages",
    "xrAcquireSwapchainImage",
    "xrEndFrame"
]

# The list of OpenXR functions our layer will use from the runtime.
# Might repeat entries from override_functions above.
requested_functions = [
    "xrGetInstanceProperties",
    "xrEnumerateViewConfigurationViews",
    "xrEnumerateSwapchainImages"
]
