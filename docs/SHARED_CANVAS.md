# iiSharedCanvas integration

Vincent's `DrawingSurfaceItem` uses `iiSharedCanvas::CanvasItem` as its native
canvas base. A document ViewModel connection creates one selected transparent
raster layer, and the existing LVRS QML painting surface binds its brush,
pressure curve, stabilizer, tool, input-state, and undo/redo properties to that
item.

The application discovers the installed `iiSharedCanvas` CMake package from
`$HOME/.local/iiSharedCanvas`. The package is linked alongside the current
installed iiPaintEngine; the removed legacy `CanvasAdapter` is no longer part of
the Vincent source or build contract.

The integration test exercises these application-level gates:

- existing Vincent brush, eraser, fill, text, shape, raster import, PNG, PSD,
  thumbnail, input-pressure, and undo/redo workflows;
- one `DrawingSurfaceItem` rendering static raster, static native vector, and a
  hold-keyframed raster layer together;
- canonical `.iisc` save, validated decode, reopen, and raster export;
- a fresh CMake configure selecting the installed iiPaintEngine and
  iiSharedCanvas packages rather than the former build-local legacy prefix.

`DrawingSurfaceItem::saveToFile()` and `openRaster()` accept `.iisc` for the
native document owned by the item. Writes use `QSaveFile`; reads pass through
the iiSharedCanvas checksum, allocation-limit, canonical-form, and document
validation gates before replacing the current document.

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

Version 0.1 rerenders synchronously after edits. Large-document memory budgets,
partial repaint/decode, background rendering, autosave, and cross-platform
package validation remain product-hardening gates.
