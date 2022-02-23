In order to make a release with number X.Y.Z:

1) Create or update the branch `release/x.y`

2) Create or update the definitions in `version.info`, eg:

```
major=0
minor=9
patch=4
pretty_name=Beta-2
```

3) Update the label in `installer/README.rtf`, eg:

```
OpenXR Toolkit - Beta-2 (v0.9.4)
```

4) Update the `Setup` project, property `Version`. When prompted to update the product code, choose Yes.

5) Push the release to the server.

6) Create a tag for the release: `x.y.z`.
