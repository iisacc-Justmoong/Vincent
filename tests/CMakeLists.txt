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

add_executable(tests_macosbuildworkflowcontract
        tst_macosbuildworkflowcontract.cpp
)

target_link_libraries(tests_macosbuildworkflowcontract PRIVATE
        Qt6::Core
        Qt6::Test
)
vincent_configure_local_runtime_paths(tests_macosbuildworkflowcontract)

add_test(NAME tests_macosbuildworkflowcontract COMMAND tests_macosbuildworkflowcontract)

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
        Qt6::Test
        iiPaintEngine::iiPaintEngine
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
        tests_macosbuildworkflowcontract
        tests_windowsbuildworkflowcontract
        tests_drawingsurfaceitem
        PROPERTIES
        ENVIRONMENT "${_vincent_test_environment}"
)

if(WIN32)
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
            tests_macosbuildworkflowcontract
            tests_windowsbuildworkflowcontract
            tests_windowsbinarycontract
            tests_drawingsurfaceitem
            PROPERTIES
            ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:$<TARGET_FILE_DIR:iiPaintEngine::iiPaintEngine>;PATH=path_list_prepend:$<TARGET_FILE_DIR:LVRS::LVRS>"
    )
endif()
