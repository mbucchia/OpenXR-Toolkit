In order to make a release with number X.Y.Z:

1) Create or update the branch `release/x.y`

2) Update the definitions in `XR_APILAYER_NOVENDOR_toolkit/layer.h`:

```
    const uint32_t VersionMajor = 0;
    const uint32_t VersionMinor = 0;
    const uint32_t VersionPatch = 0;
    const std::string VersionString = "Unreleased";
```

eg:

```
    const uint32_t VersionMajor = 0;
    const uint32_t VersionMinor = 9;
    const uint32_t VersionPatch = 4;
    const std::string VersionString = "Beta-2";
```

3) Update the label in `installer/README.rtf`:

```
OpenXR Toolkit - Development Build (v0.0.0)
```

eg:

```
OpenXR Toolkit - Beta-2 (v0.9.4)
```

4) Update the `Setup` project, property `Version`. When prompted to update the product code, choose Yes.

5) Push the release to the server.

6) Create a tag for the release: `x.y.z`.
