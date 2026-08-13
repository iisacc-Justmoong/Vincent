cmake_minimum_required(VERSION 3.24)

set(_vincent_required_build_dir "${CMAKE_SOURCE_DIR}/build")
set(_vincent_actual_build_dir "${CMAKE_BINARY_DIR}")
cmake_path(ABSOLUTE_PATH _vincent_required_build_dir NORMALIZE)
cmake_path(ABSOLUTE_PATH _vincent_actual_build_dir NORMALIZE)
if(NOT _vincent_actual_build_dir STREQUAL _vincent_required_build_dir)
    message(FATAL_ERROR
            "Vincent only supports the repository-local build/ directory. "
            "Reconfigure with: cmake -S ${CMAKE_SOURCE_DIR} -B ${_vincent_required_build_dir}")
endif()
unset(_vincent_actual_build_dir)
unset(_vincent_required_build_dir)

set(VINCENT_MIN_MACOS_VERSION "12.0")
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "${VINCENT_MIN_MACOS_VERSION}" CACHE STRING "Minimum macOS version" FORCE)
endif()

project(Vincent VERSION 4.0.5 LANGUAGES C CXX)

set(BUNDLE_ID "com.iisacc.vincent.painter")
set(VINCENT_QML_MODULE_MAJOR "${PROJECT_VERSION_MAJOR}")
# QML import minor는 API revision 없이는 0으로 유지하는 편이 안전하다.
set(VINCENT_QML_MODULE_MINOR "0")
set(VINCENT_QML_MODULE_VERSION "${VINCENT_QML_MODULE_MAJOR}.${VINCENT_QML_MODULE_MINOR}")

include(CTest)

set(VINCENT_BUNDLE_VERSION "${PROJECT_VERSION}")
if(NOT VINCENT_BUNDLE_VERSION MATCHES "^[0-9]+(\\.[0-9]+)?(\\.[0-9]+)?$")
    message(FATAL_ERROR "VINCENT_BUNDLE_VERSION must be one to three dot-separated numeric components")
endif()
message(STATUS "Vincent versions: marketing=${PROJECT_VERSION}, bundle=${VINCENT_BUNDLE_VERSION}, qml=${VINCENT_QML_MODULE_VERSION}")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

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

# 로컬 LVRS 설치 경로 힌트(.local) 우선 반영
set(_vincent_lvrs_prefix "$ENV{HOME}/.local/LVRS")
if(EXISTS "${_vincent_lvrs_prefix}/LVRSConfig.cmake")
    list(FIND CMAKE_PREFIX_PATH "${_vincent_lvrs_prefix}" _vincent_lvrs_index)
    if(_vincent_lvrs_index EQUAL -1)
        list(PREPEND CMAKE_PREFIX_PATH "${_vincent_lvrs_prefix}")
    endif()
    unset(_vincent_lvrs_index)
endif()
unset(_vincent_lvrs_prefix)

# 로컬 iiPaintEngine 설치 경로 힌트(.local) 우선 반영
set(_vincent_iipaintengine_prefix "$ENV{HOME}/.local/iiPaintEngine")
if(EXISTS "${_vincent_iipaintengine_prefix}/lib/cmake/iiPaintEngine/iiPaintEngineConfig.cmake")
    list(FIND CMAKE_PREFIX_PATH "${_vincent_iipaintengine_prefix}" _vincent_iipaintengine_index)
    if(_vincent_iipaintengine_index EQUAL -1)
        list(PREPEND CMAKE_PREFIX_PATH "${_vincent_iipaintengine_prefix}")
    endif()
    unset(_vincent_iipaintengine_index)
endif()
unset(_vincent_iipaintengine_prefix)

# 요구 버전
set(QT_MIN_VERSION "6.8.0")

# 기본 설치 경로 및 요약
include(GNUInstallDirs)
include(FeatureSummary)
include(FetchContent)

# Qt6
find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Network Qml Quick QuickControls2 Svg)
if(BUILD_TESTING)
    find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Test)
endif()
find_package(LVRS CONFIG REQUIRED)
find_package(iiPaintEngine CONFIG REQUIRED)

if(APPLE OR WIN32)
    FetchContent_Declare(qtkeychain
            GIT_REPOSITORY https://github.com/frankosterfeld/qtkeychain.git
            GIT_TAG 875f77d9f61bd97fd84cca47ce3bc71186dfbd09
            GIT_SHALLOW FALSE
            UPDATE_DISCONNECTED TRUE
    )
    function(vincent_add_qtkeychain)
        set(BUILD_WITH_QT5 OFF)
        set(BUILD_TEST_APPLICATION OFF)
        set(BUILD_QTQUICK_DEMO OFF)
        set(BUILD_TRANSLATIONS OFF)
        set(BUILD_SHARED_LIBS OFF)
        set(BUILD_TESTING OFF)
        set(USE_CREDENTIAL_STORE ON)
        set(USE_COMPAT_NAMING_SCHEME OFF)
        FetchContent_MakeAvailable(qtkeychain)
    endfunction()
    vincent_add_qtkeychain()
endif()

FetchContent_Declare(psd_sdk
        GIT_REPOSITORY https://github.com/MolecularMatters/psd_sdk.git
        GIT_TAG f51449543273cbf12058ae92b230e0c4209f5066
        UPDATE_DISCONNECTED TRUE
)
FetchContent_GetProperties(psd_sdk)
if(NOT psd_sdk_POPULATED)
    if(POLICY CMP0169)
        cmake_policy(PUSH)
        cmake_policy(SET CMP0169 OLD)
    endif()
    FetchContent_Populate(psd_sdk)
    if(POLICY CMP0169)
        cmake_policy(POP)
    endif()
endif()
file(GLOB _vincent_psd_sdk_sources CONFIGURE_DEPENDS
        "${psd_sdk_SOURCE_DIR}/src/Psd/*.cpp"
        "${psd_sdk_SOURCE_DIR}/src/Psd/*.c"
)
list(REMOVE_ITEM _vincent_psd_sdk_sources
        "${psd_sdk_SOURCE_DIR}/src/Psd/PsdNativeFile.cpp"
        "${psd_sdk_SOURCE_DIR}/src/Psd/PsdNativeFile_Linux.cpp"
        "${psd_sdk_SOURCE_DIR}/src/Psd/PsdStringUtil.cpp"
)
set_source_files_properties("${psd_sdk_SOURCE_DIR}/src/Psd/Psdminiz.c" PROPERTIES LANGUAGE CXX)
add_library(vincent_psd_sdk STATIC ${_vincent_psd_sdk_sources})
target_include_directories(vincent_psd_sdk PUBLIC "${psd_sdk_SOURCE_DIR}/src/Psd")
target_compile_features(vincent_psd_sdk PUBLIC cxx_std_11)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(vincent_psd_sdk PRIVATE -Wno-pragmas)
endif()
lvrs_apply_platform_build_optimizations(vincent_psd_sdk)

set(_vincent_local_dependency_runtime_paths)
set(_vincent_local_dependency_runtime_candidates)
if(APPLE)
    list(APPEND _vincent_local_dependency_runtime_candidates
            "$ENV{HOME}/.local/LVRS/platforms/macos/lib"
            "$ENV{HOME}/.local/iiPaintEngine/platforms/macos/lib")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND _vincent_local_dependency_runtime_candidates
            "$ENV{HOME}/.local/LVRS/platforms/linux/lib"
            "$ENV{HOME}/.local/iiPaintEngine/platforms/linux/lib")
endif()
if(UNIX AND NOT APPLE)
    list(APPEND _vincent_local_dependency_runtime_candidates
            "$ENV{HOME}/.local/LVRS/lib"
            "$ENV{HOME}/.local/iiPaintEngine/lib")
endif()
foreach(_vincent_runtime_path IN LISTS _vincent_local_dependency_runtime_candidates)
    if(IS_DIRECTORY "${_vincent_runtime_path}")
        list(APPEND _vincent_local_dependency_runtime_paths "${_vincent_runtime_path}")
    endif()
endforeach()
list(REMOVE_DUPLICATES _vincent_local_dependency_runtime_paths)
unset(_vincent_local_dependency_runtime_candidates)
unset(_vincent_runtime_path)

function(vincent_configure_local_runtime_paths target_name)
    if(_vincent_local_dependency_runtime_paths)
        set_property(TARGET ${target_name} APPEND PROPERTY
                BUILD_RPATH "${_vincent_local_dependency_runtime_paths}")
    endif()
endfunction()

qt_standard_project_setup(REQUIRES ${QT_MIN_VERSION})

# 실행 타깃
qt_add_executable(Vincent
        MANUAL_FINALIZATION
        WIN32
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
        App/qml/license/LicenseActivationPage.qml
        App/qml/canvas/PainterCanvasPage.qml
        App/qml/brush/CanvasToolBar.qml
        App/qml/brush/HslTriangleColorPicker.qml
        App/qml/painting/DrawingSurface.qml
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
        Qt6::Core Qt6::Network Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Svg
        iiPaintEngine::iiPaintEngine
        vincent_psd_sdk
)
if(APPLE OR WIN32)
    target_link_libraries(Vincent PRIVATE qt6keychain)
endif()
target_compile_definitions(Vincent PRIVATE
        VINCENT_VERSION="${PROJECT_VERSION}"
)
vincent_configure_local_runtime_paths(Vincent)

if(WIN32 AND MINGW)
    target_link_options(Vincent PRIVATE
            "$<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>,$<CONFIG:MinSizeRel>>:-Wl,--gc-sections>"
    )
endif()

set(_vincent_windows_runtime_files)
if(WIN32 AND MINGW)
    get_filename_component(_vincent_mingw_runtime_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    foreach(_vincent_mingw_runtime_name IN ITEMS
            libgcc_s_seh-1.dll
            libstdc++-6.dll
            libwinpthread-1.dll)
        string(MAKE_C_IDENTIFIER "${_vincent_mingw_runtime_name}" _vincent_mingw_runtime_var)
        find_file("${_vincent_mingw_runtime_var}"
                NAMES "${_vincent_mingw_runtime_name}"
                PATHS "${_vincent_mingw_runtime_dir}"
                NO_DEFAULT_PATH
                NO_CACHE)
        if(NOT ${_vincent_mingw_runtime_var})
            message(FATAL_ERROR
                    "MinGW runtime ${_vincent_mingw_runtime_name} was not found next to ${CMAKE_CXX_COMPILER}")
        endif()
        list(APPEND _vincent_windows_runtime_files "${${_vincent_mingw_runtime_var}}")
        unset(_vincent_mingw_runtime_var)
    endforeach()
    unset(_vincent_mingw_runtime_dir)
endif()

if(WIN32)
    add_custom_command(TARGET Vincent POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    $<TARGET_FILE:LVRS::LVRS>
                    $<TARGET_FILE:iiPaintEngine::iiPaintEngine>
                    ${_vincent_windows_runtime_files}
                    $<TARGET_FILE_DIR:Vincent>
            COMMAND_EXPAND_LISTS
            COMMENT "Copying Windows runtime DLLs next to Vincent.exe")
endif()
unset(_vincent_windows_runtime_files)

# LVRS QML 모듈 링크 및 import 경로/정적 플러그인 자동 처리
set_target_properties(Vincent PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)
lvrs_configure_qml_app(Vincent NO_PLATFORM_RUNTIME_TARGETS)
if(WIN32 AND MINGW)
    # MinGW 13's LTO plugin is not propagated consistently through Ninja's
    # archive and generated-QML steps. Keep the native Release optimization
    # flags and section GC, but do not emit fragile LTO objects on this toolchain.
    set_target_properties(vincent_psd_sdk Vincent PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION_RELEASE FALSE
            INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO FALSE
            INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL FALSE
    )
endif()

set(VINCENT_MACOS_APP_ICON "${CMAKE_SOURCE_DIR}/resources/Appicon.icns")
set(VINCENT_WINDOWS_APP_ICON "${CMAKE_SOURCE_DIR}/resources/Appicon.ico")
set(VINCENT_WINDOWS_MANIFEST_TEMPLATE "${CMAKE_SOURCE_DIR}/resources/windows/Vincent.manifest.in")
set(VINCENT_WINDOWS_RESOURCE_TEMPLATE "${CMAKE_SOURCE_DIR}/resources/windows/Vincent.rc.in")
if(WIN32)
    string(REPLACE "." ";" _vincent_windows_version_components "${PROJECT_VERSION}")
    list(LENGTH _vincent_windows_version_components _vincent_windows_version_component_count)
    while(_vincent_windows_version_component_count LESS 4)
        list(APPEND _vincent_windows_version_components "0")
        list(LENGTH _vincent_windows_version_components _vincent_windows_version_component_count)
    endwhile()
    list(JOIN _vincent_windows_version_components "." VINCENT_WINDOWS_VERSION_DOTTED)
    list(JOIN _vincent_windows_version_components "," VINCENT_WINDOWS_VERSION_COMMA)
    unset(_vincent_windows_version_component_count)
    unset(_vincent_windows_version_components)
endif()

if(APPLE)
    get_filename_component(VINCENT_MACOS_APP_ICON_FILE "${VINCENT_MACOS_APP_ICON}" NAME)
    set(VINCENT_LEGACY_MACOS_APP_ICON_FILE "icon.icns")
    set(VINCENT_QTKEYCHAIN_LICENSE
            "${CMAKE_SOURCE_DIR}/packaging/common/licenses/QtKeychain-BSD-3-Clause.txt")
    set_source_files_properties("${VINCENT_QTKEYCHAIN_LICENSE}" PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources/legal/QtKeychain")
    target_sources(Vincent PRIVATE "${VINCENT_QTKEYCHAIN_LICENSE}")
endif()

if(APPLE)
    if(NOT EXISTS "${VINCENT_MACOS_APP_ICON}")
        message(FATAL_ERROR "macOS app icon is missing: ${VINCENT_MACOS_APP_ICON}")
    endif()

    target_sources(Vincent PRIVATE "${VINCENT_MACOS_APP_ICON}")
    set_source_files_properties("${VINCENT_MACOS_APP_ICON}" PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources")
elseif(WIN32)
    if(NOT EXISTS "${VINCENT_WINDOWS_APP_ICON}")
        message(FATAL_ERROR "Windows app icon is missing: ${VINCENT_WINDOWS_APP_ICON}")
    endif()
    if(NOT EXISTS "${VINCENT_WINDOWS_MANIFEST_TEMPLATE}")
        message(FATAL_ERROR "Windows application manifest template is missing: ${VINCENT_WINDOWS_MANIFEST_TEMPLATE}")
    endif()
    if(NOT EXISTS "${VINCENT_WINDOWS_RESOURCE_TEMPLATE}")
        message(FATAL_ERROR "Windows resource template is missing: ${VINCENT_WINDOWS_RESOURCE_TEMPLATE}")
    endif()

    set(_vincent_windows_manifest_file "${CMAKE_CURRENT_BINARY_DIR}/Vincent.manifest")
    configure_file("${VINCENT_WINDOWS_MANIFEST_TEMPLATE}"
            "${_vincent_windows_manifest_file}" @ONLY)
    file(TO_NATIVE_PATH "${_vincent_windows_manifest_file}" _vincent_windows_manifest_native)
    string(REPLACE "\\" "\\\\" VINCENT_WINDOWS_MANIFEST_RC
            "${_vincent_windows_manifest_native}")
    file(TO_NATIVE_PATH "${VINCENT_WINDOWS_APP_ICON}" _vincent_windows_app_icon_native)
    string(REPLACE "\\" "\\\\" VINCENT_WINDOWS_APP_ICON_RC
            "${_vincent_windows_app_icon_native}")

    set(_vincent_windows_resource_file "${CMAKE_CURRENT_BINARY_DIR}/Vincent.rc")
    configure_file("${VINCENT_WINDOWS_RESOURCE_TEMPLATE}"
            "${_vincent_windows_resource_file}" @ONLY)
    set_property(SOURCE "${_vincent_windows_resource_file}" APPEND PROPERTY
            OBJECT_DEPENDS "${_vincent_windows_manifest_file}")
    target_sources(Vincent PRIVATE "${_vincent_windows_resource_file}")
endif()

# 번들 식별자 및 Info.plist 연결
if(APPLE)
    set(MAC_INFO_PLIST_TEMPLATE "${CMAKE_SOURCE_DIR}/packaging/macos/Info.plist")
    set(MAC_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/VincentInfo.plist")
    configure_file("${MAC_INFO_PLIST_TEMPLATE}" "${MAC_INFO_PLIST}" @ONLY)
    set_target_properties(Vincent PROPERTIES
            OUTPUT_NAME "Vincent"
            MACOSX_BUNDLE_INFO_PLIST "${MAC_INFO_PLIST}"
            MACOSX_BUNDLE_GUI_IDENTIFIER "${BUNDLE_ID}"
            XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${BUNDLE_ID}"
            MACOSX_BUNDLE_BUNDLE_VERSION "${VINCENT_BUNDLE_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )
    set_target_properties(Vincent PROPERTIES MACOSX_BUNDLE_ICON_FILE "${VINCENT_MACOS_APP_ICON_FILE}")
    add_custom_command(TARGET Vincent POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E rm -f
                    "$<TARGET_BUNDLE_DIR:Vincent>/Contents/Resources/${VINCENT_LEGACY_MACOS_APP_ICON_FILE}"
            COMMENT "Removing legacy macOS app icon from Vincent bundle")
endif()

# 설치 규칙
if(APPLE)
    install(TARGETS Vincent BUNDLE DESTINATION "Applications" COMPONENT Runtime)
elseif(WIN32)
    install(TARGETS Vincent RUNTIME DESTINATION "." COMPONENT Runtime)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set_property(TARGET Vincent PROPERTY
            INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
    install(TARGETS Vincent RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime)
    install(FILES "${CMAKE_SOURCE_DIR}/packaging/linux/com.iisacc.vincent.painter.desktop"
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications"
            COMPONENT Runtime)
    install(FILES "${CMAKE_SOURCE_DIR}/packaging/macos/Vincent.xcassets/AppIcon.appiconset/icon_256x256.png"
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/256x256/apps"
            RENAME "com.iisacc.vincent.painter.png"
            COMPONENT Runtime)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# CPack install 단계에서 Qt 런타임을 함께 배치해 패키지를 독립 실행 가능 상태로 만든다.
if(APPLE)
    install(CODE
            "include(\"${QT_DEPLOY_SUPPORT}\")\nqt6_deploy_runtime_dependencies(\n    EXECUTABLE \"Applications/Vincent.app\"\n    DEPLOY_TOOL_OPTIONS \"-qmldir=${CMAKE_SOURCE_DIR}/App/qml\"\n)\n"
            COMPONENT Runtime)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    qt_generate_deploy_qml_app_script(
            TARGET Vincent
            OUTPUT_SCRIPT _vincent_linux_deploy_script
    )
    install(SCRIPT "${_vincent_linux_deploy_script}" COMPONENT Runtime)
endif()

# CPack 구성
set(CPACK_PACKAGE_NAME "Vincent")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
if(APPLE)
    # productbuild로 단일 컴포넌트 .pkg 생성
    set(CPACK_GENERATOR "productbuild")
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
elseif(WIN32)
    set(CPACK_GENERATOR "ZIP")
    # CPack does not run windeployqt, PE closure checks, or Authenticode signing.
    # Keep its output unmistakably separate from build-windows.ps1 release artifacts.
    set(CPACK_PACKAGE_FILE_NAME "Vincent-${CPACK_PACKAGE_VERSION}-Windows-unsigned-cpack-incomplete")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CPACK_GENERATOR "TGZ")
    set(CPACK_PACKAGE_FILE_NAME "Vincent-${CPACK_PACKAGE_VERSION}-Linux")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/")
else()
    set(CPACK_GENERATOR "TGZ")
    set(CPACK_PACKAGE_FILE_NAME "Vincent-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}")
    set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
endif()

# 단일 컴포넌트로 .app을 패키지화
set(CPACK_COMPONENTS_ALL Runtime)
set(CPACK_COMPONENT_RUNTIME_DISPLAY_NAME "Vincent")
set(CPACK_COMPONENT_RUNTIME_DESCRIPTION "Vincent ${PROJECT_VERSION} Application")
set(CPACK_COMPONENT_RUNTIME_REQUIRED TRUE)
set(CPACK_COMPONENT_RUNTIME_DEPENDS "")

if(APPLE)
    set(CPACK_COMPONENT_RUNTIME_PACKAGE_TYPE "component")
    set(CPACK_COMPONENT_RUNTIME_INSTALL_PATH "/Applications")
    set(CPACK_BUNDLE_NAME "Vincent")
    if(VINCENT_CPACK_SIGNING_IDENTITY)
        set(CPACK_BUNDLE_APPLE_CERT_APP "${VINCENT_CPACK_SIGNING_IDENTITY}")
    endif()
endif()

include(CPack)

# Qt의 수동 마무리 훅
qt_finalize_executable(Vincent)

# 패키징·서명과 무관한 기본 요약
feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
