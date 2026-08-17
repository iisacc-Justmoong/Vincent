# Design QA: 2px white color-picker border

## Comparison target

- Source visual truth: `/var/folders/j3/5qyn3r610nsfxzwk8_q8w9gm0000gn/T/codex-clipboard-1c734e3e-84ed-42cc-8366-176a144f21f2.png`, refined by the user's explicit requirement that the current-color circle have a 2px white border so black remains visible.
- Component: the current-color `LV.IconButton` at the far right of `CanvasToolBar.qml`.
- Required states:
  - Black current color, picker closed, to verify minimum-contrast visibility.
  - Magenta current color, picker closed, to compare against the supplied visual.
  - Black current color, picker open, to verify the primary interaction.

## Evidence and normalization

- Source visual:
  - Path: `/var/folders/j3/5qyn3r610nsfxzwk8_q8w9gm0000gn/T/codex-clipboard-1c734e3e-84ed-42cc-8366-176a144f21f2.png`
  - Pixel size: 57 x 83 at 1x.
- Pre-fix black implementation:
  - Path: `/Users/ymy/.codex/visualizations/2026/08/17/01a00d9d-b955-71b3-b20c-e14c2686105e/vincent-color-picker-borderless-full.png`
  - Logical viewport: 1280 x 853.
  - Pixel size: 2560 x 1706 at 2x Retina.
- Post-fix black implementation:
  - Path: `/Users/ymy/.codex/visualizations/2026/08/17/01a00d9d-b955-71b3-b20c-e14c2686105e/vincent-color-picker-white-border-black-full.png`
  - Logical viewport: 1280 x 853.
  - Pixel size: 2560 x 1706 at 2x Retina.
- Post-fix magenta implementation:
  - Path: `/Users/ymy/.codex/visualizations/2026/08/17/01a00d9d-b955-71b3-b20c-e14c2686105e/vincent-color-picker-white-border-magenta-full.png`
  - Logical viewport: 1280 x 853.
  - Pixel size: 2560 x 1706 at 2x Retina.
- Picker-open implementation:
  - Path: `/Users/ymy/.codex/visualizations/2026/08/17/01a00d9d-b955-71b3-b20c-e14c2686105e/vincent-color-picker-white-border-black-picker-open.png`
- Density-normalized focused captures:
  - Black before: `/Users/ymy/.codex/visualizations/2026/08/17/01a00d9d-b955-71b3-b20c-e14c2686105e/vincent-color-picker-borderless-black-normalized.png`
  - Black after: `/Users/ymy/.codex/visualizations/2026/08/17/01a00d9d-b955-71b3-b20c-e14c2686105e/vincent-color-picker-white-border-black-normalized.png`
  - Magenta after: `/Users/ymy/.codex/visualizations/2026/08/17/01a00d9d-b955-71b3-b20c-e14c2686105e/vincent-color-picker-white-border-magenta-normalized.png`
  - Each focused capture was downsampled from the same 2x Retina crop to 57 x 83 at 1x.
- Combined comparison inputs:
  - Black visibility, left before and right after: `/Users/ymy/.codex/visualizations/2026/08/17/01a00d9d-b955-71b3-b20c-e14c2686105e/color-picker-black-before-vs-white-border-after.png`
  - Supplied source, left, and magenta implementation, right: `/Users/ymy/.codex/visualizations/2026/08/17/01a00d9d-b955-71b3-b20c-e14c2686105e/color-picker-source-vs-white-border-implementation.png`
  - Both inputs place equal 57 x 83 crops together before 4x inspection scaling.

## Findings

- No actionable P0, P1, or P2 finding remains.
- The black current-color fill is now visibly bounded by the requested white ring against the dark toolbar.
- The control keeps one color circle and one visibility border. The previous outer button frame and decorative ring do not return.
- The white border is specified as `LV.Theme.scaleMetric(2)` and resolves to exactly 2 logical pixels in the verified desktop profile.

## Required fidelity surfaces

- Fonts and typography: passed; this component has no text content, and adjacent toolbar typography is unchanged.
- Spacing and layout rhythm: passed; the stock 22 x 22 LVRS button frame and toolbar alignment are unchanged, while the border is drawn inward inside the stock icon slot.
- Colors and visual tokens: passed; fill remains bound to `toolbar.currentColor`, and the visibility border is opaque `#ffffff` as requested. Black and magenta states were both observed.
- Image quality and asset fidelity: passed; the dynamic color indicator remains an antialiased QML control surface at native Retina density, with no raster scaling artifact or replacement icon asset.
- Copy and content: passed; the `Brush color` accessible name and tooltip are unchanged.

## Interaction and accessibility

- Activating the exposed `Brush color` AX button opens the HSL triangle picker.
- HSL selection updates the toolbar indicator immediately while preserving the 2px white border.
- Escape closes the picker, and the stock LVRS `Borderless` hover, press, focus, and hit-target behavior remains available.

## Comparison history

1. The earlier magenta-only pass removed every outline and passed its focused comparison.
2. The newly required black-state comparison exposed a P1 affordance issue: the borderless black circle nearly disappeared into the dark toolbar.
3. The current-color circle received an inward 2px opaque white border without restoring the removed outer frame or decorative ring.
4. The post-fix black comparison shows a clearly visible circle, and the post-fix magenta/source comparison confirms the simplified single-ring structure. No P0/P1/P2 issue remains.

## Implementation checklist

- [x] Preserve stock `LV.IconButton` geometry and `Borderless` tone.
- [x] Preserve `toolbar.currentColor` fill.
- [x] Add a 2px `#ffffff` border to the color circle only.
- [x] Keep the outer frame and decorative ring removed.
- [x] Preserve tooltip, accessibility name, and HSL picker interaction.
- [x] Verify black and non-black states in the running 1280 x 853 app.

## Toolbar horizontal padding follow-up

- The full-width toolbar background and bottom separator remain flush with the 1280-pixel window.
- The toolbar content layout uses `LV.Theme.gap16` for both `anchors.leftMargin` and `anchors.rightMargin`; no stock LVRS button dimension is overridden.
- In the rebuilt running app, the window frame was `100,80,1280,853`, the 22 x 22 `New canvas` button frame began at `116,115`, and the 22 x 22 `Brush color` button frame began at `1342,115`. The resulting left and right edge gaps are both exactly 16 logical pixels.
- Closed-state edge evidence: `/Users/ymy/.codex/visualizations/2026/08/17/01a00d9d-b955-71b3-b20c-e14c2686105e/vincent-toolbar-horizontal-padding-16-edges.png`.
- Open-state interaction evidence: `/Users/ymy/.codex/visualizations/2026/08/17/01a00d9d-b955-71b3-b20c-e14c2686105e/vincent-toolbar-horizontal-padding-16-color-picker-open-crop.png`; the picker remains visible and aligned within the window after the right inset.

final result: passed
