cmake_minimum_required(VERSION 3.26)

project(starlight-windowing
    DESCRIPTION "Library containing windowing support for starlight"
    LANGUAGES CXX
)

option(STARLIGHT_WINDOWING_BUILD_SHARED "Build as shared library" OFF)

set(LIBTYPE STATIC)
if(STARLIGHT_WINDOWING_BUILD_SHARED)
    set(LIBTYPE SHARED)
endif()

find_package(glfw REQUIRED)

add_library(starlight-windowing ${LIBTYPE}
    src/GlfwProvider.cpp
)

target_include_directories(starlight-windowing
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(starlight-windowing
    PRIVATE glfw
)

target_compile_features(starlight-windowing PUBLIC cxx_std_20)