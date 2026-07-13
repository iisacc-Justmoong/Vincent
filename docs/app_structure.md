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
- `App/models/painting/drawingsurfaceitem.*` adapts Vincent's QML surface contract to `CanvasAdapter` from iiPaintEngine and re-emits the raster/input signals Vincent consumes across the DLL boundary.
- `App/qml/` contains the LVRS UI for the main window, toolbar, and drawing surface.
- `tests/` contains Qt Test targets for the document view model, iiPaintEngine drawing surface integration, and Windows, macOS, and Linux build/package contracts.

## Build System Overview

1. The root `CMakeLists.txt` sets up Qt 6, LVRS, iiPaintEngine, `psd_sdk`, install paths, packaging metadata, and the `Vincent` executable target.
2. `App/CMakeLists.txt` attaches the C++ sources and headers to the `Vincent` target.
3. `qt_add_qml_module` registers the `Vincent` QML module and exposes `Main.qml`, `PainterCanvasPage.qml`, `CanvasToolBar.qml`, `HslTriangleColorPicker.qml`, and `DrawingSurface.qml`.
4. The executable links against Qt Core, QML, Quick, Quick Controls 2, SVG, and `iiPaintEngine::iiPaintEngine`, then LVRS configures runtime QML import handling.
5. On Windows, the `Vincent` target uses the GUI subsystem so it starts without a console window; its lifetime remains owned by the `QGuiApplication` event loop and top-level window. Its compiled resources provide the icon, file/product version, `asInvoker` manifest, Windows 10/11 compatibility, Per-Monitor V2 DPI awareness, and long-path awareness.
6. A Windows post-build step copies LVRS, iiPaintEngine, and the runtime DLLs next to the selected MinGW compiler into `build/` so direct build-tree launches resolve a consistent toolchain. Release packaging keeps LVRS QML embedded in the LVRS binary and removes any duplicate loose `qml/LVRS` deployment tree.
7. Linux installs use the GNU `bin/lib/qml/plugins/share` layout, a `Terminal=false` desktop entry, relative ELF RPATH, and Qt's generated QML deployment script; macOS remains an app bundle and Windows remains a GUI-subsystem executable with a flat staged runtime.
8. When `BUILD_TESTING=ON`, `tests/CMakeLists.txt` registers the active unit test targets, including Windows executable resource and PE contract checks on Windows plus static macOS and Linux packaging contracts on every host.

## Runtime Entry Point (`App/main.cpp`)

- Starts a standard `QGuiApplication`/`QQmlApplicationEngine` entry point so Windows links only against LVRS's exported QML module surface instead of non-exported LVRS C++ runtime helpers.
- Calls LVRS's exported `qml_register_types_LVRS()` registration function before loading QML so LVRS-provided types such as `WindowSafeAreaObserver` and the `ViewModels` singleton are available without the full LVRS app bootstrap.
- Keeps the packaged `qml/` directory authoritative, accepts an explicit `LVRS_HOST_PREFIX` only when a bundled LVRS module is absent, and does not scan user-home `.local/LVRS` fallbacks during a packaged launch.
- Bounds the completed hidden root window once to the selected screen's available geometry, then shows it synchronously before entering the event loop. C++ never resizes the visible window, so startup has one final compositor-safe geometry instead of post-display corrections.
- Avoids normal startup log I/O. Setting `VINCENT_STARTUP_TRACE=1` enables timestamped milestone records in `%TEMP%\Vincent-startup.log`; a fatal root-QML object creation failure is always recorded and flushed.
- Registers `DrawingSurfaceItem` as the QML canvas item.
- Registers `PaletteUtils` as a QML context helper.
- Registers a shared `CanvasDocumentViewModel` in the LVRS `ViewModels` registry under `CanvasDocument` through the registry's `QObject` meta-object API.
- Launches the `Vincent` QML module's `Main` component.

## QML Module Layout (`App/qml/`)

### `Main.qml`

- Creates the main LVRS application window.
- Establishes a hidden 1400x880 preferred launch geometry before C++ applies the one-time available-screen bound and shows the window. The asynchronous canvas loader does not change that top-level size, while normal user resizing, maximize, and fullscreen controls remain available afterward.
- Keeps native close, minimize, and maximize controls while using LVRS solid chrome to suppress the visual title-bar strip.
- Uses native Windows and Linux title bars without LVRS's redundant logical drag handle. macOS alone reserves the LVRS drag surface for its full-size-content title bar in normal mode, and disables that surface in full screen so every platform avoids a redundant strip above the toolbar.
- Uses CMake platform packaging metadata so `resources/Appicon.icns` becomes the canonical macOS bundle icon and `resources/Appicon.ico` becomes the Windows executable icon resource. The macOS App Store/Xcode asset catalog is regenerated from the same `.icns` through `tools/sync_app_icon_assets.sh`, and the CMake bundle build removes the legacy `Contents/Resources/icon.icns` file from incremental bundles. Windows resources additionally embed native manifest and version metadata. Windows packaging is driven by `build-windows.ps1`, which keeps the build tree fixed at `build/`, builds in parallel, requires tests for signed releases, deploys only the English and Korean Qt translations, copies LVRS/iiPaintEngine runtime DLLs, removes duplicate loose LVRS QML and unused plugins, strips Vincent-owned MinGW Release binaries, validates PE resources plus full staged import closure and runtime compatibility, then performs the last binary mutation before Authenticode signing. Public mode binds Vincent-owned binaries to the exact selected Windows certificate-store thumbprint, preserves valid vendor identities, requires SHA-256 file digests, an RFC 3161 SHA-256 timestamp, and successful Authenticode-policy verification for every staged PE before ZIP creation; a WiX MSI is signed and verified only after it is fully generated under a partial name. Only selected partials are cleared before packaging; requested ZIP/MSI files and sidecar hashes replace their last-known-good versions as one verified, rollback-capable set. A prepared/committed transaction journal records the original presence of each pair, restores prior generations or removes first-publication remnants after interruption, and is deleted only after the complete promoted set has been revalidated. Explicitly unsigned local smoke packages carry an `-unsigned` suffix, while the unverified generic CPack diagnostic carries `-unsigned-cpack-incomplete`.
- Authors an ICE105-validated Windows Installer 5 dual-context WiX MSI with `ALLUSERS=2` and `MSIINSTALLPERUSER=1`, preserving current-user 4.0.0 upgrade continuity while offering current-user or elevated all-users scope through the native advanced UI. A required HKMU installation-context marker records scope and install location before related-product detection, preventing a later upgrade from crossing contexts; `WIX_UPGRADE_DETECTED` also locks a markerless per-user upgrade to current-user scope. The ProductCode is deterministic for the same version and architecture and changes when either input changes, preventing a same-version rebuild from registering a duplicate product. Ambiguous dual registrations block new or cross-context installation while maintenance and removal remain available for recovery. Its required core-runtime feature owns an executable-backed advertised Start Menu entry that Windows Installer materializes as a normal shortcut in the context-redirected Program Menu; this avoids fixed-scope shortcut components and their ICE38/ICE43/ICE57 conflicts. The same UI exposes the all-users destination and embeds an RTF rendering generated from the root GNU AGPL `LICENSE`. Once the product identity is registered, the same package follows Windows Installer's standard Change/Repair/Remove maintenance path.
- Builds Microsoft Store and local-development MSIX packages through the separate `build-windows-store.ps1` orchestration boundary. Both reuse the tested flat Windows runtime stage and generate exact 50/44/150 px Store assets from the canonical 1024 px icon. The x64 manifest targets `Windows.Desktop` 10.0.19041.0, declares `packagedClassicApp`, `mediumIL`, and the restricted `runFullTrust` capability, and keeps version 4.0.3 as `4.0.3.0`. Development mode requires an exact-subject self-signed Code Signing certificate and can install, activate through the package application ID, observe a visible window, and remove the package. Store mode accepts only the three exact Partner Center Product identity values, complete iiPaintEngine/corresponding-source legal evidence, and a current tested Release stage; it emits an intentionally unsigned one-package `.msixupload` for Microsoft certification and re-signing rather than treating a private certificate as public trust.
- Provides a Qt Quick Controls menu bar on the LVRS application window. Qt 6.8 promotes it to the native global menu on macOS; Windows and Linux render the LVRS-token background, text, hover state, spacing, and 22-DIP compact height in-window. File routes to the existing new/open/save/clear flows, Edit routes to undo/redo, layer creation/deletion, tool and shape selection, and brush-size changes, Window routes to canvas fit/reset plus native window controls, and Help exposes the current keyboard shortcut reference.
- Owns named shortcut contracts for every actionable menu item. File uses platform Command/Ctrl N/O/S, Command/Ctrl+Shift+K, and Command/Ctrl+Q; Edit uses platform undo/redo, Command/Ctrl+Shift+N, Command/Ctrl+Shift+Delete, B/E/H/V/Z/U/G/T for tools, Command/Ctrl+Alt+1..7 for shape kinds, and [/] for brush size; Window uses Command/Ctrl+0, Command/Ctrl+1, Command/Ctrl+M, and platform fullscreen. `Controls.Action` is the sole owner of each portable shortcut, while the canvas retains only non-duplicating two-beolsik alternatives and the loaded page disables editing commands during canvas text entry, layer renaming, or a modal file dialog.
- Passes the active macOS normal-window drag-handle height into `PainterCanvasPage`; it resolves to zero on Windows, Linux, and macOS full screen so the toolbar begins directly under the rendered menu/content edge.
- Activates an asynchronous `Loader` through `Qt.callLater` after the shell window completes its first construction turn, so the native window can appear before the heavier `PainterCanvasPage` object tree is incubated. A lightweight LVRS loading label remains visible until the page is ready.
- Hosts the loaded `PainterCanvasPage` as the single content view and assigns it to `canvasPage` only after the page emits `pageReady`.

### `PainterCanvasPage.qml`

- Binds the view to LVRS `ViewModels` using the stable `PainterCanvasPage` view ID.
- Reads brush and canvas state from `CanvasDocumentViewModel`.
- Places the toolbar directly below the menu bar/content edge on Windows, Linux, and macOS full screen because the reserved top chrome height resolves to zero there; macOS normal-window mode reserves only the full-size title-bar drag surface.
- Hosts a docked left-side `LV.Hierarchy` layer panel directly under the toolbar and flush with the window's left and bottom edges. The panel lists current QML session layers top-to-bottom plus the `Background` raster row when present, leaves the list area empty when the stack has no layers, displays each row through LVRS `iconSource` bitmap thumbnails instead of letter glyphs, activates layer selection, opens inline layer-name editing from repeated row activation/double-click, delegates row drag moves back to `DrawingSurface`, and exposes plus/minus footer buttons for creating or removing layers, including the background layer. Blank documents start with `Background` plus a selected transparent `Layer 1` so drawing can begin without a manual layer-add step.
- Routes toolbar actions into either view-model property updates or `DrawingSurface` commands.

### `CanvasToolBar.qml`

- Provides the flat-raster command bar.
- Exposes left toolbar file actions for new, open, and save, while the top-level application Actions own their shortcuts and expose clear through the menu.
- Renders the command bar inside a full-width square-corner toolbar background with a bottom separator instead of a floating cylinder.
- Uses compact 18px tool buttons with toolbar-frame and button x-axis padding set to half the y-axis padding, while the current-color picker alone uses right-side spacing set to 4x that x-axis padding; reduced auxiliary control sizes keep the toolbar reading as a dense application command bar.
- Renders the left command cluster inside the existing toolbar frame using LVRS `addFile`, `generalopen`, `generalsave`, app-local `panHand`, `translateObject`, `generalsearch`, `showCode`, `eraser`, selected shape-kind, `fillbucket`, and `typeAlias` icons.
- Keeps existing behavior on the matching actionable icons: add opens the new-canvas size modal, open shows the image-open dialog, save shows the image-save dialog, `panHand` selects the pan tool, `translateObject` selects the move tool, `generalsearch` selects the zoom tool, pencil selects or reopens brush settings, eraser selects the eraser, the shape split button selects the shape tool from its body and opens the shape menu from its chevron, `fillbucket` selects the fill tool, and `typeAlias` selects the text tool.
- Uses an LVRS-styled modal with width and height inputs before emitting the new-canvas request.
- Offers shape menu entries for rectangle, ellipse, triangle, diamond, star, rectangle bubble, and ellipse bubble insertion through `LV.ContextMenu`, shows each shape icon in the menu, and mirrors the selected shape on the split button icon.
- Uses app-local vector sources for the `panHand`, `translateObject`, and `typeAlias` slots while preserving the Figma/LVRS icon names where available. `panHand` covers the hand-pan tool missing from the LVRS icon set, `translateObject` avoids the bundled embedded-image SVG warning, and `typeAlias` matches the Figma `typeAlias / Theme=Light` metadata instead of LVRS's different default icon shape.
- Keeps pan input on the fixed canvas viewport and applies pan movement with direct item `x`/`y` offsets instead of anchor center offsets, avoiding anchor relayout during every drag frame.
- Restricts tools to brush, eraser, pan, move, zoom, shape, fill, and text.
- Assigns `B/E/H/V/Z/U/G/T` through the top-level application Actions and leaves only the matching two-beolsik Korean alternatives on the canvas, avoiding ambiguous duplicate `QShortcut` registrations. Holding `Space` temporarily routes canvas input through pan without changing the selected tool, except while the text editor is actively receiving text.
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
- Lets iiPaintEngine handle mouse, tablet, live preview, stroke commit, eraser, undo, redo, and raster save behavior inside each active raster surface. Every flat image opening replaces the base raster canvas at the source image dimensions so no workspace padding is baked into the document and the image is never scaled to the previous canvas.
- Presents a drag-to-insert shape tool when shape mode is active; the QML preview follows the drag bounds, Shift constrains the drag bounds to a 1:1 ratio, then stores the selected shape as a transformable solid-color session object and separate layer row using the current brush color. Speech-bubble tails are traced as part of the same path as the body, so rectangle and ellipse bubbles do not attach a separate triangular subshape.
- Presents a fill tool between shape and text; clicking the canvas flood-fills the contiguous same-color raster region with the current brush color through `DrawingSurfaceItem`.
- Presents a paint-style text editor when the text tool is active; the editor sizes its frame from the longest text line within the remaining canvas width, uses the current brush size and brush color as its text size and color, then stores plain text as a transformable session object and separate layer row.
- Presents a move tool for inserted image, text, and shape layer objects; clicking an object selects it, dragging the body moves it, Shift-dragging the body locks movement to the dominant straight axis, dragging enlarged corner or edge handles resizes its bounds with matching resize cursors, Shift-resizing preserves the object's original aspect ratio, transforms may extend outside the canvas like Photoshop layers, object pixels and transform chrome remain clipped to the canvas visual scope, and Delete or Backspace removes the selected object before export. Move cursor resolution is pointer-local: empty canvas uses the arrow, object bodies use four-way movement, and directional resize cursors are limited to the selected object's active handle hit targets.
- Exposes `layerHierarchyRows`, `addEmptyLayer`, `addDefaultDrawingLayer`, `activateLayerByKey`, `deleteLayerByKey`, `renameLayerByKey`, and `applyLayerHierarchyOrder` so the LVRS hierarchy panel can add, select, rename, delete, and reorder the current layer stack without bypassing the canvas state owner. Initial app load and blank `newCanvas`/`clearCanvas` flows call the default-layer helper to create a selected transparent `Layer 1` above `Background`.
- Supplies bitmap thumbnail URLs for hierarchy rows through `iconSource`; transformable objects are rasterized into small PNG thumbnails, while base/raster layers prefer an asynchronous 32px `grabToImage` preview cached as a small file and fall back to the C++ full-raster PNG cache only when grabbing is unavailable. Raster thumbnail refreshes are debounced and coalesced after `DrawingSurfaceItem` reports raster content changes so stroke input does not synchronously encode layer previews, and removed raster layers clear pending refreshes and thumbnail URLs.
- Backs added raster layers with their own transparent `DrawingSurfaceItem`, routes fill and active-layer undo/redo to the selected raster surface, snapshots layer surfaces across QML delegate rebuilds, and removes the layer pixels when the layer is deleted.
- Keeps `drawableObjects` as the document/session stack while rendering through an incremental `drawableObjectVisualModel`, so appending many layers creates only the new delegate instead of resetting every existing raster layer surface and forcing PNG snapshot restore churn.
- Runs brush live-preview work at a 16 ms frame interval for both the base canvas and added raster layers, keeping raw input accumulation immediate while avoiding iiPaintEngine preview jobs above the visible frame cadence.
- Shows brush and eraser footprint feedback as two `QtQuick.Shapes` vector paths centered on the pointer. Their path diameter is `brushSize * canvasZoomScale` logical screen pixels, real-valued strokes thin proportionally below six DIP, Qt applies monitor DPI, and a standard crosshair remains available only when the exact footprint is subpixel. Pointer coordinates use `mapToItem`, and the high-z outline renders in the unclipped viewport so the full circle stays above paint at canvas edges. A non-blocking `HoverHandler` handles idle movement while a passive `PointHandler` uses `Qt.NoButton` to preserve authentic mouse/tablet input through synthetic mouse events without taking iiPaintEngine's exclusive grab.
- Keeps zoom-tool drag input on a viewport-sized `MouseArea` so horizontal zoom drags work from empty workspace around the canvas as well as from the canvas itself. Horizontal drag remains the zoom gesture, but its cursor is the precision crosshair rather than the unrelated horizontal-resize cursor.
- Uses a proportional workspace inset to create initial and cleared canvases below the toolbar with visible dark margins instead of filling the window.
- Creates new canvases from explicit width and height values passed by the toolbar modal, clamped to the supported raster dimension range, then fits the initial zoom down only when the requested canvas is larger than the workspace viewport.
- Keeps an already-created canvas static; later window or view-model canvas dimension changes do not resize it.
- Presents only the fixed canvas area on a white paper background while `Background` exists, switches that area to a tiled PNG-style transparency grid after `Background` is removed, and leaves any resized viewport overflow in the LVRS workspace color while stacking the base raster canvas, raster layer surfaces, and transformable object overlays in hierarchy order under the clipped canvas surface.
- Keeps QML responsible for viewport placement, wheel focus handling, keyboard shortcuts, temporary Space-pan input override, and toolbar state binding.
- Resolves all canvas cursors through one effective-tool function: brush and eraser use a blank system cursor underneath their size-accurate circular outline, with a crosshair fallback below one logical pixel; zoom and shape use the precision crosshair; fill uses a pointing hand; text uses an I-beam; pan uses open/closed hands; and move resolves arrow, four-way movement, or directional resize from the current pointer position. Tool changes and temporary Space-pan entry clear cached transform-handle hover state so a stale resize cursor cannot leak into another tool, and Space-pan immediately replaces the brush outline with the hand cursor.
- Exposes a PSD compatibility manifest helper that commits pending text/shape placement and asks `DrawingSurfaceItem` to map the raster canvas plus current session objects into Photoshop-style layer records.

## Core C++ Components

### `DrawingSurfaceItem`

- Inherits `CanvasAdapter` from iiPaintEngine.
- Preserves Vincent's previous QML-facing commands such as `newCanvas`, `openRaster`, `saveToFile`, `undo`, `redo`, and compatibility stroke methods.
- Exposes `imageObjectForFile` for PSD preview/object compatibility, while the primary `openRaster` path keeps each flat image at its source pixel dimensions when replacing the base raster canvas.
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

1. `main.cpp` launches the Qt/LVRS-backed QML application and registers the shared view model plus helper services.
2. `PainterCanvasPage` binds to `CanvasDocumentViewModel` and passes brush state into `DrawingSurface`.
3. `CanvasToolBar` emits user actions for file flow, tool selection, shape selection, HSL color picker changes, brush size updates, and brush reselection settings.
4. `DrawingSurface` hosts `DrawingSurfaceItem`; the item delegates raster operations to iiPaintEngine's `CanvasAdapter`.
5. `PainterCanvasPage` hosts `LV.Hierarchy` on the left and binds it to `DrawingSurface.layerHierarchyRows`; rows use cached layer bitmap thumbnails via `iconSource`, blank documents begin with a selected transparent `Layer 1`, activation selects the matching session object, repeated activation/double-click opens an inline `TextInput` for renaming, footer add creates another transparent raster layer, row drag rewrites `drawableObjects` order, and footer delete removes the selected layer, including `Background`.
6. iiPaintEngine owns stroke rasterization, live preview, commit, eraser compositing, undo/redo snapshots, raster open, and raster save for each `DrawingSurfaceItem`; Vincent keeps the base raster canvas plus added raster layers as separate surfaces, renders session image/text/shape objects as overlays, routes fill replacements to the selected raster surface, and composites the stack during save.
7. When PSD work needs document structure, `PsdCompatibilityDocument` converts the same optional base raster canvas and session objects into a PSD-style layer manifest; `.psd` save passes the base raster composited over Vincent's white canvas paper while `Background` exists, rasterized layer/object images, and that manifest metadata to `PsdImageWriter`.
8. When opening PSD, `PsdImageReader` reads XMP metadata, merged image data, and importable 8-bit RGB/RGBA raster layers through `psd_sdk`; QML restores layered PSDs as Vincent raster layers and falls back to a cached merged PNG preview for flat PSD image objects.

## Testing Surface

- `tests/tst_canvasdocumentviewmodel.cpp` validates the flat-raster document state and value clamping, including iiPaintEngine brush settings, supported tool modes, and supported shape kinds.
- `tests/tst_psdcompatibilitydocument.cpp` validates the PSD compatibility manifest, including canvas metadata, optional raster base layer creation, session object layer mapping, PSD bounds, opacity, visibility, and PSD canvas-size limits.
- `tests/tst_canvastoolbarqmlcontract.cpp` validates QML toolbar and page contracts, including the new-canvas size modal, default blank-layer creation, brush reselection settings, pan-tool selection, move-tool selection, zoom-tool selection, shape split-menu selection, fill-tool selection, text-tool selection, centralized effective-tool cursor resolution, the vector dual-ring brush/eraser cursor with passive tablet-safe pointer tracking and zoom-compensated real strokes, horizontal-resize cursor isolation to transform handles, object-transform and keyboard-delete hooks, enlarged transform handles with resize cursors, transform bounds that are not clamped to the canvas, clipped canvas-surface rendering, the left `LV.Hierarchy` layer panel with live raster layer surfaces, deletable background-layer state, transparency-grid background display, bitmap thumbnail row icons, inline rename editing and plus/minus footer buttons, and HSL triangle color-picker usage.
- `tests/tst_mainqmlcontract.cpp` validates the LVRS application-window chrome contract, including native controls, platform/fullscreen drag-region selection, the native-macOS versus compact in-window menu contract, and single-owner shortcut registration.
- `tests/tst_linuxbuildworkflowcontract.cpp` validates Linux relative RPATH, generated Qt/QML runtime deployment, root-relative TGZ layout, desktop entry, and documentation contracts.
- `tests/tst_windowsbinarycontract.cpp` launches the real executable and samples its native outer and client sizes for three seconds, ensuring asynchronous startup produces no visible size transition.
- `tests/tst_drawingsurfaceitem.cpp` validates the Vincent-to-iiPaintEngine adapter path for drawing, erasing, fill, text and shape raster commit, effective-tool cursor mapping, native-window brush/eraser cursor diameter and hotspot tracking during pressed movement, context-sensitive move/body/handle cursors, stale resize-hover cleanup, transformable image/text/shape object movement/resizing/deletion including enlarged handle hit targets, Shift-constrained straight movement and aspect-locked resizing, clipped visual scope, and bounds that can exceed the canvas, shape/text tool creation as separate layer rows, initial blank-layer creation, hierarchy raster-layer creation without transform hit testing, raster-layer deletion removing its pixels from composite output, background-layer deletion, transparency-grid visibility, Background omission from PSD output, many-layer creation without snapshot churn, hierarchy-layer renaming, hierarchy-layer row projection and reordering, hand-tool canvas panning, viewport-wide horizontal-drag canvas zooming, composite object saving including layered PSD output with metadata, PSD merged-preview and layer import through `psd_sdk`, undo/redo, saving, repeated flat-image canvas replacement at source resolution, explicit-size new canvas creation, and workspace-inset canvas creation. Its native-pointer case requires a genuinely exposed window for synthesized mouse input and frame capture, and allows the macOS compositor up to 15 seconds to expose that fixture after a build.
- Run the suite with `ctest --test-dir build --output-on-failure` after configuring with `-DBUILD_TESTING=ON`.
