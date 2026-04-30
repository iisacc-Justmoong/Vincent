# Vincent 2.2.1

Vincent 2.2.1 is a minimalist raster drawing app built with Qt 6.

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
- Drawing-only document model with a single flat raster canvas
- Flat raster open flow for common image formats such as PNG, JPEG, BMP, GIF, WebP, and TIFF
- Flat raster save flow for PNG, JPEG, and BMP
- LVRS-backed MVVM document state with a compact C++ canvas document view model
- C++ canvas backend for undo/redo snapshots and document-fit placement
- Brush and eraser tools with quick color palette and brush size controls

## Testing
```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --target Vincent tests_brushengine tests_canvasdocumentviewmodel tests_canvasbackend tests_rasterdocumentio
ctest --test-dir build --output-on-failure
```

## Known Limitations
- No layer stack, PSD import/export, blend modes, or transform tools
- No shape tools, fill bucket, or text tool
- Opened raster images are fit onto the current canvas and then edited as part of the flat drawing surface
- Stroke rasterization still lives in the QML `Canvas`
- Palette is fixed to the built-in colors
- Canvas size follows the current drawing surface when you create a new canvas
