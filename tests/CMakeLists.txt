add_executable(tests_brushengine
        tst_brushengine.cpp
        ../App/models/brush/brushengine.cpp
        ../App/models/brush/brushengine.h
)

target_include_directories(tests_brushengine PRIVATE
        ${CMAKE_SOURCE_DIR}/App
        ${CMAKE_SOURCE_DIR}/App/models/brush
)

target_link_libraries(tests_brushengine PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME tests_brushengine COMMAND tests_brushengine)

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

add_test(NAME tests_canvasdocumentviewmodel COMMAND tests_canvasdocumentviewmodel)

add_executable(tests_canvasbackend
        tst_canvasbackend.cpp
        ../App/models/canvas/canvasbackend.cpp
        ../App/models/canvas/canvasbackend.h
)

target_include_directories(tests_canvasbackend PRIVATE
        ${CMAKE_SOURCE_DIR}/App
        ${CMAKE_SOURCE_DIR}/App/models/canvas
)

target_link_libraries(tests_canvasbackend PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME tests_canvasbackend COMMAND tests_canvasbackend)

add_executable(tests_rasterdocumentio
        tst_rasterdocumentio.cpp
        ../App/models/file/rasterdocumentio.cpp
        ../App/models/file/rasterdocumentio.h
)

target_include_directories(tests_rasterdocumentio PRIVATE
        ${CMAKE_SOURCE_DIR}/App
        ${CMAKE_SOURCE_DIR}/App/models/file
)

target_link_libraries(tests_rasterdocumentio PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Test
)

add_test(NAME tests_rasterdocumentio COMMAND tests_rasterdocumentio)

add_executable(tests_drawingsurfaceitem
        tst_drawingsurfaceitem.cpp
        ../App/models/brush/brushengine.cpp
        ../App/models/brush/brushengine.h
        ../App/models/brush/brushstrokebuilder.cpp
        ../App/models/brush/brushstrokebuilder.h
        ../App/models/brush/paletteutils.cpp
        ../App/models/brush/paletteutils.h
        ../App/models/canvas/canvasbackend.cpp
        ../App/models/canvas/canvasbackend.h
        ../App/models/canvas/canvasdocumentviewmodel.cpp
        ../App/models/canvas/canvasdocumentviewmodel.h
        ../App/models/canvas/canvasviewmodelbridge.cpp
        ../App/models/canvas/canvasviewmodelbridge.h
        ../App/models/file/canvasfileadapter.cpp
        ../App/models/file/canvasfileadapter.h
        ../App/models/file/rasterdocumentio.cpp
        ../App/models/file/rasterdocumentio.h
        ../App/models/painting/drawingdocumentcontroller.cpp
        ../App/models/painting/drawingdocumentcontroller.h
        ../App/models/painting/drawingsurfaceitem.cpp
        ../App/models/painting/drawingsurfaceitem.h
        ../App/models/painting/drawingstrokecontroller.cpp
        ../App/models/painting/drawingstrokecontroller.h
        ../App/models/painting/paintingbackgroundstate.cpp
        ../App/models/painting/paintingbackgroundstate.h
        ../App/models/painting/paintingdocumentmodel.cpp
        ../App/models/painting/paintingdocumentmodel.h
        ../App/models/painting/paintinghistorycontroller.cpp
        ../App/models/painting/paintinghistorycontroller.h
        ../App/models/painting/paintingrenderer.cpp
        ../App/models/painting/paintingrenderer.h
)

target_include_directories(tests_drawingsurfaceitem PRIVATE
        ${CMAKE_SOURCE_DIR}/App
        ${CMAKE_SOURCE_DIR}/App/models/brush
        ${CMAKE_SOURCE_DIR}/App/models/canvas
        ${CMAKE_SOURCE_DIR}/App/models/file
        ${CMAKE_SOURCE_DIR}/App/models/painting
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
