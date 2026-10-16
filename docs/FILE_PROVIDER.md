# File creation ownership in Vincent

Archived profile and drawing/cache writers delegate to provider devices. This migration preserves archived product status and does not publish or reactivate the app.

The common [ownership contract](../../../../SDK/iiFileProvider/docs/FILE_OWNERSHIP.md) defines stream, staging, control-file and
installed-package rules. Source regression tests remain with this module; the
workspace creation audit also checks these boundaries. Build/deployment and target
runtime validation are separate evidence.

Changed boundaries:


- `src/App/models/painting/drawingsurfaceitem.cpp`
- `src/App/models/painting/drawingsurfaceitem.h`
- `src/App/models/preferences/applicationpreferences.cpp`
- `src/App/models/profile/profileimageprocessor.cpp`
- `src/App/models/profile/profileimageprocessor.h`
