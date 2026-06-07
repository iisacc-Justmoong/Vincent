# Vincent 2.2.1 Application Structure

This document captures the drawing-only architecture of Vincent 2.2.1 after replacing the local painting implementation with iiPaintEngine.

## Top-Level Layout

- `CMakeLists.txt` (root) bootstraps Qt, LVRS, iiPaintEngine, packaging, and the `App/` subdirectory.
- `App/` contains the application bundle sources.
- `App/models/canvas/canvasdocumentviewmodel.*` stores LVRS-facing document state for brush color, brush size, active tool, and canvas dimensions.
- `App/models/canvas/canvasviewmodelbridge.*` gates drawing mutations through the LVRS document view model and synchronizes canvas metadata.
- `App/models/brush/paletteutils.*` provides palette ordering helpers exposed to QML.
- `App/models/painting/drawingsurfaceitem.*` adapts Vincent's QML surface contract to `CanvasAdapter` from iiPaintEngine.
- `App/qml/` contains the LVRS UI for the main window, toolbar, and drawing surface.
- `tests/` contains Qt Test targets for the document view model and iiPaintEngine drawing surface integration.

## Build System Overview

1. The root `CMakeLists.txt` sets up Qt 6, LVRS, iiPaintEngine, install paths, packaging metadata, and the `Vincent` executable target.
2. `App/CMakeLists.txt` attaches the C++ sources and headers to the `Vincent` target.
3. `qt_add_qml_module` registers the `Vincent` QML module and exposes `Main.qml`, `PainterCanvasPage.qml`, `CanvasToolBar.qml`, and `DrawingSurface.qml`.
4. The executable links against Qt Core, QML, Quick, Quick Controls 2, SVG, and `iiPaintEngine::iiPaintEngine`, then LVRS configures runtime QML import handling.
5. When `BUILD_TESTING=ON`, `tests/CMakeLists.txt` registers the active unit test targets.

## Runtime Entry Point (`App/main.cpp`)

- Configures LVRS application launch metadata.
- Adds LVRS QML import paths discovered from local installation locations.
- Registers `DrawingSurfaceItem` as the QML canvas item.
- Registers `PaletteUtils` as a QML context helper.
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

- Hosts `DrawingSurfaceItem` as the editable raster surface.
- Lets iiPaintEngine handle mouse, tablet, live preview, stroke commit, eraser, undo, redo, open, and save behavior inside the item.
- Uses the available surface size as the initial canvas size while the document model still has its default 1x1 dimensions.
- Keeps QML responsible for viewport placement, wheel focus handling, keyboard shortcuts, and toolbar state binding.

## Core C++ Components

### `DrawingSurfaceItem`

- Inherits `CanvasAdapter` from iiPaintEngine.
- Preserves Vincent's previous QML-facing commands such as `newCanvas`, `openRaster`, `saveToFile`, `undo`, `redo`, and compatibility stroke methods.
- Synchronizes brush state, tool mode, and canvas dimensions with `CanvasDocumentViewModel` through `CanvasViewModelBridge`.
- Applies QML-driven canvas surface size updates atomically so startup resizing cannot leave partial 1-pixel dimensions in the document model.
- Exposes `backgroundSource` and `hasBackground` for the current flat raster document metadata.

### `CanvasDocumentViewModel`

- Exposes palette, brush color, brush size, active tool, and canvas dimensions to QML.
- Clamps brush size and canvas dimensions to safe ranges.
- Restricts tool mode to the drawing-only set.

### `CanvasViewModelBridge`

- Resolves the active LVRS document view model.
- Blocks canvas mutation until the expected document/view binding is available.
- Keeps the model's canvas size and tool state aligned with the rendered surface.

## Data Flow Summary

1. `main.cpp` launches the LVRS-backed QML application and registers the shared view model plus helper services.
2. `PainterCanvasPage` binds to `CanvasDocumentViewModel` and passes brush state into `DrawingSurface`.
3. `CanvasToolBar` emits user actions for file flow, tool selection, palette changes, and brush size updates.
4. `DrawingSurface` hosts `DrawingSurfaceItem`; the item delegates raster operations to iiPaintEngine's `CanvasAdapter`.
5. iiPaintEngine owns stroke rasterization, live preview, commit, eraser compositing, undo/redo snapshots, raster open, and raster save.

## Testing Surface

- `tests/tst_canvasdocumentviewmodel.cpp` validates the drawing-only document state and value clamping.
- `tests/tst_drawingsurfaceitem.cpp` validates the Vincent-to-iiPaintEngine adapter path for drawing, erasing, undo/redo, saving, and opening rasters.
- Run the suite with `ctest --test-dir build --output-on-failure` after configuring with `-DBUILD_TESTING=ON`.
