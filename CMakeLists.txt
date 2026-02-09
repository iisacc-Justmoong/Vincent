cmake_minimum_required(VERSION 3.24)
project(Vincent VERSION 2.1.0 LANGUAGES CXX)

set(BUNDLE_ID "com.iisacc.vincent.painter")
set(VINCENT_QML_MODULE_MAJOR "${PROJECT_VERSION_MAJOR}")
# QML import minor는 API revision 없이는 0으로 유지하는 편이 안전하다.
set(VINCENT_QML_MODULE_MINOR "0")
set(VINCENT_QML_MODULE_VERSION "${VINCENT_QML_MODULE_MAJOR}.${VINCENT_QML_MODULE_MINOR}")
set(VINCENT_MIN_MACOS_VERSION "12.0")

math(EXPR _vincent_default_bundle_version
        "${PROJECT_VERSION_MAJOR} * 10000 + ${PROJECT_VERSION_MINOR} * 100 + ${PROJECT_VERSION_PATCH}")
set(_vincent_bundle_version_default "${_vincent_default_bundle_version}")
if(DEFINED ENV{VINCENT_BUNDLE_VERSION} AND NOT "$ENV{VINCENT_BUNDLE_VERSION}" STREQUAL "")
    set(_vincent_bundle_version_default "$ENV{VINCENT_BUNDLE_VERSION}")
endif()
set(VINCENT_BUNDLE_VERSION "${_vincent_bundle_version_default}" CACHE STRING "CFBundleVersion (numeric build number)")
if(NOT VINCENT_BUNDLE_VERSION MATCHES "^[0-9]+$")
    message(FATAL_ERROR "VINCENT_BUNDLE_VERSION must be numeric (example: 20100)")
endif()
message(STATUS "Vincent versions: marketing=${PROJECT_VERSION}, bundle=${VINCENT_BUNDLE_VERSION}, qml=${VINCENT_QML_MODULE_VERSION}")
unset(_vincent_bundle_version_default)
unset(_vincent_default_bundle_version)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# macOS 최소 타겟 고정
if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "${VINCENT_MIN_MACOS_VERSION}" CACHE STRING "Minimum macOS version" FORCE)
endif()

# Qt 경로 힌트(선택)
if(DEFINED Qt6_DIR AND EXISTS "${Qt6_DIR}")
    list(PREPEND CMAKE_MODULE_PATH "${Qt6_DIR}")
endif()

# OpenGL 래퍼 탐색
find_package(WrapOpenGL QUIET MODULE)
if(NOT TARGET WrapOpenGL::WrapOpenGL)
    find_package(OpenGL QUIET)
    if(TARGET OpenGL::GL)
        add_library(WrapOpenGL::WrapOpenGL INTERFACE IMPORTED)
        target_link_libraries(WrapOpenGL::WrapOpenGL INTERFACE OpenGL::GL)
    elseif(APPLE)
        find_library(_APPLE_OPENGL_FRAMEWORK OpenGL)
        if(_APPLE_OPENGL_FRAMEWORK)
            add_library(WrapOpenGL::WrapOpenGL INTERFACE IMPORTED)
            target_link_libraries(WrapOpenGL::WrapOpenGL INTERFACE "${_APPLE_OPENGL_FRAMEWORK}")
        endif()
        unset(_APPLE_OPENGL_FRAMEWORK)
    endif()
endif()

# Craft 환경 우선 검색
if(DEFINED ENV{CRAFTROOT})
    set(_craft_prefix "$ENV{CRAFTROOT}")
    list(FIND CMAKE_PREFIX_PATH "${_craft_prefix}" _craft_index)
    if(_craft_index EQUAL -1)
        list(PREPEND CMAKE_PREFIX_PATH "${_craft_prefix}")
    endif()
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

# 요구 버전
set(QT_MIN_VERSION "6.8.0")

# 기본 설치 경로 및 요약
include(GNUInstallDirs)
include(FeatureSummary)

# Qt6
find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Qml Quick QuickControls2 Svg)

qt_standard_project_setup(REQUIRES ${QT_MIN_VERSION})

# 실행 타깃
qt_add_executable(Vincent
        MANUAL_FINALIZATION
        MACOSX_BUNDLE
)

# 소스 추가(하위 디렉터리에서 타깃에 파일을 attach)
add_subdirectory(App)

# QML 모듈
if(CMAKE_GENERATOR STREQUAL "Xcode")
    set(QT_COPY_QML_FILES_OLD_WAY ON)
endif()
qt_add_qml_module(Vincent
        URI Vincent
        VERSION ${VINCENT_QML_MODULE_VERSION}
        QML_FILES
        App/qml/Main.qml
        App/qml/PainterCanvasPage.qml
        App/qml/CanvasToolBar.qml
        App/qml/DrawingSurface.qml
)

# qmltyperegistrar 빈 메타타입 안전장치
get_target_property(_vincent_binary_dir Vincent BINARY_DIR)
get_property(_vincent_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(_vincent_multi_config)
    foreach(_vincent_config IN LISTS CMAKE_CONFIGURATION_TYPES)
        string(TOLOWER "${_vincent_config}" _vincent_config_lower)
        string(TOLOWER "Vincent_${_vincent_config}" _vincent_target_lower)
        set(_vincent_meta_dir "${_vincent_binary_dir}/${_vincent_config}/meta_types")
        set(_vincent_meta_file "${_vincent_meta_dir}/qt6${_vincent_target_lower}_metatypes.json")
        file(MAKE_DIRECTORY "${_vincent_meta_dir}")
        if(EXISTS "${_vincent_meta_file}")
            file(SIZE "${_vincent_meta_file}" _vincent_meta_size)
            if(_vincent_meta_size EQUAL 0)
                file(WRITE "${_vincent_meta_file}" "[]\n")
            endif()
        else()
            file(WRITE "${_vincent_meta_file}" "[]\n")
        endif()
    endforeach()
else()
    if(CMAKE_BUILD_TYPE)
        string(TOLOWER "Vincent_${CMAKE_BUILD_TYPE}" _vincent_target_lower)
    else()
        string(TOLOWER "Vincent" _vincent_target_lower)
    endif()
    set(_vincent_meta_dir "${_vincent_binary_dir}/meta_types")
    set(_vincent_meta_file "${_vincent_meta_dir}/qt6${_vincent_target_lower}_metatypes.json")
    file(MAKE_DIRECTORY "${_vincent_meta_dir}")
    if(EXISTS "${_vincent_meta_file}")
        file(SIZE "${_vincent_meta_file}" _vincent_meta_size)
        if(_vincent_meta_size EQUAL 0)
            file(WRITE "${_vincent_meta_file}" "[]\n")
        endif()
    else()
        file(WRITE "${_vincent_meta_file}" "[]\n")
    endif()
endif()
#unset(_vincent_meta_dir _vincent_meta_file _vincent_meta_size _vincent_binary_dir _vincent_multi_config _vincent_config _vincent_config_lower _vincent_target_lower)

# 링크
target_link_libraries(Vincent PRIVATE
        Qt6::Core Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Svg
)

set(VINCENT_APP_ICON "${CMAKE_SOURCE_DIR}/resources/icon.icns")
get_filename_component(_vincent_icon_abs "${VINCENT_APP_ICON}" ABSOLUTE)
if(EXISTS "${_vincent_icon_abs}")
    get_filename_component(_vincent_icon_name "${_vincent_icon_abs}" NAME)
    target_sources(Vincent PRIVATE "${_vincent_icon_abs}")
    set_source_files_properties("${_vincent_icon_abs}" PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources")
    set(_vincent_icon_bundle_name "${_vincent_icon_name}")
endif()

# 번들 식별자 및 Info.plist 연결
set(MAC_INFO_PLIST_TEMPLATE "${CMAKE_SOURCE_DIR}/packaging/macos/Info.plist")
set(MAC_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/VincentInfo.plist")
configure_file("${MAC_INFO_PLIST_TEMPLATE}" "${MAC_INFO_PLIST}" @ONLY)
set_target_properties(Vincent PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${MAC_INFO_PLIST}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "${BUNDLE_ID}"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${BUNDLE_ID}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${VINCENT_BUNDLE_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
)

if(APPLE)
    set_target_properties(Vincent PROPERTIES OUTPUT_NAME "Vincent")
endif()

if(DEFINED _vincent_icon_bundle_name)
    set_target_properties(Vincent PROPERTIES MACOSX_BUNDLE_ICON_FILE "${_vincent_icon_bundle_name}")
endif()

# 설치 규칙: /Applications/Vincent.app
install(TARGETS Vincent BUNDLE DESTINATION "Applications" COMPONENT Runtime)

# Linux install target (runtime binary)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    install(TARGETS Vincent RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime)
endif()

# CPack install 단계에서 Qt 런타임을 함께 배치해 패키지를 독립 실행 가능 상태로 만든다.
if(APPLE)
    install(CODE
            "include(\"${QT_DEPLOY_SUPPORT}\")\nqt6_deploy_runtime_dependencies(\n    EXECUTABLE \"Applications/Vincent.app\"\n    DEPLOY_TOOL_OPTIONS \"-qmldir=${CMAKE_SOURCE_DIR}/App/qml\"\n)\n"
            COMPONENT Runtime)
endif()

# CPack 구성: productbuild로 단일 컴포넌트 .pkg 생성
set(CPACK_GENERATOR "productbuild")
set(CPACK_PACKAGE_NAME "Vincent")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "Vincent-${CPACK_PACKAGE_VERSION}-build${VINCENT_BUNDLE_VERSION}")

# 설치 경로: .app을 /Applications로 설치
set(CPACK_PACKAGING_INSTALL_PREFIX "/")  # 루트 기준
set(CPACK_PRODUCTBUILD_INSTALLER_DIRECTORY "/") # 내부적으로 사용
set(CPACK_PRODUCTBUILD_RESOURCES "${CMAKE_SOURCE_DIR}/packaging/macos/resources")

# 식별자
set(CPACK_PRODUCTBUILD_IDENTIFIER "com.iisacc.vincent.painter")

# 서명 신원: 기본은 무서명으로 두고, 필요한 경우에만 명시한다.
set(VINCENT_CPACK_SIGNING_IDENTITY "" CACHE STRING "Installer signing identity for CPack productbuild/pkgbuild")
if(VINCENT_CPACK_SIGNING_IDENTITY)
    set(CPACK_PKGBUILD_IDENTITY_NAME "${VINCENT_CPACK_SIGNING_IDENTITY}")
    set(CPACK_PRODUCTBUILD_IDENTITY_NAME "${VINCENT_CPACK_SIGNING_IDENTITY}")
endif()

# 단일 컴포넌트로 .app을 패키지화
set(CPACK_COMPONENTS_ALL Runtime)
set(CPACK_COMPONENT_RUNTIME_DISPLAY_NAME "Vincent")
set(CPACK_COMPONENT_RUNTIME_DESCRIPTION "Vincent ${PROJECT_VERSION} Application")
set(CPACK_COMPONENT_RUNTIME_REQUIRED TRUE)
set(CPACK_COMPONENT_RUNTIME_DEPENDS "")

set(CPACK_COMPONENT_RUNTIME_PACKAGE_TYPE "component")
set(CPACK_COMPONENT_RUNTIME_INSTALL_PATH "/Applications")
set(CPACK_BUNDLE_NAME "Vincent")
if(VINCENT_CPACK_SIGNING_IDENTITY)
    set(CPACK_BUNDLE_APPLE_CERT_APP "${VINCENT_CPACK_SIGNING_IDENTITY}")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CPACK_GENERATOR "TGZ")
    set(CPACK_PACKAGE_FILE_NAME "Vincent-${CPACK_PACKAGE_VERSION}-Linux")
    set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
endif()

include(CPack)

# Qt의 수동 마무리 훅
qt_finalize_executable(Vincent)

# 패키징·서명과 무관한 기본 요약
feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
