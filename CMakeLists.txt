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

find_package(starlight_common CONFIG REQUIRED)
if(NOT TARGET Starlight::starlight)
    # Fallback: when building windowing standalone, use the installed/config package
    find_package(starlight CONFIG REQUIRED)
endif()

set(VULKAN_VERSION "1.4.313.0")
find_package(Vulkan ${VULKAN_VERSION} REQUIRED)
find_package(glfw3 CONFIG REQUIRED)

set(${PROJECT_NAME}_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_windowing/policy/EngineMainLoopPolicy.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_windowing/policy/EngineInitPolicy.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_windowing/policy/EngineExitPolicy.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_windowing/WindowingContext.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_windowing/StarWindow.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_windowing/PresentationCommands.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_windowing/RenderingSurface.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_windowing/SwapChainRenderer.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_windowing/Keys.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_windowing/KeyStates.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_windowing/event/KeyPress.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_windowing/event/KeyRelease.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_windowing/event/MouseButton.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_windowing/event/MouseMovement.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_windowing/event/Scroll.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_windowing/event/RequestSwapChainFromService.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_windowing/policy/HandleMouseMovementPolicy.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_windowing/policy/HandleMouseButtonPolicy.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_windowing/policy/HandleKeyPressPolicy.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_windowing/InteractivityBus.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_windowing/BasicCamera.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_windowing/policy/HandleKeyReleasePolicy.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_windowing/service/SwapChainControllerService.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_windowing/Swapchain.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_windowing/policy/ListenForRequestForSwapChainPolicy.hpp
)

set(${PROJECT_NAME}_SOURCES 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_windowing/policy/EngineMainLoopPolicy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_windowing/policy/EngineInitPolicy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_windowing/policy/EngineExitPolicy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_windowing/WindowingContext.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_windowing/StarWindow.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_windowing/PresentationCommands.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_windowing/RenderingSurface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_windowing/SwapChainRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_windowing/Keys.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_windowing/KeyStates.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_windowing/event/KeyPress.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_windowing/event/KeyRelease.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_windowing/event/MouseButton.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_windowing/event/MouseMovement.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_windowing/event/Scroll.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_windowing/event/RequestSwapChainFromService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_windowing/policy/HandleMouseMovementPolicy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_windowing/policy/HandleMouseButtonPolicy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_windowing/policy/HandleKeyPressPolicy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_windowing/policy/HandleKeyReleasePolicy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_windowing/policy/ListenForRequestForSwapChainPolicy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_windowing/InteractivityBus.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_windowing/BasicCamera.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_windowing/service/SwapChainControllerService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_windowing/Swapchain.cpp
)

add_library(${PROJECT_NAME} ${LIBTYPE}
    ${${PROJECT_NAME}_HEADERS}
    ${${PROJECT_NAME}_SOURCES}
)

target_include_directories(${PROJECT_NAME}
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
    Starlight::starlight
    starlight_common
    PRIVATE 
    glfw
    Vulkan::Vulkan
)

target_compile_definitions(${PROJECT_NAME} PRIVATE GLFW_INCLUDE_VULKAN)