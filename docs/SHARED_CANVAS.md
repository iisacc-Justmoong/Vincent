# iiSharedCanvas integration

Vincent's `DrawingSurfaceItem` uses `iiSharedCanvas::CanvasItem` as its native
canvas base. A document ViewModel connection creates one selected transparent
raster layer, and the existing LVRS QML painting surface binds its brush,
pressure curve, stabilizer, tool, input-state, and undo/redo properties to that
item.

The new-canvas modal also exposes an LVRS `CheckBox` for an infinite canvas.
That path creates iiSharedCanvas 1.1 `ChunkedRasterAsset` documents with a
256-pixel chunk size and the entered width/height as the small initial allocated
region. Pan and zoom camera movement maps the fixed viewport into world
coordinates and requests any newly visible region from
`ensureInfiniteCanvasRegion()`. Growth is rounded outwards to chunk boundaries;
only painted chunks store pixels.

The application discovers the installed `iiSharedCanvas` CMake package from
`$HOME/.local/iiSharedCanvas`. The package is linked alongside the current
installed iiPaintEngine; the removed legacy `CanvasAdapter` is no longer part of
the Vincent source or build contract.

The integration test exercises these application-level gates:

- existing Vincent brush, eraser, fill, text, shape, raster import, PNG, PSD,
  thumbnail, input-pressure, and undo/redo workflows;
- one `DrawingSurfaceItem` rendering static raster, static native vector, and a
  hold-keyframed raster layer together;
- raster text, shape, and fill operations mutating only the selected raster
  asset, including inverse mapping through its affine layer transform, without
  flattening visible vector or sibling layers into that asset;
- ordinary raster open replacing the prior mixed document even when both
  documents have the same extent;
- canonical `.iisc` save, validated decode, reopen, and raster export;
- infinite-canvas creation, signed world-origin growth, camera anchoring,
  sparse brush allocation, added-raster-layer synchronization, and native 1.1
  save/reopen;
- a fresh CMake configure selecting the installed iiPaintEngine and
  iiSharedCanvas packages rather than the former build-local legacy prefix.

`DrawingSurfaceItem::saveToFile()` and `openRaster()` accept `.iisc` for the
native document owned by the item. Writes use `QSaveFile`; reads pass through
the iiSharedCanvas checksum, allocation-limit, canonical-form, and document
validation gates before replacing the current document.

The rendered mixed frame is the display and flat-export boundary only. Raster
authoring starts from `CanvasItem::selectedRasterPixels()`, maps document-space
coordinates through the selected layer's inverse affine transform, and commits
with `replaceSelectedPixels()`. This keeps native vector, sibling raster, and
other-frame content separate while editing. Opening an ordinary bitmap is an
explicit new-document operation and therefore replaces, rather than partially
mutates, any previously opened mixed document.

When an infinite region grows left or above its prior origin, QML shifts
session objects by the reported margin and expands every added raster-layer
item to the same origin and extent. It adjusts the center-origin pan offset by
the asymmetric growth, so a world point keeps its screen position during the
structural resize. Existing finite-canvas behavior remains unchanged.
Full-canvas raster-layer delegates bind their visual and pointer geometry
directly to the base canvas surface rather than cached session dimensions, so
every visibly allocated pixel remains drawable during asynchronous model
updates and recent-session restoration.

The application open dialog includes `.iisc`, so a mixed raster/vector/timeline
document can be selected and displayed by the Vincent canvas. The Save As
dialog deliberately does not advertise `.iisc` yet: Vincent's existing QML
session-layer stack must first be mapped into native assets so a normal layered
editing session cannot appear to save while losing content.

The existing `DrawingSurface.qml` still owns its image, text, shape, and extra
raster-layer delegates as product-session objects. They continue to work and
remain covered by the existing workflow tests, but they are not silently
omitted or flattened into `.iisc`: the composite save overload fails closed
when such objects are present. Mapping those session objects to native
iiSharedCanvas assets is a separate product migration, not a prerequisite for
using the mixed canvas base.

Recent canvas persistence is deliberately a separate application-session
boundary rather than pretending those QML objects are already native `.iisc`
assets. `recent-canvas.vrc` wraps the validated iiSharedCanvas document plus
PNG bytes for inserted images and additional raster layers and JSON-safe
text/shape/object metadata. It is limited to one owner-only file below
`QStandardPaths::AppLocalDataLocation/canvas`, uses a versioned header and
SHA-256 payload check, and replaces the previous snapshot through `QSaveFile`.
The native snapshot extent is normalized to the live visual canvas size before
encoding. The page schedules it 1.2 seconds after the latest session mutation
and flushes a pending edit during normal window close. Decode and every embedded
image are validated before the current native document is replaced. Valid PNGs
are extracted only to one owner-only temporary directory held by the restored
surface and are removed with that surface, so the `.vrc` remains the sole
persistent recent-session artifact.

Version 0.1 rerenders synchronously after edits. Large-document memory budgets,
partial repaint/decode, background rendering, and cross-platform package
validation remain product-hardening gates.
