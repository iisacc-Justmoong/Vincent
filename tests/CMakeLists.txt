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

add_executable(tests_canvastoolbarqmlcontract
        tst_canvastoolbarqmlcontract.cpp
)

target_link_libraries(tests_canvastoolbarqmlcontract PRIVATE
        Qt6::Core
        Qt6::Test
)
vincent_configure_local_runtime_paths(tests_canvastoolbarqmlcontract)

add_test(NAME tests_canvastoolbarqmlcontract COMMAND tests_canvastoolbarqmlcontract)

add_executable(tests_drawingsurfaceitem
        tst_drawingsurfaceitem.cpp
        ../App/models/brush/paletteutils.cpp
        ../App/models/brush/paletteutils.h
        ../App/models/canvas/canvasdocumentviewmodel.cpp
        ../App/models/canvas/canvasdocumentviewmodel.h
        ../App/models/canvas/canvasviewmodelbridge.cpp
        ../App/models/canvas/canvasviewmodelbridge.h
        ../App/models/painting/drawingsurfaceitem.cpp
        ../App/models/painting/drawingsurfaceitem.h
)

target_include_directories(tests_drawingsurfaceitem PRIVATE
        ${CMAKE_SOURCE_DIR}/App
        ${CMAKE_SOURCE_DIR}/App/models/brush
        ${CMAKE_SOURCE_DIR}/App/models/canvas
        ${CMAKE_SOURCE_DIR}/App/models/painting
)

target_link_libraries(tests_drawingsurfaceitem PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::Test
        iiPaintEngine::iiPaintEngine
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
        tests_canvastoolbarqmlcontract
        tests_drawingsurfaceitem
        PROPERTIES
        ENVIRONMENT "${_vincent_test_environment}"
)
