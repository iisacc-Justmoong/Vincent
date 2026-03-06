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

add_executable(tests_imageimport
        tst_imageimport.cpp
        ../App/imageimport.cpp
        ../App/imageimport.h
)

target_include_directories(tests_imageimport PRIVATE
        ${CMAKE_SOURCE_DIR}/App
)

target_link_libraries(tests_imageimport PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Test
)

add_test(NAME tests_imageimport COMMAND tests_imageimport)
