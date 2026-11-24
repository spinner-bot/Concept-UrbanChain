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
)

target_include_directories(uc_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(urbanchain src/main.cpp src/app/console_app.cpp)
target_link_libraries(urbanchain PRIVATE uc_core)

enable_testing()

add_executable(uc_tests tests/test_main.cpp)
target_link_libraries(uc_tests PRIVATE uc_core)
add_test(NAME uc_tests COMMAND uc_tests)
