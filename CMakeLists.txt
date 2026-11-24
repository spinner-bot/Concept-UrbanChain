cmake_minimum_required(VERSION 3.16)

project(ConceptUrbanChain VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_library(uc_core
    src/core/map.cpp
    src/core/line.cpp
    src/core/network.cpp
    src/io/map_io.cpp
    src/io/line_io.cpp
    src/nav/router.cpp
    src/edit/editor.cpp
)

target_include_directories(uc_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

option(UC_BUILD_GUI "Build the Qt Widgets GUI" ON)
if(UC_BUILD_GUI)
    find_package(Qt6 QUIET COMPONENTS Widgets)
    if(Qt6_FOUND)
        set(CMAKE_AUTOMOC ON)

        add_library(uc_ui STATIC
            src/ui/language.cpp
            src/ui/main_window.cpp
            src/ui/map_view.cpp
            src/ui/map_scene.cpp
            src/ui/station_item.cpp
            src/ui/line_dialog.cpp
            src/ui/transfer_dialog.cpp
        )
        target_link_libraries(uc_ui PUBLIC uc_core Qt6::Widgets)

        add_executable(urbanchain WIN32 src/ui/main.cpp)
        target_link_libraries(urbanchain PRIVATE uc_ui)

        get_filename_component(_qt_prefix "${Qt6_DIR}/../../.." ABSOLUTE)
        find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_prefix}/bin")
        function(uc_deploy_qt target)
            if(WINDEPLOYQT_EXECUTABLE)
                add_custom_command(TARGET ${target} POST_BUILD
                    COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-translations --no-opengl-sw
                            "$<TARGET_FILE:${target}>"
                    COMMENT "Deploying Qt runtime for ${target}")
            endif()
        endfunction()
        uc_deploy_qt(urbanchain)

        set(_qt_zh_qm "${_qt_prefix}/translations/qtbase_zh_CN.qm")
        if(EXISTS "${_qt_zh_qm}")
            add_custom_command(TARGET urbanchain POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                        "$<TARGET_FILE_DIR:urbanchain>/translations"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_qt_zh_qm}"
                        "$<TARGET_FILE_DIR:urbanchain>/translations/qtbase_zh_CN.qm"
                COMMENT "Copying Qt Chinese translations")
        endif()

        find_package(Qt6 QUIET COMPONENTS Test)
        if(Qt6Test_FOUND)
            add_executable(uc_ui_tests tests/ui_smoke.cpp)
            target_link_libraries(uc_ui_tests PRIVATE uc_ui Qt6::Test)
            add_dependencies(uc_ui_tests urbanchain)
            uc_deploy_qt(uc_ui_tests)
        endif()
    else()
        message(WARNING "Qt6 not found; set CMAKE_PREFIX_PATH to the Qt msvc2022_64 directory.")
    endif()
endif()

enable_testing()

add_executable(uc_tests tests/test_main.cpp)
target_link_libraries(uc_tests PRIVATE uc_core)
add_test(NAME uc_tests COMMAND uc_tests)

if(TARGET uc_ui_tests)
    add_test(NAME uc_ui_tests COMMAND uc_ui_tests)
endif()
