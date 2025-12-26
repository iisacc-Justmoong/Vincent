# Vincent Application Structure

This document captures the current architecture of Vincent as observed in the repository. It describes how the project is laid out, how the build system is wired, and how the runtime pieces cooperate to deliver the painting experience.

## Top-Level Layout

- `CMakeLists.txt` (root) – bootstraps the Qt build, configures install paths, and delegates to the application sources in `App/`.
- `App/` – contains all C++ and QML code for the application bundle.
- `App/paletteutils.h`, `App/paletteutils.cpp` – palette ordering helper exposed to QML as a singleton.
- `resources/` – SVG icons and design assets consumed by the QML UI.
- `build/`, `cmake-build-debug/` – out-of-source build trees (ignored in project description, but important to keep generated artifacts isolated).

No other product source directories are present at this time.

## Build System Overview

The project relies on CMake and Qt 6 modules.

1. The root `CMakeLists.txt` ensures Craft-provided prefixes take priority when `CRAFTROOT` is set. It configures GNU install paths before adding the `App/` subdirectory.
2. `App/CMakeLists.txt` ties the sources to the `Vincent` target while the root file links against the Qt stack (`Qt6::Core`, `Qt6::Qml`, `Qt6::Quick`, `Qt6::QuickControls2`, `Qt6::Svg`).
3. A single executable target, `Vincent`, is defined around `App/main.cpp`.
4. `qt_add_qml_module` registers the `Vincent` QML module version 1.0, exposing the components under `App/qml/` to the QML engine at runtime.
5. macOS-specific blocks adjust OpenGL discovery so Qt Quick works even when SDK headers are missing from the default search paths.
6. The executable links privately against the Qt targets and is installed via standard GNU install dir settings.
7. CPack is configured for macOS productbuild packaging and a Linux TGZ package.

## Runtime Entry Point (`App/main.cpp`)

- Creates the `QGuiApplication` instance that hosts the Qt Quick scene graph.
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
- Provides `Dialogs.FileDialog` instances for open/save flows, including extension inference when a filename lacks a suffix.
- Presents quick-access buttons, tool toggles, a size slider with increment/decrement buttons, and a color palette repeater that highlights the active swatch.

### `DrawingSurface.qml`

- Renders the actual canvas within a rounded `Rectangle`.
- Manages drawing state (`strokes`, `currentStroke`, `backgroundSource`) and tool behavior.
- Uses a `Canvas` element to batch-render all strokes; each stroke contains point arrays, colors, and widths.
- Supports eraser mode by substituting white strokes, wheel-based brush size adjustments via the `brushDeltaRequested` signal, and optional background image loading.
- Normalizes file URLs for loading and saving, ensuring compatibility with both `file://` URIs and bare paths.

## Data Flow & Interaction Summary

1. The C++ entry point loads `Vincent.Main` and hands off control to QML.
2. `Main.qml` instantiates `PainterCanvasPage`, which centralizes application state and owns the drawing surface.
3. `PainterCanvasPage` requests the ordered color palette from the `PaletteUtils` singleton.
4. `CanvasToolBar` surfaces user actions. Signals propagate up to `PainterCanvasPage` methods, which then mutate page state or invoke `DrawingSurface` methods.
5. `DrawingSurface` tracks strokes and encodes them into the Qt Quick `Canvas`. Brush parameters flow from the page to the surface, ensuring interactive updates.
6. File dialog selections bubble from `CanvasToolBar` to `PainterCanvasPage`, which forwards them to `DrawingSurface` for persistence or background loading.

## Notable Platform Considerations

- Craft integration: both CMake and `main.cpp` account for Craft-managed prefixes so that packaged QML plugins resolve without manual configuration.
- macOS OpenGL: The CMake logic conditionally adds shim targets and explicit frameworks to satisfy Qt Quick's OpenGL requirements on modern SDKs.

## Opportunities for Extension

- Extend the C++ back-end (e.g., document models, command stacks) if the painting logic outgrows the QML-only approach.
- Add automated tests under `tests/` once non-trivial logic (such as file handling) gains more edge cases.
- Expand documentation with user-facing guides (tool descriptions, keyboard shortcuts) alongside this structural overview.
