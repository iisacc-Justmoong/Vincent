# Vincent 4.0 Application Structure

This document captures the flat-raster architecture of Vincent 4.0 after replacing the local painting implementation with iiPaintEngine.

## Top-Level Layout

- `CMakeLists.txt` (root) bootstraps Qt, LVRS, iiPaintEngine, packaging, and the `App/` subdirectory.
- `App/` contains the application bundle sources.
- `App/models/canvas/canvasdocumentviewmodel.*` stores LVRS-facing document state for brush color, brush size, active tool, and canvas dimensions.
- `App/models/canvas/canvasviewmodelbridge.*` gates drawing mutations through the LVRS document view model and synchronizes canvas metadata.
- `App/models/brush/paletteutils.*` provides palette ordering helpers exposed to QML.
- `App/models/document/psdcompatibilitydocument.*` defines the internal Photoshop-style document/layer manifest used as the boundary for PSD import/export.
- `App/models/document/psdimagereader.*` wraps `psd_sdk` so Vincent can read PSD merged image data without relying on Qt image plugins.
- `App/models/document/psdimagewriter.*` wraps `psd_sdk` so Vincent can write layered PSD files with XMP metadata.
- `App/models/painting/drawingsurfaceitem.*` adapts Vincent's QML surface contract to `CanvasAdapter` from iiPaintEngine.
- `App/qml/` contains the LVRS UI for the main window, toolbar, and drawing surface.
- `tests/` contains Qt Test targets for the document view model and iiPaintEngine drawing surface integration.

## Build System Overview

1. The root `CMakeLists.txt` sets up Qt 6, LVRS, iiPaintEngine, `psd_sdk`, install paths, packaging metadata, and the `Vincent` executable target.
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
- Keeps LVRS's logical top drag handle enabled while solid chrome suppresses the visible titlebar strip.
- Uses CMake platform packaging metadata so `resources/Appicon.icns` becomes the canonical macOS bundle icon and `resources/Appicon.ico` becomes the Windows executable icon resource. The macOS App Store/Xcode asset catalog is regenerated from the same `.icns` through `tools/sync_app_icon_assets.sh`, and the CMake bundle build removes the legacy `Contents/Resources/icon.icns` file from incremental bundles. Windows packaging is driven by `build-windows.ps1`, which keeps the build tree fixed at `build/`, runs `windeployqt`, copies LVRS/iiPaintEngine runtime DLLs plus LVRS QML imports, and stages `dist/Vincent-Windows`.
- Provides a Qt Quick Controls menu bar on the LVRS application window. File routes to the existing new/open/save/clear flows, Edit routes to undo/redo, layer creation/deletion, tool and shape selection, and brush-size changes, Window routes to canvas fit/reset plus native window controls, and Help exposes the current keyboard shortcut reference.
- Owns named shortcut contracts for every actionable menu item. File uses platform Command/Ctrl N/O/S, Command/Ctrl+Shift+K, and Command/Ctrl+Q; Edit uses platform undo/redo, Command/Ctrl+Shift+N, Command/Ctrl+Shift+Delete, B/E/H/V/Z/U/G/T for tools, Command/Ctrl+Alt+1..7 for shape kinds, and [/] for brush size; Window uses Command/Ctrl+0, Command/Ctrl+1, Command/Ctrl+M, and platform fullscreen. Actionable menu entries are backed by `Controls.Action` objects so Qt owns shortcut registration and menu items bind through their supported `action` property.
- Passes the active top drag-handle height into `PainterCanvasPage` so the titlebar-position area remains empty, shares the toolbar background color, and the toolbar starts below it.
- Hosts `PainterCanvasPage` as the single content view.

### `PainterCanvasPage.qml`

- Binds the view to LVRS `ViewModels` using the stable `PainterCanvasPage` view ID.
- Reads brush and canvas state from `CanvasDocumentViewModel`.
- Places the toolbar below the reserved top chrome height, paints the reserved area and toolbar from the same toolbar background color, and keeps the visible titlebar suppressed.
- Hosts a docked left-side `LV.Hierarchy` layer panel directly under the toolbar and flush with the window's left and bottom edges. The panel lists current QML session layers top-to-bottom plus the `Background` raster row when present, leaves the list area empty when the stack has no layers, displays each row through LVRS `iconSource` bitmap thumbnails instead of letter glyphs, activates layer selection, opens inline layer-name editing from repeated row activation/double-click, delegates row drag moves back to `DrawingSurface`, and exposes plus/minus footer buttons for creating or removing layers, including the background layer. Blank documents start with `Background` plus a selected transparent `Layer 1` so drawing can begin without a manual layer-add step.
- Routes toolbar actions into either view-model property updates or `DrawingSurface` commands.

### `CanvasToolBar.qml`

- Provides the flat-raster command bar.
- Exposes left toolbar file actions for new, open, and save, with clear still available through the keyboard shortcut.
- Renders the command bar inside a full-width square-corner toolbar background with a bottom separator instead of a floating cylinder.
- Uses compact 18px tool buttons with toolbar-frame and button x-axis padding set to half the y-axis padding, while the current-color picker alone uses right-side spacing set to 4x that x-axis padding; reduced auxiliary control sizes keep the toolbar reading as a dense application command bar.
- Renders the left command cluster inside the existing toolbar frame using LVRS `addFile`, `generalopen`, `generalsave`, app-local `panHand`, `translateObject`, `generalsearch`, `showCode`, `eraser`, selected shape-kind, `fillbucket`, and `typeAlias` icons.
- Keeps existing behavior on the matching actionable icons: add opens the new-canvas size modal, open shows the image-open dialog, save shows the image-save dialog, `panHand` selects the pan tool, `translateObject` selects the move tool, `generalsearch` selects the zoom tool, pencil selects or reopens brush settings, eraser selects the eraser, the shape split button selects the shape tool from its body and opens the shape menu from its chevron, `fillbucket` selects the fill tool, and `typeAlias` selects the text tool.
- Uses an LVRS-styled modal with width and height inputs before emitting the new-canvas request.
- Offers shape menu entries for rectangle, ellipse, triangle, diamond, star, rectangle bubble, and ellipse bubble insertion through `LV.ContextMenu`, shows each shape icon in the menu, and mirrors the selected shape on the split button icon.
- Uses app-local vector sources for the `panHand`, `translateObject`, and `typeAlias` slots while preserving the Figma/LVRS icon names where available. `panHand` covers the hand-pan tool missing from the LVRS icon set, `translateObject` avoids the bundled embedded-image SVG warning, and `typeAlias` matches the Figma `typeAlias / Theme=Light` metadata instead of LVRS's different default icon shape.
- Keeps pan input on the fixed canvas viewport and applies pan movement with direct item `x`/`y` offsets instead of anchor center offsets, avoiding anchor relayout during every drag frame.
- Restricts tools to brush, eraser, pan, move, zoom, shape, fill, and text.
- Assigns paint-editor tool shortcuts on the canvas surface: `B` brush, `E` eraser, `H` hand-pan, `V` move, `Z` zoom, `U` shape, `G` fill, and `T` text. The same physical key positions also work under two-beolsik Korean input.
- Opens an HSL triangle color wheel from a current-color swatch button instead of presenting enumerated palette swatches.
- Orders brush size controls as decrease button, slider, and increase button so the controls follow the value direction.
- Opens a brush settings menu when the already-selected brush tool is pressed again, with sliders for iiPaintEngine brush size, flow, opacity, hardness, spacing, and stabilizer strength, plus a pressure-opacity toggle that controls whether tablet pressure scales the brush opacity cap. The pressure minimum, center, and maximum parameters sit at the bottom of the menu as a three-point curve graph instead of separate sliders.
- Restricts open dialogs to supported Qt image inputs plus PSD files and lets the save dialog default to Photoshop PSD while still offering PNG, JPEG, BMP, WebP, and TIFF, with default extension handling per selected filter.

### `HslTriangleColorPicker.qml`

- Draws a hue ring and an inner HSL triangle with QML `Canvas`.
- Fills the inner triangle from the currently selected hue, with pure hue, white, and black as the three vertices.
- Maps triangle points to pure-hue, white, and black weights, then derives brush color through HSL saturation and lightness.
- Emits selected colors directly to the toolbar without depending on a predefined palette list.

### `DrawingSurface.qml`

- Hosts `DrawingSurfaceItem` as the editable raster surface.
- Lets iiPaintEngine handle mouse, tablet, live preview, stroke commit, eraser, undo, redo, and raster save behavior inside each active raster surface. The first flat image opening replaces the base raster canvas at the source image dimensions so no workspace padding is baked into the document; later flat image opens in that image document append transformable image object layers without recreating the canvas.
- Presents a drag-to-insert shape tool when shape mode is active; the QML preview follows the drag bounds, Shift constrains the drag bounds to a 1:1 ratio, then stores the selected shape as a transformable solid-color session object and separate layer row using the current brush color. Speech-bubble tails are traced as part of the same path as the body, so rectangle and ellipse bubbles do not attach a separate triangular subshape.
- Presents a fill tool between shape and text; clicking the canvas flood-fills the contiguous same-color raster region with the current brush color through `DrawingSurfaceItem`.
- Presents a paint-style text editor when the text tool is active; the editor sizes its frame from the longest text line within the remaining canvas width, uses the current brush size and brush color as its text size and color, then stores plain text as a transformable session object and separate layer row.
- Presents a move tool for inserted image, text, and shape layer objects; clicking an object selects it, dragging the body moves it, dragging a corner handle resizes its bounds, transforms may extend outside the canvas like Photoshop layers, and Delete or Backspace removes the selected object before export.
- Exposes `layerHierarchyRows`, `addEmptyLayer`, `addDefaultDrawingLayer`, `activateLayerByKey`, `deleteLayerByKey`, `renameLayerByKey`, and `applyLayerHierarchyOrder` so the LVRS hierarchy panel can add, select, rename, delete, and reorder the current layer stack without bypassing the canvas state owner. Initial app load and blank `newCanvas`/`clearCanvas` flows call the default-layer helper to create a selected transparent `Layer 1` above `Background`.
- Supplies bitmap thumbnail URLs for hierarchy rows through `iconSource`; transformable objects are rasterized into small PNG thumbnails, while base/raster layers prefer an asynchronous 32px `grabToImage` preview cached as a small file and fall back to the C++ full-raster PNG cache only when grabbing is unavailable. Raster thumbnail refreshes are debounced and coalesced after `DrawingSurfaceItem` reports raster content changes so stroke input does not synchronously encode layer previews, and removed raster layers clear pending refreshes and thumbnail URLs.
- Backs added raster layers with their own transparent `DrawingSurfaceItem`, routes fill and active-layer undo/redo to the selected raster surface, snapshots layer surfaces across QML delegate rebuilds, and removes the layer pixels when the layer is deleted.
- Keeps `drawableObjects` as the document/session stack while rendering through an incremental `drawableObjectVisualModel`, so appending many layers creates only the new delegate instead of resetting every existing raster layer surface and forcing PNG snapshot restore churn.
- Runs brush live-preview work at a 16 ms frame interval for both the base canvas and added raster layers, keeping raw input accumulation immediate while avoiding iiPaintEngine preview jobs above the visible frame cadence.
- Uses a proportional workspace inset to create initial and cleared canvases below the toolbar with visible dark margins instead of filling the window.
- Creates new canvases from explicit width and height values passed by the toolbar modal, clamped to the supported raster dimension range, then fits the initial zoom down only when the requested canvas is larger than the workspace viewport.
- Keeps an already-created canvas static; later window or view-model canvas dimension changes do not resize it.
- Presents only the fixed canvas area on a white paper background while `Background` exists, switches that area to a tiled PNG-style transparency grid after `Background` is removed, and leaves any resized viewport overflow in the LVRS workspace color while stacking the base raster canvas, raster layer surfaces, and transformable object overlays in hierarchy order.
- Keeps QML responsible for viewport placement, wheel focus handling, keyboard shortcuts, and toolbar state binding.
- Exposes a PSD compatibility manifest helper that commits pending text/shape placement and asks `DrawingSurfaceItem` to map the raster canvas plus current session objects into Photoshop-style layer records.

## Core C++ Components

### `DrawingSurfaceItem`

- Inherits `CanvasAdapter` from iiPaintEngine.
- Preserves Vincent's previous QML-facing commands such as `newCanvas`, `openRaster`, `saveToFile`, `undo`, `redo`, and compatibility stroke methods.
- Exposes `imageObjectForFile` for PSD preview/object compatibility and repeated flat-image insertion, while the primary `openRaster` path keeps the first flat image at its source pixel dimensions when creating the base raster canvas.
- Exposes `commitText` so QML can pass text bounds, content, text size, and text color into Qt text layout and commit the result back through iiPaintEngine's raster replacement path.
- Exposes `commitShape` so QML can pass shape bounds, selected shape kind, and fill color into Qt painter paths and commit the solid-color result back through iiPaintEngine's raster replacement path.
- Exposes `fillAt` so QML can pass a canvas point and brush color into an exact-color flood fill that commits back through iiPaintEngine's raster replacement path.
- Exposes `saveToFileWithObjectsAndRasterLayers` so QML can save the current base raster canvas plus live raster layer surfaces and transformable image, text, and shape session objects without flattening those objects into the live raster state.
- Writes `.psd` paths as 8-bit RGB Photoshop documents with a merged preview, the opaque white `Background` canvas layer when it still exists, one rasterized layer per current raster/image/text/shape object, and Vincent XMP metadata for the compatibility manifest, preserving the bottom-to-top layer record order that Photoshop displays with `Background` at the bottom when present.
- Exposes `psdCompatibilityManifest` so QML can retrieve a Photoshop-style manifest for the current raster canvas and transformable session objects.
- Routes `.psd` imports through `PsdImageReader`; importable layered PSD documents become Vincent raster layers via cached full-canvas PNG snapshots, while flat PSD image-object imports still receive a cached PNG preview source and retain the original PSD source metadata.
- Synchronizes brush state, tool mode, and canvas dimensions with `CanvasDocumentViewModel` through `CanvasViewModelBridge`.
- Applies QML-driven canvas surface size updates atomically so startup resizing cannot leave partial 1-pixel dimensions in the document model.
- Exposes `backgroundSource` and `hasBackground` for the current flat raster document metadata.
- Emits `rasterContentChanged` when raster pixels change, wiring brush strokes through iiPaintEngine's post-commit `strokeCountChanged` signal, and exposes thumbnail cache helpers used by the hierarchy panel for `Background`, added raster layers, and rasterized object rows. The QML hierarchy layer batches raster thumbnail work behind an idle timer, uses `grabToImage` to cache a 32px preview file, and invokes full-raster PNG generation only as a fallback.

### `CanvasDocumentViewModel`

- Exposes palette, brush color, brush size, iiPaintEngine brush settings, active tool, selected shape kind, and canvas dimensions to QML.
- Sets the default brush hardness to the app's maximum anti-aliased edge setting for iiPaintEngine's coverage-based circular brush.
- Clamps brush size, brush dynamics, pressure curve, pressure-opacity enablement, stabilizer, and canvas dimensions to safe ranges.
- Restricts tool mode to the flat-raster tool set: brush, eraser, pan, move, zoom, shape, fill, and text.

### `PsdCompatibilityDocument`

- Provides the internal PSD compatibility boundary over the app's raster/session stack.
- Stores an RGB, 8-bit document manifest with Photoshop-style top/left/bottom/right layer bounds, Photoshop blend mode keys, byte opacity, visibility, and per-layer payload.
- Creates a bottom `Background` layer when the canvas still has one and maps current raster, image, text, and shape session layers into ordered layer records above it.
- Clamps layer bounds to the current canvas and flags canvases larger than PSD's 30,000 px edge limit as not PSD-compatible.

### `PsdImageReader`

- Uses the BSD-2-Clause `psd_sdk` parser to read Photoshop PSD files directly inside Vincent.
- Converts 8-bit RGB/RGBA merged image data into `QImage`.
- Parses image resources for XMP metadata and restores Vincent's base64 JSON layer manifest when present.
- Parses the layer mask section, extracts importable 8-bit RGB/RGBA raster layers, and exposes bottom-to-top layer images plus bounds, opacity, visibility, blend-mode keys, and mask-presence flags to QML.

### `PsdImageWriter`

- Uses the BSD-2-Clause `psd_sdk` exporter to write Photoshop PSD files directly inside Vincent.
- Converts Vincent's bottom-to-top raster/session object stack into PSD layers without reversing that record order, writing the base raster canvas over Vincent's white paper color as the bottom opaque `Background` layer only while that layer exists and current raster, image, text, and shape layers as transparent-capable layers above it.
- Adds XMP metadata keys for Vincent compatibility version, layer count, and a base64-encoded JSON copy of the PSD compatibility manifest so object bounds, source/text/shape payloads, and layer ordering survive PSD export.

### `CanvasViewModelBridge`

- Resolves the active LVRS document view model.
- Blocks canvas mutation until the expected document/view binding is available.
- Keeps the model's canvas size, tool state, and iiPaintEngine brush config aligned with the rendered surface.

## Data Flow Summary

1. `main.cpp` launches the LVRS-backed QML application and registers the shared view model plus helper services.
2. `PainterCanvasPage` binds to `CanvasDocumentViewModel` and passes brush state into `DrawingSurface`.
3. `CanvasToolBar` emits user actions for file flow, tool selection, shape selection, HSL color picker changes, brush size updates, and brush reselection settings.
4. `DrawingSurface` hosts `DrawingSurfaceItem`; the item delegates raster operations to iiPaintEngine's `CanvasAdapter`.
5. `PainterCanvasPage` hosts `LV.Hierarchy` on the left and binds it to `DrawingSurface.layerHierarchyRows`; rows use cached layer bitmap thumbnails via `iconSource`, blank documents begin with a selected transparent `Layer 1`, activation selects the matching session object, repeated activation/double-click opens an inline `TextInput` for renaming, footer add creates another transparent raster layer, row drag rewrites `drawableObjects` order, and footer delete removes the selected layer, including `Background`.
6. iiPaintEngine owns stroke rasterization, live preview, commit, eraser compositing, undo/redo snapshots, raster open, and raster save for each `DrawingSurfaceItem`; Vincent keeps the base raster canvas plus added raster layers as separate surfaces, renders opened images/text/shapes as overlays, routes fill replacements to the selected raster surface, and composites the stack during save.
7. When PSD work needs document structure, `PsdCompatibilityDocument` converts the same optional base raster canvas and session objects into a PSD-style layer manifest; `.psd` save passes the base raster composited over Vincent's white canvas paper while `Background` exists, rasterized layer/object images, and that manifest metadata to `PsdImageWriter`.
8. When opening PSD, `PsdImageReader` reads XMP metadata, merged image data, and importable 8-bit RGB/RGBA raster layers through `psd_sdk`; QML restores layered PSDs as Vincent raster layers and falls back to a cached merged PNG preview for flat PSD image objects.

## Testing Surface

- `tests/tst_canvasdocumentviewmodel.cpp` validates the flat-raster document state and value clamping, including iiPaintEngine brush settings, supported tool modes, and supported shape kinds.
- `tests/tst_psdcompatibilitydocument.cpp` validates the PSD compatibility manifest, including canvas metadata, optional raster base layer creation, session object layer mapping, PSD bounds, opacity, visibility, and PSD canvas-size limits.
- `tests/tst_canvastoolbarqmlcontract.cpp` validates QML toolbar and page contracts, including the new-canvas size modal, default blank-layer creation, brush reselection settings, pan-tool selection, move-tool selection, zoom-tool selection, shape split-menu selection, fill-tool selection, text-tool selection, object-transform and keyboard-delete hooks, transform bounds that are not clamped to the canvas, the left `LV.Hierarchy` layer panel with live raster layer surfaces, deletable background-layer state, transparency-grid background display, bitmap thumbnail row icons, inline rename editing and plus/minus footer buttons, and HSL triangle color-picker usage.
- `tests/tst_mainqmlcontract.cpp` validates the LVRS application-window chrome contract, including native controls, the logical top drag handle, and the populated top-level application menu bar.
- `tests/tst_drawingsurfaceitem.cpp` validates the Vincent-to-iiPaintEngine adapter path for drawing, erasing, fill, text and shape raster commit, transformable image/text/shape object movement/resizing/deletion including bounds that can exceed the canvas, shape/text tool creation as separate layer rows, initial blank-layer creation, hierarchy raster-layer creation without transform hit testing, raster-layer deletion removing its pixels from composite output, background-layer deletion, transparency-grid visibility, Background omission from PSD output, many-layer creation without snapshot churn, hierarchy-layer renaming, hierarchy-layer row projection and reordering, hand-tool canvas panning, horizontal-drag canvas zooming, composite object saving including layered PSD output with metadata, PSD merged-preview and layer import through `psd_sdk`, undo/redo, saving, first-image flat raster canvas creation, repeated-image object insertion, explicit-size new canvas creation, and workspace-inset canvas creation.
- Run the suite with `ctest --test-dir build --output-on-failure` after configuring with `-DBUILD_TESTING=ON`.
