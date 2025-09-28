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
        resources.qrc
)

qt_add_qml_module(Vincent
    URI Vincent
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/PainterCanvasPage.qml
        qml/CanvasToolBar.qml
        qml/DrawingSurface.qml
)

# Ensure the qmltyperegistrar always sees a valid (non-empty) metatypes.json.
get_target_property(_vincent_binary_dir Vincent BINARY_DIR)
get_property(_vincent_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(_vincent_multi_config)
    foreach(_vincent_config IN LISTS CMAKE_CONFIGURATION_TYPES)
        string(TOLOWER "${_vincent_config}" _vincent_config_lower)
        string(TOLOWER "Vincent_${_vincent_config}" _vincent_target_lower)
        set(_vincent_meta_dir "${_vincent_binary_dir}/${_vincent_config}/meta_types")
        set(_vincent_meta_file "${_vincent_meta_dir}/qt6${_vincent_target_lower}_metatypes.json")
        if(EXISTS "${_vincent_meta_file}")
            file(SIZE "${_vincent_meta_file}" _vincent_meta_size)
            if(_vincent_meta_size EQUAL 0)
                file(WRITE "${_vincent_meta_file}" "[]\n")
            endif()
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
    if(EXISTS "${_vincent_meta_file}")
        file(SIZE "${_vincent_meta_file}" _vincent_meta_size)
        if(_vincent_meta_size EQUAL 0)
            file(WRITE "${_vincent_meta_file}" "[]\n")
        endif()
    endif()
endif()
