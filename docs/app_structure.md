# Vincent 2.2.1 Application Structure

This document captures the current architecture of Vincent 2.2.1 as observed in the repository. It describes how the project is laid out, how the build system is wired, and how the runtime pieces cooperate to deliver the painting experience.

## Top-Level Layout

- `CMakeLists.txt` (root) – bootstraps the Qt build, configures install paths, and delegates to the application sources in `App/`.
- `App/` – contains all C++ and QML code for the application bundle.
- `App/brushengine.h`, `App/brushengine.cpp` – pressure-aware brush dynamics engine exposed to QML.
- `App/imageimport.h`, `App/imageimport.cpp` – image import service that passes standard rasters through and decodes PSD composites into cacheable PNGs for QML.
- `App/paletteutils.h`, `App/paletteutils.cpp` – palette ordering helper exposed to QML as a singleton.
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
- Registers the `ImageImport` context object used by QML to normalize raster imports and decode PSD files before they reach the canvas.
- Registers the `PaletteUtils` singleton in the `Vincent` QML module for palette computation.
- Configures a `QQmlApplicationEngine` and augments its import paths when `CRAFTROOT` exposes prebuilt QML modules.
- Connects `objectCreationFailed` to `QCoreApplication::exit(-1)` for fail-fast behavior if the QML scene cannot load.
- Loads the `Vincent` QML module's `Main` component and starts the event loop.

All UI and interaction logic lives in QML, with palette calculation delegated to the C++ helper.

## QML Module Layout (`App/qml/`)

### `Main.qml`

- Declares a `Controls.ApplicationWindow` with fixed initial dimensions and title.
- Stores a reference to the active canvas page for cross-component coordination.
- Instantiates `PainterCanvasPage` as the lone content item and exposes the page instance via the `pageReady` signal.

### `PainterCanvasPage.qml`

- Extends `Controls.Page` to host the main drawing surface.
- Maintains the user-facing state (`brushColor`, `brushSize`, `toolMode`, and color `palette`).
- Computes the default palette by calling `PaletteUtils.buildDefaultPalette` instead of running JavaScript sorting logic.
- Emits `pageReady` when the component loads to let `Main.qml` grab a pointer.
- Provides imperative helpers (`newCanvas`, `clearCanvas`, `saveCanvasAs`, `openImage`, `adjustBrush`) that wrap the lower-level `DrawingSurface` API.
- Instantiates the `CanvasToolBar` as the page header and wires its signals back into the page state.
- Hosts the `DrawingSurface` inside a `Rectangle`, forwarding brush parameters and listening for scroll-wheel-driven size changes.

### `CanvasToolBar.qml`

- Implements the horizontal toolbar purely with Qt Quick Controls.
- Defines a local `ToolbarButton` component so every button shares the same icon slot, spacing, and padding rules.
- Exposes signals for high-level actions (new, open, save, clear) and tool adjustments (brush size, tool selection, palette picks).
- Provides `Dialogs.FileDialog` instances for open/save flows, including PSD-aware open filters and extension inference when a filename lacks a suffix.
- Presents quick-access buttons, tool toggles, a size slider with increment/decrement buttons, and a color palette repeater that highlights the active swatch.

### `DrawingSurface.qml`

- Renders the actual canvas within a rounded `Rectangle`.
- Manages drawing state (`strokes`, `currentStroke`, imported images, and undo/redo snapshots) and tool behavior.
- Uses `PointHandler` for stylus and mouse stroke capture, enabling pressure-sensitive brush sampling by default when a pen device reports pressure.
- Stores each stroke as a sequence of sampled points that carry normalized pressure, resolved brush diameter, and resolved opacity.
- Uses a `Canvas` element to batch-render all strokes; variable-width strokes are rasterized by interpolated stamp placement rather than fixed-width `lineTo()` segments, and each stamp carries its own alpha.
- Supports eraser mode through pressure-sensitive `destination-out` compositing strength, wheel-based brush size adjustments via the `brushDeltaRequested` signal, and optional background image loading.
- Routes all opened or dropped images through `ImageImport`, so PSD documents are decoded into flattened raster layers before the existing image-placement workflow runs.
- Normalizes file URLs for loading and saving, ensuring compatibility with both `file://` URIs and bare paths.

### `BrushEngine` (`App/brushengine.*`)

- Centralizes the brush-response policy so pressure handling is not duplicated in QML.
- Resolves raw stylus pressure into usable size and opacity curves, clamps minimum visible stroke size, smooths sudden width and alpha jumps, and decides when a new sample is significant enough to append.
- Provides segment stamp density hints so the QML renderer can fill gaps while preserving variable stroke width and opacity.

### `ImageImport` (`App/imageimport.*`)

- Accepts local file paths or URLs from QML and keeps the image import decision in one place.
- Passes already-supported raster formats through unchanged.
- Decodes PSD composite image data directly in C++ for 8-bit and 16-bit grayscale, indexed, RGB, and CMYK documents when the merged image uses raw or RLE compression.
- Writes decoded PSD results into a cache PNG so the existing `Image`-based canvas workflow can treat them like ordinary imported images.

## Data Flow & Interaction Summary

1. The C++ entry point loads `Vincent.Main` and hands off control to QML.
2. `Main.qml` instantiates `PainterCanvasPage`, which centralizes application state and owns the drawing surface.
3. `PainterCanvasPage` requests the ordered color palette from the `PaletteUtils` singleton.
4. `CanvasToolBar` surfaces user actions. Signals propagate up to `PainterCanvasPage` methods, which then mutate page state or invoke `DrawingSurface` methods.
5. `DrawingSurface` tracks strokes and encodes them into the Qt Quick `Canvas`. Brush parameters flow from the page to the surface, and `BrushEngine` converts stylus pressure into the final per-sample stroke width and opacity.
6. File dialog selections bubble from `CanvasToolBar` to `PainterCanvasPage`, which forwards them to `DrawingSurface`; `ImageImport` either forwards the original raster URL or rasterizes PSD into a cache PNG before the image is added to the canvas.

## Notable Platform Considerations

- Craft integration: both CMake and `main.cpp` account for Craft-managed prefixes so that packaged QML plugins resolve without manual configuration.
- macOS OpenGL: The CMake logic conditionally adds shim targets and explicit frameworks to satisfy Qt Quick's OpenGL requirements on modern SDKs.

## Testing Surface

- `tests/tst_brushengine.cpp` validates the pressure-to-size and pressure-to-opacity curves, sample smoothing, append heuristics, and stamp-density calculations of the brush engine with Qt Test.
- `tests/tst_imageimport.cpp` validates PSD support detection, passthrough import for standard rasters, and PSD composite decoding for both raw and RLE-compressed fixtures.
- Run the suite with `ctest --test-dir build --output-on-failure` after configuring with `-DBUILD_TESTING=ON`.
