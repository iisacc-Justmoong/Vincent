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

## Missing Paint Features
Vincent intentionally keeps the feature set minimal. The current QML implementation lacks several capabilities that even a basic paint program is expected to provide:

- **Fill and shape tools are absent.** `App/qml/CanvasToolBar.qml` only wires up brush, eraser, grab, and text toggles, so there is no fill bucket or line/rectangle/ellipse primitive drawing workflow.
- **Brush strokes cannot be re-selected.** Once a stroke is rasterized inside `App/qml/DrawingSurface.qml` it becomes part of the flattened canvas and cannot be moved, scaled, cropped, or duplicated; the grab/free-transform overlay only works for imported bitmap/text elements.
- **Color choice is fixed to the built-in palette.** The palette assembled in `App/qml/PainterCanvasPage.qml` is the only way to pick colors—there is no eyedropper, custom color dialog, or numeric color entry.
- **Canvas size and export settings are rigid.** The canvas always matches the window-backed `DrawingSurface`, and `saveToFile()` simply captures that buffer, so you cannot set an explicit resolution, crop the canvas, or export with transparency/background options inside the app.
