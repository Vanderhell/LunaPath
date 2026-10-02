# ESP32-S3 validation application

This isolated ESP-IDF application exercises the same `src/lunapath.c` portable core as the host build. It includes focused tests, host-known event vectors, deterministic replays, a long-run check, a fault subset, and cycle benchmarks.

With ESP-IDF installed and exported, configure a profile and build:

```sh
idf.py -DLUNAPATH_PATH_BITS=64 -DLUNAPATH_GUARD=3 -DLUNAPATH_USE_ESP_CRC32=ON build
idf.py -p <PORT> app-flash
idf.py -p <PORT> monitor
python collect_hw_runs.py --port <PORT> --runs 3
```

`app-flash` writes the application partition only. It does not erase the full flash or write eFuses, security settings, or unrelated data partitions. The portable C CRC remains the reference; the ESP ROM backend is optional and must match its results.

The replay capture script uses `pyserial`. It is not required for the library, host CMake build, or ESP-IDF firmware build.
