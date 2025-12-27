# Vincent 2.0 Packaging Guide

This document captures the end-to-end steps needed to turn the `Vincent` build tree into an App Store–compliant macOS package for Vincent 2.0, plus the Linux TGZ package. It assumes you have the Qt dependencies referenced in the repository README installed.

## 1. Prerequisites
- Apple Developer Program membership with access to App Store Connect.
- Certificates downloaded in Keychain Access:
  - **Apple Distribution** (or legacy *3rd Party Mac Developer Application*).
  - **Apple Installer** (or legacy *3rd Party Mac Developer Installer*).
- A macOS App Store provisioning profile that matches your bundle identifier.
- Xcode command-line tools (`xcode-select --install`) and Transporter from the Mac App Store.
- Qt toolchain (Core, Qml, Quick, QuickControls2, Svg) available in your `PATH` so that `macdeployqt` is callable.

## 2. Configure the Release Build
```bash
cmake -S . -B build-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0
cmake --build build-release --target Vincent
```
If you need a different bundle identifier, update `BUNDLE_ID` in `CMakeLists.txt` before configuring. Adjust the deployment target if you need to support newer or older macOS releases.

## 3. Stage the App Bundle
The built app lives under `build-release/"Vincent 2.0.app"`. Copy it to a staging directory (for example, `dist/"Vincent 2.0.app"`) so you can safely run deployment tools without touching your build tree.

## 4. Embed Qt Frameworks
Run `macdeployqt` in App Store mode to embed the required Qt frameworks and QML plugins:
```bash
macdeployqt "dist/Vincent 2.0.app" \
  -appstore-compliant \
  -qmldir=App/qml \
  -always-overwrite
```
Verify that all `.framework` bundles now sit inside `dist/"Vincent 2.0.app"/Contents/Frameworks` and that `qt.conf` exists in `Contents/Resources/`.

## 5. Prepare Metadata
Update the generated `Info.plist` (inside `dist/"Vincent 2.0.app"/Contents/`) with:
- `CFBundleIdentifier` matching your bundle ID.
- `CFBundleVersion` and `CFBundleShortVersionString` set to a semantic version number you are shipping.
- Any usage description strings your app requires (e.g., `NSMicrophoneUsageDescription`)—Vincent 2.0 currently relies only on file picker access.

## 6. Sandbox Entitlements
Customize `packaging/macos/Vincent.entitlements` if the app needs additional capabilities. The default template enables the App Sandbox and grants read/write access to user-selected files and picture libraries. Keep entitlements minimal to improve App Review approval chances.

## 7. Codesign the Bundle
```bash
codesign --force --options runtime \
  --entitlements packaging/macos/Vincent.entitlements \
  --sign "Apple Distribution: MUYEONG YUN (5U49ST9XZH)" \
  "dist/Vincent 2.0.app"
```
Then validate the signature:
```bash
codesign --verify --deep --strict "dist/Vincent 2.0.app"
spctl --assess --type execute "dist/Vincent 2.0.app"
```
If `spctl` warns about missing the hardened runtime, make sure `--options runtime` was passed.

## 8. Create the Installer Package
```bash
productbuild \
  --component "dist/Vincent 2.0.app" /Applications \
  --sign "Apple Installer: MUYEONG YUN (5U49ST9XZH)" \
  "dist/Vincent-2.0.0.pkg"
```
This produces the installer payload required by App Store Connect. Keep the `.pkg` under 4 GB.

## 9. Upload to App Store Connect
1. Open Transporter.
2. Drag `dist/Vincent-2.0.0.pkg` into the queue.
3. Provide your App Store Connect credentials and upload.
4. Resolve any validation issues that Transporter reports (missing icons, entitlement mismatches, etc.).

## 10. Post-Upload Checklist
- Create an App Store Connect record with screenshots, localized descriptions, and pricing.
- Attach the uploaded build to a new version submission and complete the export compliance questionnaire.
- Submit for review.

## Troubleshooting Tips
- Use `otool -L "dist/Vincent 2.0.app"/Contents/MacOS/"Vincent 2.0"` to ensure no absolute paths to your build tree remain.
- Leverage `plutil -p` to inspect `Info.plist` after `macdeployqt` runs.
- If Transporter rejects the upload due to missing `LC_VERSION_MIN_MACOSX`, make sure `CMAKE_OSX_DEPLOYMENT_TARGET` is set at configure time.
- Should you require notarization for outside-the-store distribution, rerun codesigning with the same entitlements and submit via `xcrun notarytool`; App Store submissions do not need separate notarization.

## Linux Build and Packaging
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --build build --target package
```
The TGZ archive is written to the build directory as `Vincent-<version>-Linux.tar.gz`. If you prefer an installed tree, run `cmake --install build --prefix <path>`.
