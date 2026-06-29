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
- iiPaintEngine-backed brush and eraser strokes with native pointer and tablet event handling
- Flat raster document model with a single canvas and engine-managed undo/redo
- Image open flow through Qt image formats such as PNG, JPEG, BMP, GIF, WebP, and TIFF plus PSD files read through `psd_sdk`, with oversized images fitted inside the current workspace as transformable image objects so the initial canvas stays smaller than the window
- Flat raster save flow that composites the iiPaintEngine raster canvas with current image, text, and shape objects
- Internal PSD compatibility document layer that maps the raster canvas and current session objects into Photoshop-style layer records for future import/export work
- LVRS-backed MVVM document state with a compact C++ canvas document view model
- Toolbar file actions expose new canvas, open image, and save image buttons; new canvas opens a width/height modal before creating the raster
- Save image defaults to Photoshop PSD and also offers PNG, JPEG, BMP, WebP, and TIFF; PSD export writes the current visible composite as a flat 8-bit RGB/RGBA document
- Pan, move, zoom, brush, eraser, shape, fill bucket, and paint-style text tools with the Figma-aligned left toolbar, a current-color swatch that opens the HSL color picker, and brush controls that also set text size and text color
- Pan mode moves the canvas in the workspace by grabbing it with the hand tool, using viewport-relative movement offsets for smoother dragging
- Zoom mode scales the canvas by dragging horizontally: right to zoom in, left to zoom out
- Drag-to-insert solid shapes for rectangle, ellipse, triangle, diamond, star, rectangle bubble, and ellipse bubble, with Shift-constrained 1:1 bounds
- Inserted image, shape, and text objects can be selected with the move tool, dragged, resized from corner transform handles, and deleted with Delete or Backspace before raster export
- Tool shortcuts follow common paint-editor keys: B brush, E eraser, H hand-pan, V move, Z zoom, U shape, G fill, and T text, with matching two-beolsik Korean key positions supported
- Default brush hardness keeps iiPaintEngine's coverage-based edge anti-aliasing at its maximum app setting
- LVRS solid chrome keeps the top window drag handle logically active without adding a visible title strip, while the toolbar is offset below that reserved chrome area
- Initial canvases are created inside the workspace with proportional top, side, and bottom margins so the dark workspace remains visible, while new canvases use the dimensions entered in the toolbar modal
- Large new canvases are automatically zoomed down on creation so the full canvas remains visible inside the current workspace viewport

## Testing
```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --target Vincent tests_canvasdocumentviewmodel tests_drawingsurfaceitem
ctest --test-dir build --output-on-failure
```

## Known Limitations
- PSD import currently reads the merged/flat image only; no layered PSD import/export, blend modes, layer effects, masks, or persisted vector-object project file yet. The PSD compatibility layer is an internal manifest contract and `.psd` save currently exports a flat composite
- Opened image objects are session overlays, not persisted vector/layer document records
- Palette is fixed to the built-in colors
- Canvas dimensions are clamped to the supported raster size range when entered through the new-canvas modal
