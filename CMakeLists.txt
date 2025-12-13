cmake_minimum_required(VERSION 3.26)

project(starlight_windowing
    DESCRIPTION "Library containing windowing support for starlight"
    LANGUAGES CXX
)

option(STARLIGHT_WINDOWING_BUILD_SHARED "Build as shared library" OFF)

set(LIBTYPE STATIC)
if(STARLIGHT_WINDOWING_BUILD_SHARED)
    set(LIBTYPE SHARED)
endif()

find_package(starlight_common REQUIRED)
find_package(glfw3 REQUIRED)

set(${PROJECT_NAME}_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/windowing/policy/InteractiveWindowingPolicy.hpp
)

set(${PROJECT_NAME}_SOURCES 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/windowing/policy/InteractiveWindowingPolicy.cpp
)

add_library(${PROJECT_NAME} ${LIBTYPE}
    ${${PROJECT_NAME}_HEADERS}
    ${${PROJECT_NAME}_SOURCES}
)

target_include_directories(${PROJECT_NAME}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE glfw3
)