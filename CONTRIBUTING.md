# Contributing

- Keep the production core in portable C17 with no heap allocation, recursion, or mutable global state.
- Preserve deterministic behavior, bounded storage, transactional failure semantics, and canonical wire encoding.
- Add or update tests for semantic changes. Do not silently change the serialized path format or event encoding.
- Keep platform-specific code outside `src/lunapath.c` and `include/lunapath.h`.

Build and run the profile tests with:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The independent Python oracle and extended deterministic fault campaign can be run with `python tools/validate.py`. Python is only needed for extended validation, not for the library build.
