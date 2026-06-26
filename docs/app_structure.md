# Vincent 2.2.1 Application Structure

This document captures the flat-raster architecture of Vincent 2.2.1 after replacing the local painting implementation with iiPaintEngine.

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
3. `qt_add_qml_module` registers the `Vincent` QML module and exposes `Main.qml`, `PainterCanvasPage.qml`, `CanvasToolBar.qml`, `HslTriangleColorPicker.qml`, and `DrawingSurface.qml`.
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
- Keeps native close, minimize, and maximize controls while using LVRS solid chrome to suppress the visual title-bar strip.
- Enables LVRS's logical top drag handle so the window can move from the top area without adding a visible handle.
- Passes the active top drag-handle height into `PainterCanvasPage` so floating toolbar placement clears ApplicationWindow controls.
- Hosts `PainterCanvasPage` as the single content view.

### `PainterCanvasPage.qml`

- Binds the view to LVRS `ViewModels` using the stable `PainterCanvasPage` view ID.
- Reads brush and canvas state from `CanvasDocumentViewModel`.
- Offsets the floating toolbar by the reserved top chrome height plus the normal page gap, keeping its absolute top position below the ApplicationWindow control/drag region.
- Routes toolbar actions into either view-model property updates or `DrawingSurface` commands.

### `CanvasToolBar.qml`

- Provides the flat-raster command bar.
- Exposes actions for new, open, save, and clear, with open/save still available through shortcuts and dialogs.
- Renders the command bar inside a full-height solid round cylinder background.
- Renders the left command cluster inside the existing toolbar frame using LVRS `addFile`, `generaldelete`, `translateObject`, `showCode`, `eraser`, selected shape-kind, `fillbucket`, and `typeAlias` icons.
- Keeps existing behavior on the matching actionable icons: add creates a new canvas, delete clears the canvas, `translateObject` selects the move tool, pencil selects or reopens brush settings, eraser selects the eraser, the shape split button selects the shape tool from its body and opens the shape menu from its chevron, `fillbucket` selects the fill tool, and `typeAlias` selects the text tool.
- Offers shape menu entries for rectangle, ellipse, triangle, diamond, star, rectangle bubble, and ellipse bubble insertion through `LV.ContextMenu`, shows each shape icon in the menu, and mirrors the selected shape on the split button icon.
- Uses app-local vector sources for the `translateObject` and `typeAlias` slots while preserving the Figma/LVRS icon names. `translateObject` avoids the bundled embedded-image SVG warning, and `typeAlias` matches the Figma `typeAlias / Theme=Light` metadata instead of LVRS's different default icon shape.
- Restricts tools to brush, eraser, move, shape, fill, and text.
- Opens an HSL triangle color wheel from an RGB rainbow ball button instead of presenting enumerated palette swatches.
- Orders brush size controls as decrease button, slider, and increase button so the controls follow the value direction.
- Opens a brush settings menu when the already-selected brush tool is pressed again, with sliders for iiPaintEngine brush size, flow, opacity, hardness, spacing, and stabilizer strength. The pressure minimum, center, and maximum parameters sit at the bottom of the menu as a three-point curve graph instead of separate sliders.
- Restricts open/save dialogs to flat raster formats and handles default save extensions.

### `HslTriangleColorPicker.qml`

- Draws a hue ring and an inner HSL triangle with QML `Canvas`.
- Fills the inner triangle from the currently selected hue, with pure hue, white, and black as the three vertices.
- Maps triangle points to pure-hue, white, and black weights, then derives brush color through HSL saturation and lightness.
- Emits selected colors directly to the toolbar without depending on a predefined palette list.

### `DrawingSurface.qml`

- Hosts `DrawingSurfaceItem` as the editable raster surface.
- Lets iiPaintEngine handle mouse, tablet, live preview, stroke commit, eraser, undo, redo, open, and save behavior inside the item.
- Presents a drag-to-insert shape tool when shape mode is active; the QML preview follows the drag bounds, Shift constrains the drag bounds to a 1:1 ratio, then stores the selected shape outline as a transformable session object with the current brush color and brush size.
- Presents a fill tool between shape and text; clicking the canvas flood-fills the contiguous same-color raster region with the current brush color through `DrawingSurfaceItem`.
- Presents a paint-style text editor when the text tool is active; the editor sizes its frame from the longest text line within the remaining canvas width, uses the current brush size and brush color as its text size and color, then stores plain text as a transformable session object.
- Presents a move tool for inserted text and shape objects; clicking an object selects it, dragging the body moves it, and dragging a corner handle resizes its bounds before export.
- Uses a proportional workspace inset to create initial, new, and cleared canvases below the toolbar with visible dark margins instead of filling the window.
- Keeps an already-created canvas static; later window or view-model canvas dimension changes do not resize it.
- Presents only the fixed canvas area on a white paper background and leaves any resized viewport overflow in the LVRS workspace color, while keeping iiPaintEngine's raster layer semantics unchanged.
- Keeps QML responsible for viewport placement, wheel focus handling, keyboard shortcuts, and toolbar state binding.

## Core C++ Components

### `DrawingSurfaceItem`

- Inherits `CanvasAdapter` from iiPaintEngine.
- Preserves Vincent's previous QML-facing commands such as `newCanvas`, `openRaster`, `saveToFile`, `undo`, `redo`, and compatibility stroke methods.
- Exposes `commitText` so QML can pass text bounds, content, text size, and text color into Qt text layout and commit the result back through iiPaintEngine's raster replacement path.
- Exposes `commitShape` so QML can pass shape bounds, selected shape kind, stroke color, and stroke width into Qt painter paths and commit the result back through iiPaintEngine's raster replacement path.
- Exposes `fillAt` so QML can pass a canvas point and brush color into an exact-color flood fill that commits back through iiPaintEngine's raster replacement path.
- Exposes `saveToFileWithObjects` so QML can save a composite of the current raster canvas plus transformable text and shape session objects without flattening those objects into the live raster state.
- Synchronizes brush state, tool mode, and canvas dimensions with `CanvasDocumentViewModel` through `CanvasViewModelBridge`.
- Applies QML-driven canvas surface size updates atomically so startup resizing cannot leave partial 1-pixel dimensions in the document model.
- Exposes `backgroundSource` and `hasBackground` for the current flat raster document metadata.

### `CanvasDocumentViewModel`

- Exposes palette, brush color, brush size, iiPaintEngine brush settings, active tool, selected shape kind, and canvas dimensions to QML.
- Sets the default brush hardness to the app's maximum anti-aliased edge setting for iiPaintEngine's coverage-based circular brush.
- Clamps brush size, brush dynamics, pressure curve, stabilizer, and canvas dimensions to safe ranges.
- Restricts tool mode to the flat-raster tool set: brush, eraser, move, shape, fill, and text.

### `CanvasViewModelBridge`

- Resolves the active LVRS document view model.
- Blocks canvas mutation until the expected document/view binding is available.
- Keeps the model's canvas size, tool state, and iiPaintEngine brush config aligned with the rendered surface.

## Data Flow Summary

1. `main.cpp` launches the LVRS-backed QML application and registers the shared view model plus helper services.
2. `PainterCanvasPage` binds to `CanvasDocumentViewModel` and passes brush state into `DrawingSurface`.
3. `CanvasToolBar` emits user actions for file flow, tool selection, shape selection, HSL color picker changes, brush size updates, and brush reselection settings.
4. `DrawingSurface` hosts `DrawingSurfaceItem`; the item delegates raster operations to iiPaintEngine's `CanvasAdapter`.
5. iiPaintEngine owns stroke rasterization, live preview, commit, eraser compositing, undo/redo snapshots, raster open, and raster save; Vincent keeps inserted text and shape as QML session objects, renders them as overlays, applies fill replacements with Qt image APIs, and composites objects with the raster canvas during save.

## Testing Surface

- `tests/tst_canvasdocumentviewmodel.cpp` validates the flat-raster document state and value clamping, including iiPaintEngine brush settings, supported tool modes, and supported shape kinds.
- `tests/tst_canvastoolbarqmlcontract.cpp` validates QML toolbar contracts, including brush reselection settings, move-tool selection, shape split-menu selection, fill-tool selection, text-tool selection, object-transform hooks, and HSL triangle color-picker usage.
- `tests/tst_mainqmlcontract.cpp` validates the LVRS application-window chrome contract, including native controls and the logical top drag handle.
- `tests/tst_drawingsurfaceitem.cpp` validates the Vincent-to-iiPaintEngine adapter path for drawing, erasing, fill, text and shape raster commit, transformable object movement/resizing, composite object saving, undo/redo, saving, opening rasters, and workspace-inset canvas creation.
- Run the suite with `ctest --test-dir build --output-on-failure` after configuring with `-DBUILD_TESTING=ON`.
