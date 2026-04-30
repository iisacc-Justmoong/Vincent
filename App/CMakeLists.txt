if(APPLE)
    set(CMAKE_FIND_FRAMEWORK FIRST)
    list(PREPEND CMAKE_SYSTEM_FRAMEWORK_PATH
        "${CMAKE_OSX_SYSROOT}/System/Library/Frameworks"
        "${CMAKE_OSX_SYSROOT}/Library/Frameworks")

    set(CMAKE_DISABLE_FIND_PACKAGE_OpenGL ON CACHE BOOL "" FORCE)
    set(CMAKE_DISABLE_FIND_PACKAGE_WrapOpenGL ON CACHE BOOL "" FORCE)

    if(NOT TARGET WrapOpenGL::WrapOpenGL)
        add_library(WrapOpenGL::WrapOpenGL INTERFACE IMPORTED)
        target_link_libraries(WrapOpenGL::WrapOpenGL INTERFACE "-framework OpenGL")
    endif()

    set(WrapOpenGL_FOUND TRUE CACHE BOOL "" FORCE)
    set(OPENGL_LIBRARIES "-framework OpenGL" CACHE STRING "" FORCE)
    set(OPENGL_gl_LIBRARY "-framework OpenGL" CACHE STRING "" FORCE)
    set(OPENGL_opengl_LIBRARY "-framework OpenGL" CACHE STRING "" FORCE)
    set(OPENGL_INCLUDE_DIR "" CACHE PATH "" FORCE)
endif()

target_sources(Vincent PRIVATE
        main.cpp
        metatypes_dummy.cpp
        metatypes_dummy.h
        models/brush/brushengine.cpp
        models/brush/brushengine.h
        models/brush/brushstrokebuilder.cpp
        models/brush/brushstrokebuilder.h
        models/brush/paletteutils.cpp
        models/brush/paletteutils.h
        models/canvas/canvasbackend.cpp
        models/canvas/canvasbackend.h
        models/canvas/canvasdocumentviewmodel.cpp
        models/canvas/canvasdocumentviewmodel.h
        models/canvas/canvasviewmodelbridge.cpp
        models/canvas/canvasviewmodelbridge.h
        models/file/canvasfileadapter.cpp
        models/file/canvasfileadapter.h
        models/file/rasterdocumentio.cpp
        models/file/rasterdocumentio.h
        models/painting/drawingdocumentcontroller.cpp
        models/painting/drawingdocumentcontroller.h
        models/painting/drawingsurfaceitem.cpp
        models/painting/drawingsurfaceitem.h
        models/painting/drawingstrokecontroller.cpp
        models/painting/drawingstrokecontroller.h
        models/painting/paintingbackgroundstate.cpp
        models/painting/paintingbackgroundstate.h
        models/painting/paintingdocumentmodel.cpp
        models/painting/paintingdocumentmodel.h
        models/painting/paintinghistorycontroller.cpp
        models/painting/paintinghistorycontroller.h
        models/painting/paintingrenderer.cpp
        models/painting/paintingrenderer.h
        resources.qrc
)

target_include_directories(Vincent PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/models/brush
        ${CMAKE_CURRENT_SOURCE_DIR}/models/canvas
        ${CMAKE_CURRENT_SOURCE_DIR}/models/file
        ${CMAKE_CURRENT_SOURCE_DIR}/models/painting
)
