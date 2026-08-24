add_executable(tests_canvasdocumentviewmodel
        tst_canvasdocumentviewmodel.cpp
        ../App/models/canvas/canvasdocumentviewmodel.cpp
        ../App/models/canvas/canvasdocumentviewmodel.h
        ../App/models/brush/paletteutils.cpp
        ../App/models/brush/paletteutils.h
)

target_include_directories(tests_canvasdocumentviewmodel PRIVATE
        ${CMAKE_SOURCE_DIR}/App
        ${CMAKE_SOURCE_DIR}/App/models/brush
        ${CMAKE_SOURCE_DIR}/App/models/canvas
)

target_link_libraries(tests_canvasdocumentviewmodel PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Test
)
vincent_configure_local_runtime_paths(tests_canvasdocumentviewmodel)

add_test(NAME tests_canvasdocumentviewmodel COMMAND tests_canvasdocumentviewmodel)

add_executable(tests_psdcompatibilitydocument
        tst_psdcompatibilitydocument.cpp
        ../App/models/document/psdcompatibilitydocument.cpp
        ../App/models/document/psdcompatibilitydocument.h
)

target_include_directories(tests_psdcompatibilitydocument PRIVATE
        ${CMAKE_SOURCE_DIR}/App/models/document
)

target_link_libraries(tests_psdcompatibilitydocument PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Test
)
vincent_configure_local_runtime_paths(tests_psdcompatibilitydocument)

add_test(NAME tests_psdcompatibilitydocument COMMAND tests_psdcompatibilitydocument)

add_executable(tests_canvastoolbarqmlcontract
        tst_canvastoolbarqmlcontract.cpp
)

target_link_libraries(tests_canvastoolbarqmlcontract PRIVATE
        Qt6::Core
        Qt6::Test
)
vincent_configure_local_runtime_paths(tests_canvastoolbarqmlcontract)

add_test(NAME tests_canvastoolbarqmlcontract COMMAND tests_canvastoolbarqmlcontract)

add_executable(tests_mainqmlcontract
        tst_mainqmlcontract.cpp
)

target_link_libraries(tests_mainqmlcontract PRIVATE
        Qt6::Core
        Qt6::Test
)
vincent_configure_local_runtime_paths(tests_mainqmlcontract)

add_test(NAME tests_mainqmlcontract COMMAND tests_mainqmlcontract)

add_executable(tests_memberprofilelistbuilder
        tst_memberprofilelistbuilder.cpp
        ../App/models/collaboration/memberprofilelistbuilder.cpp
        ../App/models/collaboration/memberprofilelistbuilder.h
)

target_include_directories(tests_memberprofilelistbuilder PRIVATE
        ${CMAKE_SOURCE_DIR}/App/models/collaboration
)

target_link_libraries(tests_memberprofilelistbuilder PRIVATE
        Qt6::Core
        Qt6::Test
)
vincent_configure_local_runtime_paths(tests_memberprofilelistbuilder)

add_test(NAME tests_memberprofilelistbuilder COMMAND tests_memberprofilelistbuilder)

add_executable(tests_profileimageprocessor
        tst_profileimageprocessor.cpp
        ../App/models/profile/profileimageprocessor.cpp
        ../App/models/profile/profileimageprocessor.h
)

target_include_directories(tests_profileimageprocessor PRIVATE
        ${CMAKE_SOURCE_DIR}/App/models/profile
)

target_link_libraries(tests_profileimageprocessor PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Test
)
vincent_configure_local_runtime_paths(tests_profileimageprocessor)

add_test(NAME tests_profileimageprocessor COMMAND tests_profileimageprocessor)

add_executable(tests_applicationpreferences
        tst_applicationpreferences.cpp
        ../App/models/preferences/applicationpreferences.cpp
        ../App/models/preferences/applicationpreferences.h
)

target_include_directories(tests_applicationpreferences PRIVATE
        ${CMAKE_SOURCE_DIR}/App/models/preferences
)

target_link_libraries(tests_applicationpreferences PRIVATE
        Qt6::Core
        Qt6::Test
)
vincent_configure_local_runtime_paths(tests_applicationpreferences)

add_test(NAME tests_applicationpreferences COMMAND tests_applicationpreferences)

add_executable(tests_nearbyvincentdiscovery
        tst_nearbyvincentdiscovery.cpp
        ../App/models/network/nearbyvincentdiscovery.cpp
        ../App/models/network/nearbyvincentdiscovery.h
)

target_include_directories(tests_nearbyvincentdiscovery PRIVATE
        ${CMAKE_SOURCE_DIR}/App/models/network
)

target_link_libraries(tests_nearbyvincentdiscovery PRIVATE
        Qt6::Core
        Qt6::Network
        Qt6::Test
)
vincent_configure_local_runtime_paths(tests_nearbyvincentdiscovery)

add_test(NAME tests_nearbyvincentdiscovery COMMAND tests_nearbyvincentdiscovery)

add_executable(tests_localcanvassession
        tst_localcanvassession.cpp
        ../App/models/document/recentcanvascontainer.cpp
        ../App/models/document/recentcanvascontainer.h
        ../App/models/network/localcanvassession.cpp
        ../App/models/network/localcanvassession.h
        ../App/models/network/nearbyvincentdiscovery.cpp
        ../App/models/network/nearbyvincentdiscovery.h
)

target_include_directories(tests_localcanvassession PRIVATE
        ${CMAKE_SOURCE_DIR}/App/models/document
        ${CMAKE_SOURCE_DIR}/App/models/network
)

target_link_libraries(tests_localcanvassession PRIVATE
        Qt6::Core
        Qt6::Network
        Qt6::Test
)
vincent_configure_local_runtime_paths(tests_localcanvassession)

add_test(NAME tests_localcanvassession COMMAND tests_localcanvassession)

add_executable(tests_temporarycamerainput
        tst_temporarycamerainput.cpp
        ../App/models/input/temporarycamerainput.cpp
        ../App/models/input/temporarycamerainput.h
)

target_include_directories(tests_temporarycamerainput PRIVATE
        ${CMAKE_SOURCE_DIR}/App/models/input
)

target_link_libraries(tests_temporarycamerainput PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Test
)
vincent_configure_local_runtime_paths(tests_temporarycamerainput)

add_test(NAME tests_temporarycamerainput COMMAND tests_temporarycamerainput)

add_executable(tests_licensemanager
        tst_licensemanager.cpp
        ../App/models/license/accountmanager.cpp
        ../App/models/license/accountmanager.h
        ../App/models/license/licensecredentialstore.cpp
        ../App/models/license/licensecredentialstore.h
        ../App/models/license/licensemanager.cpp
        ../App/models/license/licensemanager.h
)

target_include_directories(tests_licensemanager PRIVATE
        ${CMAKE_SOURCE_DIR}/App/models/license
)

target_link_libraries(tests_licensemanager PRIVATE
        Qt6::Core
        Qt6::Network
        Qt6::Test
        iiLicenseManager::iiLicenseManager
)
if(APPLE OR WIN32)
    target_link_libraries(tests_licensemanager PRIVATE qt6keychain)
endif()
vincent_configure_local_runtime_paths(tests_licensemanager)

add_test(NAME tests_licensemanager COMMAND tests_licensemanager)

add_executable(tests_vincentupdatemanager
        tst_vincentupdatemanager.cpp
        ../App/models/license/licensecredentialstore.cpp
        ../App/models/license/licensecredentialstore.h
        ../App/models/license/licensemanager.cpp
        ../App/models/license/licensemanager.h
        ../App/models/update/vincentupdatecredentialprovider.cpp
        ../App/models/update/vincentupdatecredentialprovider.h
        ../App/models/update/vincentupdatemanager.cpp
        ../App/models/update/vincentupdatemanager.h
)

target_include_directories(tests_vincentupdatemanager PRIVATE
        ${CMAKE_SOURCE_DIR}/App/models/license
        ${CMAKE_SOURCE_DIR}/App/models/update
)

target_link_libraries(tests_vincentupdatemanager PRIVATE
        Qt6::Core
        Qt6::Network
        Qt6::Test
        iiUpdateManager::iiUpdateManager
)
if(APPLE OR WIN32)
    target_link_libraries(tests_vincentupdatemanager PRIVATE qt6keychain)
endif()
vincent_configure_local_runtime_paths(tests_vincentupdatemanager)

add_test(NAME tests_vincentupdatemanager COMMAND tests_vincentupdatemanager)

add_executable(tests_macosbuildworkflowcontract
        tst_macosbuildworkflowcontract.cpp
)

target_link_libraries(tests_macosbuildworkflowcontract PRIVATE
        Qt6::Core
        Qt6::Test
)
vincent_configure_local_runtime_paths(tests_macosbuildworkflowcontract)

add_test(NAME tests_macosbuildworkflowcontract COMMAND tests_macosbuildworkflowcontract)

add_executable(tests_linuxbuildworkflowcontract
        tst_linuxbuildworkflowcontract.cpp
)

target_link_libraries(tests_linuxbuildworkflowcontract PRIVATE
        Qt6::Core
        Qt6::Test
)
vincent_configure_local_runtime_paths(tests_linuxbuildworkflowcontract)

add_test(NAME tests_linuxbuildworkflowcontract COMMAND tests_linuxbuildworkflowcontract)

add_executable(tests_windowsbuildworkflowcontract
        tst_windowsbuildworkflowcontract.cpp
)

target_link_libraries(tests_windowsbuildworkflowcontract PRIVATE
        Qt6::Core
        Qt6::Test
)
vincent_configure_local_runtime_paths(tests_windowsbuildworkflowcontract)

add_test(NAME tests_windowsbuildworkflowcontract COMMAND tests_windowsbuildworkflowcontract)

if(WIN32)
    add_executable(tests_windowsbinarycontract
            tst_windowsbinarycontract.cpp
    )

    target_compile_definitions(tests_windowsbinarycontract PRIVATE
            VINCENT_EXECUTABLE_PATH="$<TARGET_FILE:Vincent>"
    )
    target_link_libraries(tests_windowsbinarycontract PRIVATE
            Qt6::Core
            Qt6::Test
            version
    )
    add_dependencies(tests_windowsbinarycontract Vincent)
    vincent_configure_local_runtime_paths(tests_windowsbinarycontract)

    add_test(NAME tests_windowsbinarycontract COMMAND tests_windowsbinarycontract)
endif()

add_executable(tests_drawingsurfaceitem
        tst_drawingsurfaceitem.cpp
        ../App/models/brush/paletteutils.cpp
        ../App/models/brush/paletteutils.h
        ../App/models/canvas/canvasdocumentviewmodel.cpp
        ../App/models/canvas/canvasdocumentviewmodel.h
        ../App/models/canvas/canvasviewmodelbridge.cpp
        ../App/models/canvas/canvasviewmodelbridge.h
        ../App/models/document/psdcompatibilitydocument.cpp
        ../App/models/document/psdcompatibilitydocument.h
        ../App/models/document/psdimagereader.cpp
        ../App/models/document/psdimagereader.h
        ../App/models/document/psdimagewriter.cpp
        ../App/models/document/psdimagewriter.h
        ../App/models/document/recentcanvascontainer.cpp
        ../App/models/document/recentcanvascontainer.h
        ../App/models/painting/drawingsurfaceitem.cpp
        ../App/models/painting/drawingsurfaceitem.h
)

target_include_directories(tests_drawingsurfaceitem PRIVATE
        ${CMAKE_SOURCE_DIR}/App
        ${CMAKE_SOURCE_DIR}/App/models/brush
        ${CMAKE_SOURCE_DIR}/App/models/canvas
        ${CMAKE_SOURCE_DIR}/App/models/document
        ${CMAKE_SOURCE_DIR}/App/models/painting
)

target_link_libraries(tests_drawingsurfaceitem PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::Network
        Qt6::Test
        iiPaintEngine::iiPaintEngine
        iiSharedCanvas::iiSharedCanvas
        vincent_psd_sdk
)
vincent_configure_local_runtime_paths(tests_drawingsurfaceitem)

add_test(NAME tests_drawingsurfaceitem COMMAND tests_drawingsurfaceitem)

set(_vincent_test_environment
        "CMAKE_PREFIX_PATH="
        "QML2_IMPORT_PATH="
        "QTDIR="
        "QT_PLUGIN_PATH="
        "QT_PREFIX="
        "QT_QML_IMPORT_PATH="
)

set_tests_properties(
        tests_canvasdocumentviewmodel
        tests_psdcompatibilitydocument
        tests_canvastoolbarqmlcontract
        tests_mainqmlcontract
        tests_memberprofilelistbuilder
        tests_profileimageprocessor
        tests_applicationpreferences
        tests_nearbyvincentdiscovery
        tests_temporarycamerainput
        tests_licensemanager
        tests_vincentupdatemanager
        tests_macosbuildworkflowcontract
        tests_linuxbuildworkflowcontract
        tests_windowsbuildworkflowcontract
        tests_drawingsurfaceitem
        PROPERTIES
        ENVIRONMENT "${_vincent_test_environment}"
)

if(WIN32)
    find_program(_vincent_powershell_executable NAMES powershell.exe REQUIRED)
    add_test(
            NAME tests_windowsauthenticodepolicy
            COMMAND "${_vincent_powershell_executable}"
                    -NoProfile
                    -ExecutionPolicy Bypass
                    -File "${CMAKE_CURRENT_SOURCE_DIR}/tst_windowsauthenticodepolicy.ps1"
                    -ScriptPath "${CMAKE_SOURCE_DIR}/build-windows.ps1"
    )
    set_tests_properties(tests_windowsauthenticodepolicy PROPERTIES
            ENVIRONMENT "${_vincent_test_environment}"
    )

    add_test(
            NAME tests_necessaryauthenticode
            COMMAND "${_vincent_powershell_executable}"
                    -NoProfile
                    -ExecutionPolicy Bypass
                    -File "${CMAKE_CURRENT_SOURCE_DIR}/tst_necessaryauthenticode.ps1"
                    -ScriptPath "${CMAKE_SOURCE_DIR}/tools/necessary-authenticode.ps1"
    )
    set_tests_properties(tests_necessaryauthenticode PROPERTIES
            ENVIRONMENT "${_vincent_test_environment}"
    )

    add_test(
            NAME tests_necessarywindowsrelease
            COMMAND "${_vincent_powershell_executable}"
                    -NoProfile
                    -ExecutionPolicy Bypass
                    -File "${CMAKE_CURRENT_SOURCE_DIR}/tst_necessarywindowsrelease.ps1"
                    -ScriptPath "${CMAKE_SOURCE_DIR}/tools/necessary-windows-release.ps1"
    )
    set_tests_properties(tests_necessarywindowsrelease PROPERTIES
            ENVIRONMENT "${_vincent_test_environment}"
    )

    find_program(_vincent_pwsh_executable NAMES pwsh.exe)
    if(_vincent_pwsh_executable)
        add_test(
                NAME tests_windowsauthenticodepolicy_pwsh
                COMMAND "${_vincent_pwsh_executable}"
                        -NoProfile
                        -ExecutionPolicy Bypass
                        -File "${CMAKE_CURRENT_SOURCE_DIR}/tst_windowsauthenticodepolicy.ps1"
                        -ScriptPath "${CMAKE_SOURCE_DIR}/build-windows.ps1"
        )
        set_tests_properties(tests_windowsauthenticodepolicy_pwsh PROPERTIES
                ENVIRONMENT "${_vincent_test_environment}"
        )

        add_test(
                NAME tests_necessaryauthenticode_pwsh
                COMMAND "${_vincent_pwsh_executable}"
                        -NoProfile
                        -File "${CMAKE_CURRENT_SOURCE_DIR}/tst_necessaryauthenticode.ps1"
                        -ScriptPath "${CMAKE_SOURCE_DIR}/tools/necessary-authenticode.ps1"
        )
        set_tests_properties(tests_necessaryauthenticode_pwsh PROPERTIES
                ENVIRONMENT "${_vincent_test_environment}"
        )

        add_test(
                NAME tests_necessarywindowsrelease_pwsh
                COMMAND "${_vincent_pwsh_executable}"
                        -NoProfile
                        -File "${CMAKE_CURRENT_SOURCE_DIR}/tst_necessarywindowsrelease.ps1"
                        -ScriptPath "${CMAKE_SOURCE_DIR}/tools/necessary-windows-release.ps1"
        )
        set_tests_properties(tests_necessarywindowsrelease_pwsh PROPERTIES
                ENVIRONMENT "${_vincent_test_environment}"
        )
    endif()

    add_test(
            NAME tests_windowsmsiauthoringcontract
            COMMAND "${_vincent_powershell_executable}"
                    -NoProfile
                    -ExecutionPolicy Bypass
                    -File "${CMAKE_CURRENT_SOURCE_DIR}/tst_windowsmsiauthoringcontract.ps1"
                    -ScriptPath "${CMAKE_SOURCE_DIR}/build-windows.ps1"
    )
    set_tests_properties(tests_windowsmsiauthoringcontract PROPERTIES
            ENVIRONMENT "${_vincent_test_environment}"
    )

    add_test(
            NAME tests_windowsstorepackagecontract
            COMMAND "${_vincent_powershell_executable}"
                    -NoProfile
                    -ExecutionPolicy Bypass
                    -File "${CMAKE_CURRENT_SOURCE_DIR}/tst_windowsstorepackagecontract.ps1"
                    -ScriptPath "${CMAKE_SOURCE_DIR}/build-windows-store.ps1"
                    -ManifestTemplatePath "${CMAKE_SOURCE_DIR}/packaging/windows/AppxManifest.xml.in"
    )
    set_tests_properties(tests_windowsstorepackagecontract PROPERTIES
            ENVIRONMENT "${_vincent_test_environment}"
    )
    if(_vincent_pwsh_executable)
        add_test(
                NAME tests_windowsstorepackagecontract_pwsh
                COMMAND "${_vincent_pwsh_executable}"
                        -NoProfile
                        -File "${CMAKE_CURRENT_SOURCE_DIR}/tst_windowsstorepackagecontract.ps1"
                        -ScriptPath "${CMAKE_SOURCE_DIR}/build-windows-store.ps1"
                        -ManifestTemplatePath "${CMAKE_SOURCE_DIR}/packaging/windows/AppxManifest.xml.in"
        )
        set_tests_properties(tests_windowsstorepackagecontract_pwsh PROPERTIES
                ENVIRONMENT "${_vincent_test_environment}"
        )
    endif()

    set_tests_properties(tests_windowsbinarycontract PROPERTIES
            ENVIRONMENT "${_vincent_test_environment}"
    )
endif()

if(WIN32)
    set_tests_properties(
            tests_canvasdocumentviewmodel
            tests_psdcompatibilitydocument
            tests_canvastoolbarqmlcontract
            tests_mainqmlcontract
            tests_memberprofilelistbuilder
            tests_profileimageprocessor
            tests_applicationpreferences
            tests_nearbyvincentdiscovery
            tests_temporarycamerainput
            tests_licensemanager
            tests_vincentupdatemanager
            tests_macosbuildworkflowcontract
            tests_linuxbuildworkflowcontract
            tests_windowsbuildworkflowcontract
            tests_windowsauthenticodepolicy
            tests_necessaryauthenticode
            tests_necessarywindowsrelease
            tests_windowsmsiauthoringcontract
            tests_windowsstorepackagecontract
            tests_windowsbinarycontract
            tests_drawingsurfaceitem
            PROPERTIES
            ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:$<TARGET_FILE_DIR:iiPaintEngine::iiPaintEngine>;PATH=path_list_prepend:$<TARGET_FILE_DIR:iiSharedCanvas::iiSharedCanvas>;PATH=path_list_prepend:$<TARGET_FILE_DIR:iiUpdateManager::iiUpdateManager>;PATH=path_list_prepend:$<TARGET_FILE_DIR:iiLicenseManager::iiLicenseManager>;PATH=path_list_prepend:$<TARGET_FILE_DIR:LVRS::LVRS>"
    )
endif()

set_property(
        TEST tests_drawingsurfaceitem
        APPEND PROPERTY ENVIRONMENT_MODIFICATION
        "QT_QPA_PLATFORM=set:offscreen"
)
