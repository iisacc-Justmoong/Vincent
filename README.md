# Vincent 2.2.1

Vincent 2.2.1 is a minimalist painting app built with Qt 6.

## System Requirements
- macOS 12 or later (Apple Silicon or Intel)
- Linux builds are available when provided by the release page

## Install (macOS)
1. Download `Vincent-2.2.1.pkg` from the latest release.
2. Double-click the installer and follow the prompts to install Vincent 2.2.1 in Applications.
3. Open Vincent 2.2.1 from Launchpad or the Applications folder.

## Install (macOS .app bundle)
1. Download `Vincent 2.2.1.app` from the release assets.
2. Drag it into the Applications folder.
3. Open Vincent 2.2.1 and allow any prompts to access files you select.

## Install (Linux)
1. Download the `Vincent-2.2.1-Linux.tar.gz` release archive.
2. Extract it and run the `Vincent` binary inside the extracted folder.

## Features at a Glance
- Pressure-sensitive size and opacity for stylus brush and eraser strokes
- PSD import that can split document layers into separate in-app layers while capturing PSD metadata at import time
- PSD export that preserves imported layers and writes canvas strokes as a top `Paint` layer
- LVRS-backed MVVM document state with a C++ canvas document view model and layer list model
- C++ canvas backend for undo/redo snapshots, document fit, and free-transform geometry resolution
- Brush, eraser, grab, and text tools
- Layer panel for selecting imported layers and toggling visibility
- Quick color palette and brush size controls
- Save canvases to PNG

## Testing
```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --target tests_brushengine tests_imageimport tests_canvasdocumentviewmodel tests_canvasbackend tests_imageexport
ctest --test-dir build --output-on-failure
```

## Known Limitations
- No shape tools or fill bucket yet
- Strokes are rasterized and cannot be re-selected
- Stroke rasterization still lives in the QML `Canvas`; only snapshot history and transform math have moved to the C++ canvas backend so far
- PSD export currently writes 8-bit raw RGB+alpha documents and rasterizes live strokes into a single `Paint` layer
- Palette is fixed to the built-in colors
- Canvas size follows the window size
- PSD blend modes are preserved as metadata, but rendering currently uses standard alpha compositing
