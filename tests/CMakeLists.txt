add_executable(tests_brushengine
        tst_brushengine.cpp
        ../App/brushengine.cpp
        ../App/brushengine.h
)

target_include_directories(tests_brushengine PRIVATE
        ${CMAKE_SOURCE_DIR}/App
)

target_link_libraries(tests_brushengine PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME tests_brushengine COMMAND tests_brushengine)

add_executable(tests_canvasdocumentviewmodel
        tst_canvasdocumentviewmodel.cpp
        ../App/canvasdocumentviewmodel.cpp
        ../App/canvasdocumentviewmodel.h
        ../App/paletteutils.cpp
        ../App/paletteutils.h
)

target_include_directories(tests_canvasdocumentviewmodel PRIVATE
        ${CMAKE_SOURCE_DIR}/App
)

target_link_libraries(tests_canvasdocumentviewmodel PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Test
)

add_test(NAME tests_canvasdocumentviewmodel COMMAND tests_canvasdocumentviewmodel)

add_executable(tests_canvasbackend
        tst_canvasbackend.cpp
        ../App/canvasbackend.cpp
        ../App/canvasbackend.h
)

target_include_directories(tests_canvasbackend PRIVATE
        ${CMAKE_SOURCE_DIR}/App
)

target_link_libraries(tests_canvasbackend PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME tests_canvasbackend COMMAND tests_canvasbackend)

add_executable(tests_rasterdocumentio
        tst_rasterdocumentio.cpp
        ../App/rasterdocumentio.cpp
        ../App/rasterdocumentio.h
)

target_include_directories(tests_rasterdocumentio PRIVATE
        ${CMAKE_SOURCE_DIR}/App
)

target_link_libraries(tests_rasterdocumentio PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Test
)

add_test(NAME tests_rasterdocumentio COMMAND tests_rasterdocumentio)

add_executable(tests_drawingsurfaceitem
        tst_drawingsurfaceitem.cpp
        ../App/brushengine.cpp
        ../App/brushengine.h
        ../App/canvasbackend.cpp
        ../App/canvasbackend.h
        ../App/canvasdocumentviewmodel.cpp
        ../App/canvasdocumentviewmodel.h
        ../App/paletteutils.cpp
        ../App/paletteutils.h
        ../App/rasterdocumentio.cpp
        ../App/rasterdocumentio.h
        ../App/models/painting/drawingsurfaceitem.cpp
        ../App/models/painting/drawingsurfaceitem.h
)

target_include_directories(tests_drawingsurfaceitem PRIVATE
        ${CMAKE_SOURCE_DIR}/App
)

target_link_libraries(tests_drawingsurfaceitem PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::Test
)

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
        tests_brushengine
        tests_canvasdocumentviewmodel
        tests_canvasbackend
        tests_rasterdocumentio
        tests_drawingsurfaceitem
        PROPERTIES
        ENVIRONMENT "${_vincent_test_environment}"
)
