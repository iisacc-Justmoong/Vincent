# Changelog

## 2026-07-02

### 현재 작업트리 수정본

- Flat 이미지 열기 경로를 transformable 이미지 객체 생성에서 원본 이미지 크기의 배경 래스터 캔버스 생성으로 변경했다. 이제 PNG/JPEG/BMP/WebP/TIFF 같은 일반 이미지 파일은 작업 영역 크기에 맞춰 축소 배치되지 않으며, 캔버스 크기와 이미지 크기가 일치한다.
- 열기 성공 후에는 이미지가 `Background` 래스터로 들어가고, 그 위에 투명 `Layer 1`이 선택 상태로 생성되어 기존 레이어 기반 편집 흐름을 유지한다.
- `tst_drawingsurfaceitem`은 C++ `openRaster`가 큰 이미지도 원본 크기로 여는지와 QML 열기 경로가 이미지 객체 대신 이미지 크기 캔버스를 생성하고 같은 크기로 저장하는지를 검증하도록 갱신했다.
- Move tool의 자유 변형에서 이동과 코너 핸들 리사이즈가 더 이상 캔버스 경계에 clamp되지 않도록 바꿨다. 객체 bounds는 Photoshop 레이어처럼 캔버스 밖으로 확장될 수 있으며, 최소 크기 제한만 유지된다.
- Flat 이미지로 만든 문서에서 다시 이미지를 열면 캔버스를 재생성하지 않고 기존 캔버스 중앙에 원본 크기의 transformable 이미지 객체 레이어를 추가하도록 바꿨다.
- 모양 툴과 텍스트 툴이 생성하는 객체를 레이어 패널에서 각각 별개의 `layer` 행으로 투영하도록 바꿨다. 레이어 행은 `contentKind`로 shape/text/image/raster layer 구분을 유지한다.

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

- `Main.qml`의 메뉴 단축키 연결을 `Controls.MenuItem.shortcut`에서 `Controls.Action.shortcut`으로 옮겼다. 현재 Qt Quick Controls `MenuItem`에는 `shortcut` 속성이 없어 앱 시작 시 QML 로드가 실패했기 때문이다.
- 메뉴 항목은 `action` 속성으로 named action에 연결되며, 테스트와 앱 구조 문서는 이 계약을 기준으로 갱신했다.
- `build.sh` 기본 local 흐름이 `dist/Vincent.app` 생성에서 멈추지 않고 unsigned `dist/Vincent.pkg`와 `dist/Vincent-appstore.pkg`를 함께 재생성하도록 바꿨다.
- `dist/Vincent.pkg`는 수정 전 산출물이어서 현재 정상 `dist/Vincent.app` 기준으로 unsigned 로컬 설치용 패키지를 재생성했다. 이 패키지는 Developer ID Installer 서명이 없으므로 Gatekeeper 배포 신뢰 대상은 아니다.
- Transporter Active 목록에 이전 아이콘이 보이는 경우를 패키지 payload 문제와 분리했다. 현재 `dist/Vincent-appstore.pkg`는 `Appicon.icns`를 포함하고 legacy `icon.icns`는 포함하지 않으므로, 목록 썸네일은 App Store Connect 레코드/캐시 아이콘으로 별도 확인해야 한다.
- CMake binary directory를 저장소 루트의 `build/`로 강제했다. 이제 다른 build tree로 configure하면 CMake가 즉시 실패하며, 저장소 지침과 패키징 문서도 `build/` 단일 경로만 안내한다.
- CMake 프로젝트 버전, macOS `CFBundleShortVersionString`, `CFBundleVersion`, 앱 메뉴 표시, README와 패키징 문서를 모두 Vincent 4.0 기준으로 고정했다.
- Windows PowerShell용 `build-windows.ps1`을 추가했다. 이 스크립트는 Windows용 Qt/LVRS/iiPaintEngine prefix를 검증하고, `build/`에서 configure/build/test를 수행한 뒤 `windeployqt`, dependency DLL 및 LVRS QML import 복사, `dist/Vincent-Windows` staging, `dist/Vincent-4.0-Windows.zip` 생성을 처리한다.
- Windows configure에서 macOS 전용 `productbuild` 설정이 섞이지 않도록 CPack 설정을 플랫폼별로 분리하고, Windows install target은 `Vincent.exe` runtime을 패키지 루트에 설치하도록 정리했다.

### 검증 관점

- 오늘 커밋들은 기능 변경에 대응하는 테스트와 문서 갱신을 함께 포함한다.
- 현재 작업트리 수정본은 아직 커밋되지 않았으며, 설치 앱 시작 실패를 막기 위한 QML 메뉴 단축키 후속 변경과 `build/` 단일 CMake build tree 계약으로 분류된다.
- `tests_mainqmlcontract`, macOS 빌드 workflow 계약 테스트, 전체 `ctest`, `./build.sh local`, `/Applications/Vincent.app` 실행 스모크, 새 `dist/Vincent.pkg` payload 및 추출 앱 실행 스모크를 통과했다.
- `dist/Vincent-appstore.pkg` payload의 `Appicon.icns`는 canonical `resources/Appicon.icns`와 일치하며, `CFBundleIconFile`도 `Appicon.icns`로 해석된다.
- `dist/Vincent.pkg`는 unsigned 상태이므로 `spctl -a -vv -t install dist/Vincent.pkg`는 `no usable signature`로 거부된다. 신뢰 배포용 패키지는 Developer ID Installer 서명과 notarization이 필요하다.
- Windows 스크립트는 macOS 호스트에서 직접 실행 검증할 수 없으므로, `tests_windowsbuildworkflowcontract`가 PowerShell 실행 계약, CMake Windows install/ZIP CPack 설정, 문서화된 Windows 실행 명령을 텍스트 계약으로 고정한다.
