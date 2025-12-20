# Vincent

Vincent is a Qt 6 QML painting app.

## Requirements
- CMake 3.24+
- Qt 6.8+ modules: Core, Qml, Quick, QuickControls2, Svg
- A C++ compiler compatible with Qt 6

If Qt is installed in a non-standard path, set `CMAKE_PREFIX_PATH` or `Qt6_DIR` before configuring.

## Build
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

## Run
- Linux: `./build/bin/Vincent`
- macOS: `./build/Vincent.app/Contents/MacOS/Vincent`

## Packaging
- Linux (TGZ via CPack): `cmake --build build --target package`
- macOS App Store packaging: see `docs/BUILD.md`

## Tests
There are no automated tests yet.
