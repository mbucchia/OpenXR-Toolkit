# The list of OpenXR functions our layer will override.
override_functions = [
    "xrCreateSession",
    "xrDestroySession"
]

# The list of OpenXR functions our layer will use from the runtime.
# Might repeat entries from override_functions above.
requested_functions = [
    "xrGetSystem"
]
