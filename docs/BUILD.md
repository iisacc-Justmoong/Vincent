# Vincent 4.0 Packaging Guide

This document captures the end-to-end steps needed to turn the `Vincent` build tree into an App Store–compliant macOS package for Vincent 4.0, a Windows runtime package, plus the Linux TGZ package. It assumes you have the Qt dependencies referenced in the repository README installed.

## 1. Prerequisites
- Apple Developer Program membership with access to App Store Connect.
- Certificates downloaded in Keychain Access:
  - **Apple Distribution** (or legacy *3rd Party Mac Developer Application*).
  - **Apple Installer** (or legacy *3rd Party Mac Developer Installer*).
- A macOS App Store provisioning profile that matches your bundle identifier.
- Xcode command-line tools (`xcode-select --install`) and Transporter from the Mac App Store.
- Qt toolchain (Core, Qml, Quick, QuickControls2, Svg) available in your `PATH` so that `macdeployqt` is callable.
- iiPaintEngine installed under `$HOME/.local/iiPaintEngine` or available through `CMAKE_PREFIX_PATH` as `iiPaintEngine::iiPaintEngine`.

## 1a. Automated Local Build Script
Use `./build.sh` or `./build.sh local` for local development validation. The default local mode configures `build/`, builds Vincent, runs `ctest --test-dir build --output-on-failure`, deploys Qt runtime files into `dist/Vincent.app`, signs that app with `LOCAL_APP_CERT`, the first valid `Apple Development` identity, or an ad-hoc signature if no development certificate is installed, then regenerates unsigned local installer packages at `dist/Vincent.pkg` and `dist/Vincent-appstore.pkg`.

The default macOS workflow is incremental and preserves `build/`, including the disconnected `psd_sdk` FetchContent checkout. Use `./build.sh --clean local` or `CLEAN_BUILD_DIR=1 ./build.sh local` only after changing toolchains or when recovering a stale cache. Local packages keep symbols with `macdeployqt -no-strip`; Developer ID, Mac App Store, and combined distribution modes omit `-no-strip` so release bundles are stripped by the deployment tool. `MACDEPLOYQT_NO_STRIP=0|1` remains an explicit override.

After `macdeployqt`, the script inspects every Mach-O file with `otool -L` and every `LC_RPATH` with `otool -l`. Packaging fails if a deployed binary still references an absolute non-system dependency or RPATH, preventing a build-tree, `$HOME/.local`, Homebrew, or maintainer-specific path from escaping into the signed bundle.

The script is expected to run on the macOS system Bash 3.2 with `set -u` enabled and no custom extra CMake arguments. A default local build must not require callers to define `CMAKE_EXTRA_ARGS` or any other optional argument array before invoking `./build.sh`. Because `build.sh` is the official packaging entry point, it is tracked source and must not be ignored by `.gitignore`. Vincent's marketing version and generated macOS `CFBundleVersion` are both fixed to `4.0` by CMake and the configured Info.plist.

Vincent intentionally supports only the repository-local `build/` CMake binary directory. Configure, build, test, and package commands must use `-B build`, `cmake --build build`, and `ctest --test-dir build`; alternate build trees are rejected during CMake configure so CLion and shell workflows cannot silently produce stale bundles elsewhere.

On a Darwin host, CMake sets the macOS 12 deployment target before `project()` performs compiler detection. This keeps compiler feature checks, FetchContent targets, and the final bundle on the same minimum-OS contract instead of adding the flag only after the first try-compile.

On Windows, CMake links `Vincent.exe` with the Windows GUI subsystem, so launching it does not allocate a console window and the `QGuiApplication` event loop owns the process lifetime. CMake also compiles `resources/windows/Vincent.manifest.in` and `resources/windows/Vincent.rc.in` into the executable. These resources carry the application icon, four-part file/product version, `asInvoker` execution level, Windows 10/11 compatibility, Per-Monitor V2 DPI awareness, and long-path awareness. The build copies `LVRS.dll`, `libiiPaintEngine.dll`, and the runtime DLLs found next to the selected MinGW compiler into `build/`; it does not search an unrelated system `PATH` for a different compiler runtime. This keeps direct CLion or shell launches from the `build/` tree aligned with the selected Qt kit without partially redeploying Qt itself; a missing `LVRS.dll` dialog from `build/Vincent.exe` means the build tree is stale and should be rebuilt with `cmake --build build`.

Every mode verifies that `CFBundleIconFile` resolves to `Contents/Resources/Appicon.icns`, that the legacy `Contents/Resources/icon.icns` file is not present, and that the bundled icon matches `resources/Appicon.icns`. All generated `.pkg` payloads are inspected before the build exits, so a stale installer package that still contains `icon.icns` fails the build instead of installing the old app icon.

`resources/Appicon.icns` is the canonical macOS icon source. After replacing it, regenerate the App Store/Xcode asset catalog before building:

```bash
tools/sync_app_icon_assets.sh
```

The script rewrites `packaging/macos/Vincent.xcassets/AppIcon.appiconset`, including `AppIcon-1024.png`, from the canonical `.icns`. The CMake bundle target also removes stale `Contents/Resources/icon.icns` after each macOS build, so an incremental `build/` bundle cannot keep advertising the removed legacy icon.

Signed distribution modes must be requested explicitly:

```bash
VINCENT_BUILD_MODE=devid ./build.sh
VINCENT_BUILD_MODE=mas ./build.sh
VINCENT_BUILD_MODE=all ./build.sh
```

`devid` requires `Developer ID Application` and `Developer ID Installer` certificates plus notarization credentials. `mas` requires `Apple Distribution` and `3rd Party Mac Developer Installer` certificates. `all` runs both distribution flows. The local `dist/Vincent.pkg` and `dist/Vincent-appstore.pkg` outputs are unsigned smoke/install artifacts only; they are not notarized or App Store upload packages. For Developer ID notarization, prefer `NOTARY_KEYCHAIN_PROFILE`; if Apple ID mode is used, provide the app-specific password through `NOTARY_APP_PASSWORD` rather than storing it in the script.

## 1b. Windows Build, Package, and Current-User Install Script
Run the Windows build from Windows PowerShell 5.1 or newer. The script expects Windows-built Qt, LVRS, and iiPaintEngine prefixes; macOS `.local` binaries cannot be reused on Windows. Set the prefix variables once per shell session:

```powershell
$env:QT_PREFIX = "C:\Qt\6.8.3\mingw_64"
$env:LVRS_PREFIX = "$HOME\.local\LVRS"
$env:IIPAINTENGINE_PREFIX = "$HOME\.local\iiPaintEngine"
```

The supported MinGW setup uses Qt's bundled CMake, Ninja, and MinGW tools on `PATH`. The vendored `psd_sdk` build treats `Psdminiz.c` as C++ because that upstream C file includes headers with C++ `static_assert` declarations. The current iiPaintEngine Qt adapter exposes the brush opacity toggle as `brushOpacityEnabled`, so QML and bridge code should not use the older `pressureToOpacityEnabled` property name.

Then build, test, deploy Qt runtime files, copy dependency DLLs, and create the Windows ZIP package:

```powershell
powershell -ExecutionPolicy Bypass -File .\build-windows.ps1
```

To also create the Windows Installer package, make WiX Toolset 3.x available through `WIX_TOOLS_DIR`, `WIX`, or `PATH`, then add `-CreateMsi`:

```powershell
powershell -ExecutionPolicy Bypass -File .\build-windows.ps1 -CreateMsi
```

The script always configures with `cmake -S . -B build`, preserves that repository-local tree for incremental work, builds with CMake's parallel mode, and runs `ctest --test-dir build --output-on-failure` unless `-SkipTests` is passed. Use `powershell -ExecutionPolicy Bypass -File .\build-windows.ps1 -Clean` only after changing toolchains or when recovering a stale build tree. Passing `-SkipTests` configures `BUILD_TESTING=OFF` and builds only the `Vincent` target. Runtime deployment uses `windeployqt --qmldir App/qml --translations en,ko`, requires the Qt Quick Shapes QML plugin used by the vector brush cursor, omits the release-only QML debugger plugin group plus the unused PDF, Qt Virtual Keyboard, PostgreSQL, and Mimer SQL plugins, then copies the LVRS and iiPaintEngine runtime DLLs. LVRS QML is compiled into the LVRS binary, so the stage deliberately removes any loose `qml/LVRS` directory emitted by deployment instead of copying thousands of duplicate source files or rewriting `qmldir` `prefer` directives.

Before packaging, the script verifies that the staged executable is a native AMD64 PE32+ Windows GUI binary, has the required ASLR/DEP flags, and exposes the expected file version, product version, and product name. MinGW Release and MinSizeRel stages strip the complete COFF symbol tables from `Vincent.exe`, `LVRS.dll`, and `libiiPaintEngine.dll` only; Qt's deployed binaries are left untouched. The stage is also inspected with `objdump`; if a dependency such as `LVRS.dll` imports `__cxa_thread_atexit` while the staged `libstdc++-6.dll` does not export it, the script fails with a MinGW ABI error and the dependency must be rebuilt with the same MinGW kit as Qt. The same pass checks the PE import closure of every staged executable and DLL against the staged payload and Windows system DLL set, so unresolved third-party imports fail before ZIP or MSI creation. The staged app is written to `dist/Vincent-Windows`, the ZIP package is written to `dist/Vincent-4.0-Windows.zip`, and the MSI package is written to `build/Vincent-4.0-Windows.msi` when `-CreateMsi` is passed. The generated MSI installs for the current user under `%LOCALAPPDATA%\Programs\Vincent`, and uses `AllowSameVersionUpgrades` so rebuilding Vincent 4.0.0 and reinstalling it replaces a previously installed 4.0.0 payload instead of leaving stale DLLs in the install directory.

Normal Windows startup does not open or flush a diagnostic file. `Main.qml` establishes a hidden 1400x880 launch geometry as the preferred size. Before the first show, the common Qt entry point bounds that hidden size once to the selected screen's available geometry, then shows the completed window. It does not resize the window after it becomes visible. The same pre-show rule prevents Cocoa and Linux compositors from visibly correcting an oversized first frame. The asynchronous canvas `Loader` therefore cannot renegotiate the top-level size; users can still resize or maximize the window after launch. For an opt-in startup timing trace, set `$env:VINCENT_STARTUP_TRACE = "1"` before launching `Vincent.exe`; startup milestones are then appended to `%TEMP%\Vincent-startup.log`. A fatal root-QML object creation failure is written to that file and flushed even when tracing is disabled, but ordinary Qt/QML warnings are not redirected there. If Windows reports that the procedure entry point `__cxa_thread_atexit` cannot be found in `LVRS.dll`, that failure happens before Vincent reaches application logging and indicates a platform runtime mismatch, not a QML load failure. Rebuild LVRS with the same Qt MinGW kit used for Vincent, rerun `build-windows.ps1 -Clean`, and regenerate the installer from the refreshed `dist/Vincent-Windows` payload.

For a current-user smoke install, run:

```powershell
powershell -ExecutionPolicy Bypass -File .\build-windows.ps1 -InstallForCurrentUser
```

That copies the staged runtime to `%LOCALAPPDATA%\Programs\Vincent` by default and creates a shortcut in the current user's Start Menu. `-InstallDir <path>` may override the location only below the current user's Local Application Data directory. An existing non-empty target must contain Vincent's ownership marker or a versioned `Vincent.exe`; drive roots, user data roots, reparse points, unrelated directories, and the staged source are rejected before recursive deletion. If the Qt kit is MSVC-based and `cl.exe` is not already in `PATH`, the script tries to load the Visual Studio C++ build environment through `vswhere.exe`; otherwise, launch it from a Developer PowerShell for VS.

## 2. Configure the Release Build
```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0
cmake --build build --target Vincent
```
If you need a different bundle identifier, update `BUNDLE_ID` in `CMakeLists.txt` before configuring. Adjust the deployment target if you need to support newer or older macOS releases.

## 3. Stage the App Bundle
The built app lives under `build/Vincent.app`. Copy it to a staging directory (for example, `dist/Vincent.app`) so you can safely run deployment tools without touching your build tree.

## 4. Embed Qt Frameworks
Run `macdeployqt` in App Store mode to embed the required Qt frameworks and QML plugins:
```bash
macdeployqt "dist/Vincent.app" \
  -appstore-compliant \
  -qmldir=App/qml \
  -always-overwrite
```
Verify that all `.framework` bundles now sit inside `dist/Vincent.app/Contents/Frameworks` and that `qt.conf` exists in `Contents/Resources/`.

## 5. Prepare Metadata
Update the generated `Info.plist` (inside `dist/Vincent.app/Contents/`) with:
- `CFBundleIdentifier` matching your bundle ID.
- `CFBundleVersion` and `CFBundleShortVersionString` set to a semantic version number you are shipping.
- `CFBundleIconFile` should resolve to the bundled `resources/Appicon.icns` file. Windows builds embed `resources/Appicon.ico` through the generated resource script.
- Any usage description strings your app requires (e.g., `NSMicrophoneUsageDescription`)—Vincent 4.0 currently relies only on file picker access.

## 6. Sandbox Entitlements
Customize `packaging/macos/Vincent.entitlements` if the app needs additional capabilities. The default template enables the App Sandbox and grants read/write access to user-selected files and picture libraries. Keep entitlements minimal to improve App Review approval chances.

## 7. Codesign the Bundle
```bash
codesign --force --options runtime \
  --entitlements packaging/macos/Vincent.entitlements \
  --sign "Apple Distribution: MUYEONG YUN (5U49ST9XZH)" \
  "dist/Vincent.app"
```
Then validate the signature:
```bash
codesign --verify --deep --strict "dist/Vincent.app"
spctl --assess --type execute "dist/Vincent.app"
```
If `spctl` warns about missing the hardened runtime, make sure `--options runtime` was passed.

## 8. Create the Installer Package
```bash
productbuild \
  --component "dist/Vincent.app" /Applications \
  --sign "Apple Installer: MUYEONG YUN (5U49ST9XZH)" \
  "dist/Vincent-4.0.pkg"
```
This produces the installer payload required by App Store Connect. Keep the `.pkg` under 4 GB.

If installing `dist/Vincent.pkg` or `dist/Vincent-appstore.pkg` shows an older app icon, treat the package as stale. A default local build now refreshes `dist/Vincent.app` and regenerates both unsigned package outputs from that app. Use `VINCENT_BUILD_MODE=devid ./build.sh`, `VINCENT_BUILD_MODE=mas ./build.sh`, or `VINCENT_BUILD_MODE=all ./build.sh` only when signed distribution packages are needed. You can inspect the package payload directly:

```bash
pkgutil --payload-files dist/Vincent.pkg | grep 'Contents/Resources/.*icns'
```

The payload should list `./Vincent.app/Contents/Resources/Appicon.icns` and should not list `./Vincent.app/Contents/Resources/icon.icns`.

If Transporter's Active list still shows the previous icon after this payload check passes, do not treat that queue thumbnail as proof that the `.pkg` still embeds the old icon. The Active list can identify the App Store Connect record by app name and Apple ID before delivery, so remove and re-add the item, confirm you dragged the freshly rebuilt `dist/Vincent-appstore.pkg`, and inspect the package payload again:

```bash
pkgutil --expand-full dist/Vincent-appstore.pkg /tmp/vincent-appstore-payload
cmp resources/Appicon.icns /tmp/vincent-appstore-payload/com.iisacc.vincent.painter.pkg/Payload/Vincent.app/Contents/Resources/Appicon.icns
```

If the `cmp` command succeeds, the upload package contains the current app icon. Update or refresh the App Store Connect app record icon separately if Transporter continues to show the old store listing icon before delivery.

If Dock or Launchpad still shows the old icon after the installed bundle is correct, remove stale `/Applications/Vincent.app` Dock entries and let LaunchServices re-register the rebuilt app. A pinned Dock item can continue pointing at an old or removed bundle path even after the workspace `dist/Vincent.app` has the new icon.

## 9. Upload to App Store Connect
1. Open Transporter.
2. Drag `dist/Vincent-4.0.pkg` into the queue.
3. Provide your App Store Connect credentials and upload.
4. Resolve any validation issues that Transporter reports (missing icons, entitlement mismatches, etc.).

## 10. Post-Upload Checklist
- Create an App Store Connect record with screenshots, localized descriptions, and pricing.
- Attach the uploaded build to a new version submission and complete the export compliance questionnaire.
- Submit for review.

## Troubleshooting Tips
- Use `otool -L dist/Vincent.app/Contents/MacOS/Vincent` to ensure no absolute paths to your build tree remain.
- Leverage `plutil -p` to inspect `Info.plist` after `macdeployqt` runs.
- If Transporter rejects the upload due to missing `LC_VERSION_MIN_MACOSX`, make sure `CMAKE_OSX_DEPLOYMENT_TARGET` is set at configure time.
- Should you require notarization for outside-the-store distribution, rerun codesigning with the same entitlements and submit via `xcrun notarytool`; App Store submissions do not need separate notarization.
- If CLion reports `loading 'build.ninja': No such file or directory`, rerun CMake configure for the selected profile before building. The project also injects local build-tree rpaths for `$HOME/.local/LVRS` and `$HOME/.local/iiPaintEngine` so CLion and CTest can run without extra `DYLD_LIBRARY_PATH` setup.

## Linux Build and Packaging
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --build build --target package
```
The TGZ archive is written to the build directory as `Vincent-<version>-Linux.tar.gz`. CMake uses Qt 6.8's `qt_generate_deploy_qml_app_script` to collect the linked Qt, LVRS, and iiPaintEngine shared libraries, QML modules, translations, and platform plugins into the install image. Vincent's installed ELF RPATH is `$ORIGIN/../lib` (or the configured GNU install lib directory), so it does not retain a build-tree or `$HOME/.local` runtime dependency. The archive uses a root-relative `bin/`, `lib/`, `qml/`, `plugins/`, and `share/` layout; launch it with `bin/Vincent` after extraction.

The install image includes `share/applications/com.iisacc.vincent.painter.desktop` with `Terminal=false` and a hicolor application icon. Validate release archives on both X11 and Wayland: run an Xvfb/XCB smoke launch and a headless Weston/Wayland smoke launch, inspect `ldd bin/Vincent` for unresolved dependencies, and confirm no resolved path points into the source tree, `build/`, or the maintainer's home directory. If you prefer an installed tree, run `cmake --install build --prefix <path>`; the same relative layout and deployment script are used.

## Automated Tests
To configure and run the current unit test suite:

```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --target Vincent tests_canvasdocumentviewmodel tests_drawingsurfaceitem
ctest --test-dir build --output-on-failure
```
