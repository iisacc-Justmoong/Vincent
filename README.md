# Vincent 4.0

Vincent 4.0 is a minimalist raster drawing app built with Qt 6.

## System Requirements
- macOS 12 or later (Apple Silicon or Intel)
- Windows 10 or later when using the Windows package
- Linux builds are available when provided by the release page

## Install (macOS)
1. Download `Vincent-4.0.pkg` from the latest release.
2. Double-click the installer and follow the prompts to install Vincent 4.0 in Applications.
3. Open Vincent 4.0 from Launchpad or the Applications folder.

## Install (macOS .app bundle)
1. Download `Vincent 4.0.app` from the release assets.
2. Drag it into the Applications folder.
3. Open Vincent 4.0 and allow any prompts to access files you select.

## Install (Linux)
1. Download the `Vincent-4.0-Linux.tar.gz` release archive.
2. Extract it and run the `Vincent` binary inside the extracted folder.

## Install (Windows)
1. Download `Vincent-4.0-Windows.zip` from the release assets.
2. Extract it to a writable folder such as `%LOCALAPPDATA%\Programs\Vincent`.
3. Run `Vincent.exe`.

Windows maintainers can generate the package from a Windows Qt/LVRS/iiPaintEngine environment with `powershell -ExecutionPolicy Bypass -File .\build-windows.ps1 -Clean`. The script stages `dist/Vincent-Windows`, creates `dist/Vincent-4.0-Windows.zip`, and can install to the current user with `-InstallForCurrentUser`.

## Features at a Glance
- iiPaintEngine-backed brush and eraser strokes with native pointer/tablet event handling and pressure-controlled brush opacity
- Layered raster document model with a base raster canvas plus selectable transparent raster layers; blank documents start with `Background` plus a selected transparent `Layer 1`, each backed by its own iiPaintEngine surface and undo/redo state
- Image open flow through Qt image formats such as PNG, JPEG, BMP, GIF, WebP, and TIFF plus PSD files read through `psd_sdk`; importable 8-bit RGB/RGBA PSD layers reopen as Vincent raster layers, and every flat image open replaces the raster canvas at the source image dimensions without fitting the image into the previous canvas
- Raster save flow that composites the base canvas, current raster layers, and current image, text, and shape objects
- Internal PSD compatibility document layer that maps the raster canvas and current session objects into Photoshop-style layer records and XMP metadata
- LVRS-backed MVVM document state with a compact C++ canvas document view model
- Toolbar file actions expose new canvas, open image, and save image buttons; new canvas opens a width/height modal before creating the raster
- The application window exposes a standard top menu bar with File, Edit, Window, and Help menus wired to the existing canvas dialogs, undo/redo, layer actions, tool and shape selection, brush-size changes, canvas view controls, window controls, and complete shortcut reference entries
- Platform builds use `resources/Appicon.icns` as the canonical macOS icon source and `resources/Appicon.ico` for Windows executable resources; run `tools/sync_app_icon_assets.sh` after replacing the macOS icon so the App Store/Xcode asset catalog stays in sync
- Save image defaults to Photoshop PSD and also offers PNG, JPEG, BMP, WebP, and TIFF; PSD export writes a merged preview plus the current `Background` canvas layer when present, current raster layers, and current image/text/shape objects in bottom-to-top order so Photoshop presents `Background` at the bottom, with Vincent layer metadata embedded as XMP
- A docked left-side LVRS hierarchy panel lists the background raster canvas and current image, shape, text, and raster layers; new blank canvases select the initial transparent `Layer 1` so brush strokes land on an editable layer immediately, double-clicking a layer name edits it inline, dragging rows changes layer order, and the plus/minus footer buttons create or remove layers, including the background layer; when `Background` is removed, the canvas shows a tiled PNG-style transparency grid
- Layer creation uses an incremental visual model so adding many raster layers does not recreate existing layer surfaces or write PNG snapshots for unchanged layers
- Pan, move, zoom, brush, eraser, shape, fill bucket, and paint-style text tools with a dense full-width top toolbar placed below the empty titlebar drag area, toolbar-frame and button x-axis padding set to half the y-axis padding, a current-color swatch with 4x right-edge padding that opens the HSL color picker, and brush controls that also set text size and text color
- Pan mode moves the canvas in the workspace by grabbing it with the hand tool, using viewport-relative movement offsets for smoother dragging; holding Space temporarily enters pan input without changing the selected tool
- Zoom mode scales the canvas by dragging horizontally anywhere in the canvas workspace: right to zoom in, left to zoom out
- Drag-to-insert solid shapes for rectangle, ellipse, triangle, diamond, star, rectangle bubble, and ellipse bubble, with Shift-constrained 1:1 bounds
- Inserted image, shape, and text objects are created as separate layer rows, can be selected with the move tool, dragged or resized past the canvas bounds from enlarged corner and edge transform handles while their visible pixels stay clipped to the canvas, Shift-dragged along the dominant straight movement axis, Shift-resized with their original aspect ratio, and deleted with Delete or Backspace before raster export
- Global menu shortcuts cover every actionable menu item: Command/Ctrl+N/O/S for new/open/save-as, Command/Ctrl+Z and platform redo for history, Command/Ctrl+Shift+N/Delete for layer creation/deletion, B/E/H/V/Z/U/G/T for tools, Command/Ctrl+Alt+1..7 for shape kinds, [/] for brush size, and Command/Ctrl+0/1/M plus platform fullscreen for window controls. Tool shortcuts also support matching two-beolsik Korean key positions on the canvas.
- Default brush hardness keeps iiPaintEngine's coverage-based edge anti-aliasing at its maximum app setting
- LVRS solid chrome keeps native window controls and the logical top drag handle available without restoring a visible titlebar; that reserved top area stays empty, shares the toolbar background color, and the toolbar starts below it
- Initial canvases are created inside the workspace with proportional top, side, and bottom margins so the dark workspace remains visible, while new canvases use the dimensions entered in the toolbar modal
- Large new canvases are automatically zoomed down on creation so the full canvas remains visible inside the current workspace viewport

## Testing
```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --target Vincent tests_canvasdocumentviewmodel tests_drawingsurfaceitem
ctest --test-dir build --output-on-failure
```

## Known Limitations
- PSD import reads 8-bit RGB/RGBA raster layers, layer names, bounds, opacity, visibility, Photoshop blend-mode keys, and Vincent XMP manifests when present; Photoshop smart objects, adjustment layers, layer effects, and editable text/vector reconstruction still fall back to raster compatibility data
- Transformable image objects remain session overlays when present, while added paint layers own their own raster pixels for deletion and export
- Palette is fixed to the built-in colors
- Canvas dimensions are clamped to the supported raster size range when entered through the new-canvas modal
