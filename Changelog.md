# Changelog

## 2026-07-01

분석 기준: `2026-07-01 00:00:00 +0900` 이후의 Git 커밋 3건과 현재 작업트리 수정본을 함께 검토했다.

### 커밋 반영분

#### `732ed9a` - Add menu bar func

- 브러시 압력 입력이 불투명도에 반영되는지 제어하는 `brushPressureControlsOpacity` 상태를 문서 모델, QML 툴바, 캔버스 표면, 브리지 계층에 연결했다.
- 브러시 설정 패널에 `Pressure Opacity` 토글을 추가해 압력 기반 불투명도 동작을 사용자가 켜고 끌 수 있게 했다.
- 수동 stroke 입력 경로에서 pressure-sensitive 입력이 마우스 이벤트로 축소되지 않도록 합성 `QTabletEvent` 경로를 추가했다.
- 테스트는 `tst_canvasdocumentviewmodel`, `tst_canvastoolbarqmlcontract`, `tst_drawingsurfaceitem`에 pressure opacity 상태, QML 계약, pressure 보존 시나리오를 추가했다.
- README와 앱 구조 문서는 브러시 압력/불투명도 및 관련 UI 계약을 반영했다.

#### `9388d2e` - Add func shortcut

- `Main.qml`이 메뉴 단축키의 단일 계약 소유자가 되도록 `shortcutNewCanvas`, `shortcutUndo`, `shortcutBrushTool`, `shortcutFitCanvasToWindow` 같은 named shortcut property를 추가했다.
- File, Edit, Tools, Shape Kind, Window 메뉴 항목에 실제 `shortcut` 바인딩을 연결하고, 메뉴만으로 실행되던 일부 명령에는 `Shortcut` 항목을 추가해 표시 단축키와 실행 동작을 맞췄다.
- Help 메뉴의 `Keyboard Shortcuts` 항목을 File/Edit/Tools/Shape Kind/Window 범주별 전체 단축키 참조로 확장했다.
- 테스트는 `tst_mainqmlcontract`에서 모든 shortcut property, 메뉴 바인딩, Help 참조, application shortcut 존재 여부를 계약으로 고정했다.
- README와 앱 구조 문서는 전역 메뉴 단축키 범위와 플랫폼별 Command/Ctrl 차이를 반영했다.

#### `d6c65f8` - Add func shortcut

- 플랫폼 앱 아이콘 소스를 `resources/Appicon.icns`와 `resources/Appicon.ico`로 정리하고, macOS 번들과 Windows 리소스가 각각 해당 파일을 사용하도록 CMake 패키징 계약을 갱신했다.
- macOS `Info.plist`의 `CFBundleIconFile`을 CMake 변수 기반으로 치환해 실제 번들 아이콘 파일명과 중복 계약이 생기지 않도록 했다.
- `.gitignore`에서 `resources/Appicon.icns`와 `resources/Appicon.ico`를 추적 허용 목록에 추가하고, 기존 `resources/icon.icns`를 제거했다.
- 테스트는 `tst_macosbuildworkflowcontract`에서 아이콘 파일 존재, CMake 번들 등록, Windows `.rc` 생성, `Info.plist` 치환, `.gitignore` 허용 규칙을 확인하도록 확장했다.
- README, `docs/BUILD.md`, `docs/app_structure.md`는 macOS/Windows 앱 아이콘 패키징 계약을 설명하도록 갱신했다.

### 현재 작업트리 수정본

- CMake macOS 빌드 후처리에 legacy `Contents/Resources/icon.icns` 제거 단계를 추가해, 증분 빌드나 기존 번들에 남은 오래된 아이콘 파일이 새 `Appicon.icns`와 공존하지 않도록 했다.
- `tools/sync_app_icon_assets.sh`를 추가해 canonical `resources/Appicon.icns`에서 Xcode/App Store용 `packaging/macos/Vincent.xcassets/AppIcon.appiconset` PNG 파일들을 재생성할 수 있게 했다.
- AppIcon asset catalog의 `16x16`부터 `1024`까지 PNG 산출물을 새 아이콘 기준으로 갱신했다.
- 루트의 legacy `icon.icns`는 삭제되었고, 루트 `Appicon.icns`가 미추적 파일로 남아 있다. 추적 대상 canonical icon은 `resources/Appicon.icns`다.
- `docs/BUILD.md`는 stale installer package 진단 절차를 추가했다. `pkgutil --payload-files dist/Vincent.pkg`로 `Appicon.icns`가 포함되고 `icon.icns`가 제외되는지 확인하는 흐름을 문서화했다.
- `tst_macosbuildworkflowcontract`는 sync script 존재, asset catalog 동기화 계약, legacy icon 제거, fake bundle의 `CFBundleIconFile` 검증 준비를 포함하도록 보강됐다.

### 검증 관점

- 오늘 커밋들은 기능 변경에 대응하는 테스트와 문서 갱신을 함께 포함한다.
- 현재 작업트리 수정본은 아직 커밋되지 않았으며, 앱 아이콘 stale bundle/package 문제를 막기 위한 패키징 후속 변경으로 분류된다.
- 추가 검증 시 우선순위는 `cmake --build build --target tests_macosbuildworkflowcontract`, `ctest --test-dir build --output-on-failure -R tests_macosbuildworkflowcontract`, `./build.sh local`, 번들 `Contents/Resources` 아이콘 확인, `.pkg` payload 확인 순서가 적절하다.
