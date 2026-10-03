# Releasing LunaPath

For each release:

1. Start from a clean working tree and verify the branch and intended release commit.
2. Check the project version in `CMakeLists.txt` and `include/lunapath.h`.
3. Review `CHANGELOG.md` for the release.
4. Run the current-tree and full reachable-history privacy scans.
5. Run the full local host regression suite.
6. Push the release candidate to `master`.
7. Require hosted CI to pass on the exact intended release commit SHA.
8. Create an annotated version tag pointing to that exact SHA.
9. Push only that tag and verify its remote SHA.
10. Create the GitHub Release from the same tag.

**Never create the release tag before hosted CI passes on the exact intended release commit.**

For semantic changes to the core, public API, wire behavior, or ESP32 backend, rerun ESP32-S3 hardware validation. Documentation-only and CI-only changes do not automatically require a hardware rerun.
