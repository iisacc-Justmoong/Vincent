cmake_minimum_required(VERSION 3.24)
project(Vincent VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(DEFINED Qt6_DIR AND EXISTS "${Qt6_DIR}")
    list(PREPEND CMAKE_MODULE_PATH "${Qt6_DIR}")
endif()

# WrapOpenGL을 모듈 방식으로 우선 탐색(FindWrapOpenGL.cmake)
find_package(WrapOpenGL QUIET MODULE)

# 필요 시 대체: 타깃이 아직 없으면 직접 만들어 제공
if(NOT TARGET WrapOpenGL::WrapOpenGL)
    # 1) 표준 OpenGL 패키지 시도
    find_package(OpenGL QUIET)
    if(TARGET OpenGL::GL)
        add_library(WrapOpenGL::WrapOpenGL INTERFACE IMPORTED)
        target_link_libraries(WrapOpenGL::WrapOpenGL INTERFACE OpenGL::GL)
    elseif(APPLE)
        # 2) macOS 프레임워크 직결(구형이지만 호환성용으로 충분)
        find_library(_APPLE_OPENGL_FRAMEWORK OpenGL)
        if(_APPLE_OPENGL_FRAMEWORK)
            add_library(WrapOpenGL::WrapOpenGL INTERFACE IMPORTED)
            target_link_libraries(WrapOpenGL::WrapOpenGL INTERFACE "${_APPLE_OPENGL_FRAMEWORK}")
        endif()
        unset(_APPLE_OPENGL_FRAMEWORK)
    endif()
endif()

# 3) Craft 환경(CRAFTROOT) 우선 검색 경로 정리: 존재할 때만 선두에 삽입
if(DEFINED ENV{CRAFTROOT})
    set(_craft_prefix "$ENV{CRAFTROOT}")
    # 이미 포함되었으면 중복 삽입 방지
    list(FIND CMAKE_PREFIX_PATH "${_craft_prefix}" _craft_index)
    if(_craft_index EQUAL -1)
        list(PREPEND CMAKE_PREFIX_PATH "${_craft_prefix}")
    endif()
    # Qt6_DIR 힌트(선택)
    if(NOT DEFINED Qt6_DIR)
        set(_craft_qt_dir "$ENV{CRAFTROOT}/lib/cmake/Qt6")
        if(EXISTS "${_craft_qt_dir}/Qt6Config.cmake")
            set(Qt6_DIR "${_craft_qt_dir}" CACHE PATH "Qt6 config path (CraftRoot)" FORCE)
        endif()
        unset(_craft_qt_dir)
    endif()
    unset(_craft_index)
    unset(_craft_prefix)
endif()

# 4) 요구 버전 선언
set(QT_MIN_VERSION "6.8.0")
set(KF_MIN_VERSION "6.0.0")

# 5) ECM/KF6 기본 설정 로드(ECM 먼저)
find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)

# 6) Qt6/KF6 탐색을 타깃·매크로 사용보다 먼저 수행
find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS
        Core
        Qml
        Quick
        QuickControls2
)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS
        Kirigami
        I18n
        Svg
        Config
)
find_package(KF6KirigamiPlatform ${KF_MIN_VERSION} REQUIRED)

# 7) 표준 Qt 프로젝트 설정(가용성 확보 후 호출)
qt_standard_project_setup(REQUIRES ${QT_MIN_VERSION})

# 8) 실행 타깃 생성
qt_add_executable(Vincent
        MANUAL_FINALIZATION
        MACOSX_BUNDLE
)

# 9) 하위 디렉터리(예: App)에서 소스/리소스를 타깃에 추가하거나
#    별도 라이브러리를 만들어 아래에서 링크할 수 있음
add_subdirectory(App)

# 10) 링크 일괄 지정(단일 호출로 정리)
target_link_libraries(Vincent
        PRIVATE
        Qt6::Core
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        KF6::Kirigami
        KF6::I18n
        KF6::Svg
        KF6::KirigamiPlatform
        KF6::ConfigCore
)

# 11) macOS 번들·코드사인 속성
if(APPLE)
    set(VINCENT_BUNDLE_IDENTIFIER "com.iisacc.vincent.app")
    set(VINCENT_CATEGORY "public.app-category.graphics-design")
    set(VINCENT_VERSION "0.1.0")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0")

    # 아이콘(.icns)을 번들 Resources에 포함
    set(VINCENT_APP_ICON "${CMAKE_SOURCE_DIR}/resources/icon.icns")
    get_filename_component(_vincent_icon_abs "${VINCENT_APP_ICON}" ABSOLUTE)
    get_filename_component(_vincent_icon_name_we "${_vincent_icon_abs}" NAME_WE)
    if(EXISTS "${_vincent_icon_abs}")
        target_sources(Vincent PRIVATE "${_vincent_icon_abs}")
        set_source_files_properties("${_vincent_icon_abs}" PROPERTIES
                MACOSX_PACKAGE_LOCATION "Resources")
    endif()

    # Info.plist를 빌드 트리로 복사(토큰 대체)하여 Xcode가 참조할 수 있도록 함
    set(_vincent_info_plist "${CMAKE_CURRENT_BINARY_DIR}/Info.plist")
    configure_file(
            "${CMAKE_SOURCE_DIR}/mac/Info.plist.in"
            "${_vincent_info_plist}"
            @ONLY
    )
    set_source_files_properties("${_vincent_info_plist}" PROPERTIES GENERATED TRUE)

    # Entitlements는 템플릿을 그대로 복사하여 Xcode가 코드사인 단계에서 사용하게 한다
    set(_vincent_entitlements_source "${CMAKE_SOURCE_DIR}/mac/Vincent.entitlements")
    if(NOT EXISTS "${_vincent_entitlements_source}")
        set(_vincent_entitlements_source "${CMAKE_SOURCE_DIR}/packaging/macos/Vincent.entitlements")
    endif()
    set(_vincent_entitlements "")
    if(EXISTS "${_vincent_entitlements_source}")
        set(_vincent_entitlements "${CMAKE_CURRENT_BINARY_DIR}/Vincent.entitlements")
        configure_file("${_vincent_entitlements_source}" "${_vincent_entitlements}" COPYONLY)
        set_source_files_properties("${_vincent_entitlements}" PROPERTIES GENERATED TRUE)
    endif()

    set_target_properties(Vincent PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_INFO_PLIST "${_vincent_info_plist}"
            MACOSX_BUNDLE_ICON_FILE "${_vincent_icon_name_we}"
            MACOSX_BUNDLE_GUI_IDENTIFIER "${VINCENT_BUNDLE_IDENTIFIER}"
            MACOSX_BUNDLE_BUNDLE_VERSION "${VINCENT_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${VINCENT_VERSION}"

            XCODE_ATTRIBUTE_INFOPLIST_FILE "${_vincent_info_plist}"
            XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${VINCENT_BUNDLE_IDENTIFIER}"
            XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "GRWGSK8RDF"
            XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
            XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Apple Distribution"
            XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${_vincent_entitlements}"
            XCODE_ATTRIBUTE_SKIP_INSTALL "NO"
    )
endif()

# 12) Qt의 MANUAL_FINALIZATION 사용 시 마무리
qt_finalize_executable(Vincent)

install(TARGETS Vincent ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
