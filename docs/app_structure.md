# Vincent 2.2.1 Application Structure

This document captures the drawing-only architecture of Vincent 2.2.1 after PSD, layer, and transform compatibility features were removed.

## Top-Level Layout

- `CMakeLists.txt` (root) – bootstraps Qt, LVRS, packaging, and the `App/` subdirectory.
- `App/` – contains the application bundle sources.
- `App/brushengine.h`, `App/brushengine.cpp` – pressure-aware brush dynamics exposed to QML.
- `App/canvasbackend.h`, `App/canvasbackend.cpp` – snapshot history and document-fit math for the drawing surface.
- `App/canvasdocumentviewmodel.h`, `App/canvasdocumentviewmodel.cpp` – LVRS-facing document state for brush color, brush size, active tool, and canvas dimensions.
- `App/paletteutils.h`, `App/paletteutils.cpp` – palette ordering helper exposed to QML.
- `App/rasterdocumentio.h`, `App/rasterdocumentio.cpp` – flat raster file validation and metadata loading for the open flow.
- `App/qml/` – QML UI for the main window, toolbar, and drawing surface.
- `tests/` – Qt Test targets for brush behavior, canvas history, document state, and raster document loading.

## Build System Overview

1. The root `CMakeLists.txt` sets up Qt 6, LVRS, install paths, packaging metadata, and the `Vincent` executable target.
2. `App/CMakeLists.txt` attaches the C++ sources and headers to the `Vincent` target.
3. `qt_add_qml_module` registers the `Vincent` QML module and exposes `Main.qml`, `PainterCanvasPage.qml`, `CanvasToolBar.qml`, and `DrawingSurface.qml`.
4. The executable links against Qt Core, QML, Quick, Quick Controls 2, and SVG, then LVRS configures runtime QML import handling.
5. When `BUILD_TESTING=ON`, `tests/CMakeLists.txt` registers the unit test targets.

## Runtime Entry Point (`App/main.cpp`)

- Configures LVRS application launch metadata.
- Adds LVRS QML import paths discovered from local installation locations.
- Registers `CanvasBackend`, `BrushEngine`, `PaletteUtils`, and `RasterDocumentIO` as QML context objects.
- Registers a shared `CanvasDocumentViewModel` in the LVRS `ViewModels` registry under `CanvasDocument`.
- Launches the `Vincent` QML module's `Main` component.

## QML Module Layout (`App/qml/`)

### `Main.qml`

- Creates the main LVRS application window.
- Hosts `PainterCanvasPage` as the single content view.

### `PainterCanvasPage.qml`

- Binds the view to LVRS `ViewModels` using the stable `PainterCanvasPage` view ID.
- Reads brush and canvas state from `CanvasDocumentViewModel`.
- Routes toolbar actions into either view-model property updates or `DrawingSurface` commands.

### `CanvasToolBar.qml`

- Provides the drawing-only command bar.
- Exposes actions for new, open, save, and clear.
- Restricts tools to brush and eraser.
- Restricts open/save dialogs to flat raster formats and handles default save extensions.

### `DrawingSurface.qml`

- Owns transient stroke data, the opened flat raster background, and the undo/redo flow.
- Uses `PointHandler` for mouse and pen input and delegates pressure response to `BrushEngine`.
- Draws the opened raster background plus all strokes into a single QML `Canvas`, so erasing affects the flattened document rather than a separate compatibility layer.
- Uses `CanvasBackend` to capture deep-cloned snapshots for undo/redo and to calculate fit placement for opened rasters.
- Uses `RasterDocumentIO` to validate and inspect raster files before they are loaded into the canvas.

## Core C++ Components

### `CanvasDocumentViewModel`

- Exposes palette, brush color, brush size, active tool, and canvas dimensions to QML.
- Clamps brush size and canvas dimensions to safe ranges.
- Restricts tool mode to the drawing-only set.

### `CanvasBackend`

- Stores undo and redo stacks outside QML object graphs.
- Deep-clones stroke lists and flattened background metadata for history snapshots.
- Calculates scale and offsets to fit an opened raster into the current canvas.

### `RasterDocumentIO`

- Accepts local file paths or file URLs.
- Rejects PSD and any unsupported formats.
- Uses `QImageReader` to validate raster files and report normalized source URL plus dimensions.

### `BrushEngine`

- Converts stylus pressure into brush size and opacity.
- Smooths abrupt brush changes and decides when a new sample should be appended.
- Provides stamp density hints for variable-width raster stroke rendering.

## Data Flow Summary

1. `main.cpp` launches the LVRS-backed QML application and registers the shared view model plus helper services.
2. `PainterCanvasPage` binds to `CanvasDocumentViewModel` and passes brush state into `DrawingSurface`.
3. `CanvasToolBar` emits user actions for file flow, tool selection, palette changes, and brush size updates.
4. `DrawingSurface` renders a single flat document by combining an optional opened raster background with live brush and eraser strokes.
5. Undo and redo are backed by `CanvasBackend` snapshots instead of QML-only object copies.

## Testing Surface

- `tests/tst_brushengine.cpp` validates pressure handling, smoothing, append heuristics, and stamp density.
- `tests/tst_canvasbackend.cpp` validates snapshot cloning, undo/redo history, and fit placement calculations.
- `tests/tst_canvasdocumentviewmodel.cpp` validates the drawing-only document state and value clamping.
- `tests/tst_rasterdocumentio.cpp` validates flat raster format support and raster metadata loading.
- Run the suite with `ctest --test-dir build --output-on-failure` after configuring with `-DBUILD_TESTING=ON`.
