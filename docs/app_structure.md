# Vincent 2.2.1 Application Structure

This document captures the current architecture of Vincent 2.2.1 as observed in the repository. It describes how the project is laid out, how the build system is wired, and how the runtime pieces cooperate to deliver the painting experience.

## Top-Level Layout

- `CMakeLists.txt` (root) – bootstraps the Qt build, configures install paths, and delegates to the application sources in `App/`.
- `App/` – contains all C++ and QML code for the application bundle.
- `App/brushengine.h`, `App/brushengine.cpp` – pressure-aware brush dynamics engine exposed to QML.
- `App/canvasbackend.h`, `App/canvasbackend.cpp` – canvas support backend for snapshot history, fit transforms, and free-transform geometry resolution.
- `App/canvasdocumentviewmodel.h`, `App/canvasdocumentviewmodel.cpp` – LVRS-facing document view model that owns brush state, canvas sizing, selection state, and imported layers.
- `App/imageexport.h`, `App/imageexport.cpp` – PSD export service that serializes the current document into layered Photoshop files.
- `App/imageimport.h`, `App/imageimport.cpp` – image import service that passes standard rasters through and decodes PSD composites into cacheable PNGs for QML.
- `App/layerlistmodel.h`, `App/layerlistmodel.cpp` – `QAbstractListModel` implementation for imported image layers, including PSD metadata payloads.
- `App/paletteutils.h`, `App/paletteutils.cpp` – palette ordering helper exposed to QML as a context object.
- `resources/` – SVG icons and design assets consumed by the QML UI.
- `build/`, `cmake-build-debug/` – out-of-source build trees (ignored in project description, but important to keep generated artifacts isolated).

No other product source directories are present at this time.

## Build System Overview

The project relies on CMake and Qt 6 modules.

1. The root `CMakeLists.txt` ensures Craft-provided prefixes take priority when `CRAFTROOT` is set. It configures GNU install paths before adding the `App/` subdirectory.
2. `App/CMakeLists.txt` ties the sources to the `Vincent` target while the root file links against the Qt stack (`Qt6::Core`, `Qt6::Qml`, `Qt6::Quick`, `Qt6::QuickControls2`, `Qt6::Svg`).
3. A single executable target, `Vincent`, is defined around `App/main.cpp`.
4. `qt_add_qml_module` registers the `Vincent` QML module version 2.1, exposing the components under `App/qml/` to the QML engine at runtime.
5. macOS-specific blocks adjust OpenGL discovery so Qt Quick works even when SDK headers are missing from the default search paths.
6. The executable links privately against the Qt targets and is installed via standard GNU install dir settings.
7. CPack is configured for macOS productbuild packaging and a Linux TGZ package.

## Runtime Entry Point (`App/main.cpp`)

- Creates the `QGuiApplication` instance that hosts the Qt Quick scene graph.
- Registers the `BrushEngine` singleton-style context object used by the QML canvas for pressure-aware stroke sampling.
- Registers the `CanvasBackend` context object used by QML to offload snapshot cloning, undo/redo history, and transform math into C++.
- Registers the `ImageExport` context object used by QML to save layered PSD documents.
- Registers the `ImageImport` context object used by QML to normalize raster imports and decode PSD files before they reach the canvas.
- Registers the `PaletteUtils` context object for palette computation.
- Resolves the LVRS `ViewModels` registry and registers a shared `CanvasDocumentViewModel` under the `CanvasDocument` key before the main view loads.
- Configures a `QQmlApplicationEngine` and augments its import paths when `CRAFTROOT` exposes prebuilt QML modules.
- Connects `objectCreationFailed` to `QCoreApplication::exit(-1)` for fail-fast behavior if the QML scene cannot load.
- Loads the `Vincent` QML module's `Main` component and starts the event loop.

Stroke rasterization and pointer interaction still live in QML, while persistent document-facing state plus snapshot history and transform geometry are now modeled in C++.

## QML Module Layout (`App/qml/`)

### `Main.qml`

- Declares a `Controls.ApplicationWindow` with fixed initial dimensions and title.
- Stores a reference to the active canvas page for cross-component coordination.
- Instantiates `PainterCanvasPage` as the lone content item and exposes the page instance via the `pageReady` signal.

### `PainterCanvasPage.qml`

- Extends `Controls.Page` to host the main drawing surface.
- Binds the view to LVRS `ViewModels` using the stable `PainterCanvasPage` view ID and the shared `CanvasDocument` model key.
- Reads user-facing document state (`brushColor`, `brushSize`, `toolMode`, canvas size, palette) from `CanvasDocumentViewModel` instead of duplicating that state in QML.
- Emits `pageReady` when the component loads to let `Main.qml` grab a pointer.
- Provides imperative helpers (`newCanvas`, `clearCanvas`, `saveCanvasAs`, `openImage`, `adjustBrush`) that wrap the lower-level `DrawingSurface` API while routing simple state changes back through `LV.ViewModels.updateProperty`.
- Instantiates the `CanvasToolBar` as the page header and wires its signals into the bound document view model.
- Hosts the `DrawingSurface` inside a `Rectangle`, forwarding the bound document view model and listening for scroll-wheel-driven size changes.

### `CanvasToolBar.qml`

- Implements the horizontal toolbar purely with Qt Quick Controls.
- Defines a local `ToolbarButton` component so every button shares the same icon slot, spacing, and padding rules.
- Exposes signals for high-level actions (new, open, save, clear) and tool adjustments (brush size, tool selection, palette picks).
- Provides `Dialogs.FileDialog` instances for open/save flows, including PSD-aware open filters and extension inference when a filename lacks a suffix.
- Extends the save flow with a PSD option so the document can be exported either as a flattened raster or as a layered Photoshop file.
- Presents quick-access buttons, tool toggles, a size slider with increment/decrement buttons, and a color palette repeater that highlights the active swatch.

### `DrawingSurface.qml`

- Renders the actual canvas within a rounded `Rectangle`.
- Manages drawing state (`strokes`, `currentStroke`, and transform overlays) and tool behavior.
- Uses `PointHandler` for stylus and mouse stroke capture, enabling pressure-sensitive brush sampling by default when a pen device reports pressure.
- Stores each stroke as a sequence of sampled points that carry normalized pressure, resolved brush diameter, and resolved opacity.
- Uses a `Canvas` element to batch-render all strokes; variable-width strokes are rasterized by interpolated stamp placement rather than fixed-width `lineTo()` segments, and each stamp carries its own alpha.
- Supports eraser mode through pressure-sensitive `destination-out` compositing strength, wheel-based brush size adjustments via the `brushDeltaRequested` signal, and optional background image loading.
- Routes all opened or dropped images through `ImageImport`, so PSD documents can arrive either as a flattened raster fallback or as multiple positioned image layers.
- Routes `.psd` saves through `ImageExport`, which serializes the current imported layers plus a top rasterized paint layer while keeping raster exports on the existing `grabToImage` path.
- Treats imported layers as a `LayerListModel` supplied by `CanvasDocumentViewModel`; imports, removal, visibility changes, geometry edits, and selection updates go through the view model instead of a QML `ListModel`.
- Delegates undo/redo snapshot capture, document-fit placement, imported-image reset placement, and free-transform rectangle solving to `CanvasBackend` so repeated deep cloning and geometry math do not run in QML JavaScript.
- Honors LVRS single-writer semantics for document mutations after binding, while still allowing pre-bind initialization to seed canvas dimensions during startup.
- Preserves imported image stacking order during selection and exposes a right-side layer panel for selecting layers and toggling visibility without reordering the canvas stack.
- Normalizes file URLs for loading and saving, ensuring compatibility with both `file://` URIs and bare paths.

### `CanvasDocumentViewModel` (`App/canvasdocumentviewmodel.*`)

- Owns the document-facing MVVM state shared by the canvas page.
- Exposes palette, brush color, brush size, current tool, selected layer ID, selected layer metadata, canvas dimensions, and layer count as QML-friendly properties.
- Provides mutation methods for appending, importing, updating, moving, hiding, and removing layers by stable image ID.
- Keeps selection state valid when layers are imported, removed, or replaced from undo/redo snapshots.

### `LayerListModel` (`App/layerlistmodel.*`)

- Implements the imported-layer store as a typed `QAbstractListModel`.
- Exposes each layer's source URL, geometry, readiness, visibility, opacity, blend mode key, and import metadata to QML delegates.
- Supports imperative mutation (`append`, `remove`, `move`, `setProperty`, `importEntries`, `exportEntries`) so existing QML interactions can migrate gradually without losing undo/redo compatibility.

### `BrushEngine` (`App/brushengine.*`)

- Centralizes the brush-response policy so pressure handling is not duplicated in QML.
- Resolves raw stylus pressure into usable size and opacity curves, clamps minimum visible stroke size, smooths sudden width and alpha jumps, and decides when a new sample is significant enough to append.
- Provides segment stamp density hints so the QML renderer can fill gaps while preserving variable stroke width and opacity.

### `CanvasBackend` (`App/canvasbackend.*`)

- Owns canvas-local undo and redo history outside QML object graphs.
- Deep-clones stroke and imported-layer snapshots in C++ so undo capture no longer depends on large JavaScript object walks.
- Resolves document-fit placement, imported-image reset placement, and free-transform rectangle geometry for the QML surface.
- Exposes `canUndo` and `canRedo` state so QML can react to history availability without managing parallel stack arrays.

### `ImageExport` (`App/imageexport.*`)

- Accepts the current canvas size, imported-layer snapshot, and a rasterized paint overlay from QML.
- Writes 8-bit PSD files with a raw RGBA composite image and explicit layer records.
- Preserves layer names, positions, visibility, opacity, and blend mode keys for imported layers.
- Crops the stroke raster into a top `Paint` layer so freehand work survives PSD round-trips even though strokes are not stored as vector-editable objects.

### `ImageImport` (`App/imageimport.*`)

- Accepts local file paths or URLs from QML and keeps the image import decision in one place.
- Passes already-supported raster formats through unchanged.
- Decodes PSD composite image data directly in C++ for 8-bit and 16-bit grayscale, indexed, RGB, and CMYK documents when the merged image uses raw or RLE compression.
- Extracts PSD document metadata during import, including dimensions, channel/depth information, color mode, compression mode, alpha presence, and decoded layer-count hints.
- Parses PSD layer records and layer channel image data for raw and RLE-compressed layers, then writes each decoded layer into a cache PNG with per-layer metadata such as name, bounds, opacity, visibility, and blend mode key.
- Writes decoded PSD results into cache PNGs so the existing `Image`-based canvas workflow can treat both composite fallbacks and individual PSD layers like ordinary imported images.

## Data Flow & Interaction Summary

1. The C++ entry point loads `Vincent.Main` and hands off control to QML.
2. `Main.qml` instantiates `PainterCanvasPage`, which binds its LVRS view ID to the shared `CanvasDocumentViewModel`.
3. `CanvasToolBar` surfaces user actions. Signals propagate up to `PainterCanvasPage`, which either updates view-model properties directly or invokes `DrawingSurface` commands.
4. `DrawingSurface` tracks transient stroke rendering data and encodes it into the Qt Quick `Canvas`. Brush parameters flow from the document view model to the surface, `BrushEngine` converts stylus pressure into the final per-sample stroke width and opacity, and `CanvasBackend` handles snapshot/history and transform calculations.
5. File dialog selections bubble from `CanvasToolBar` to `PainterCanvasPage`, which forwards them to `DrawingSurface`; `ImageImport` either forwards the original raster URL, returns multiple positioned PSD layers, or falls back to a rasterized PSD composite before the image data is added to `CanvasDocumentViewModel`, while `ImageExport` handles `.psd` saves in the opposite direction.
6. `LayerListModel` feeds both the visual image repeater and the layer sidebar, keeping imported layer metadata in one shared model.

## Notable Platform Considerations

- Craft integration: both CMake and `main.cpp` account for Craft-managed prefixes so that packaged QML plugins resolve without manual configuration.
- macOS OpenGL: The CMake logic conditionally adds shim targets and explicit frameworks to satisfy Qt Quick's OpenGL requirements on modern SDKs.

## Testing Surface

- `tests/tst_brushengine.cpp` validates the pressure-to-size and pressure-to-opacity curves, sample smoothing, append heuristics, and stamp-density calculations of the brush engine with Qt Test.
- `tests/tst_canvasbackend.cpp` validates deep snapshot cloning, undo/redo history semantics, fit placement calculations, and free-transform rectangle resolution.
- `tests/tst_canvasdocumentviewmodel.cpp` validates default MVVM document state, selection-derived properties, layer export/import with metadata, layer movement, and layer-removal fallback behavior.
- `tests/tst_imageexport.cpp` validates PSD export for both transparent canvases and layered round-trips that preserve imported layers, Unicode names, blend keys, and the rasterized paint overlay.
- `tests/tst_imageimport.cpp` validates PSD support detection, passthrough import for standard rasters, PSD metadata extraction, PSD composite decoding for both raw and RLE-compressed fixtures, and multi-layer PSD extraction into separate cached images.
- Run the suite with `ctest --test-dir build --output-on-failure` after configuring with `-DBUILD_TESTING=ON`.
