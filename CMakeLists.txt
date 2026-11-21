cmake_minimum_required(VERSION 3.21)
project(TlutPreview LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PNG REQUIRED)
find_package(JPEG REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
qt_standard_project_setup()

# BitDeck's texture codec and per-game TLUT / I8 transparency tables, copied
# unmodified by tools/sync_from_bitdeck.sh.
file(GLOB_RECURSE BITDECK_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/bitdeck/*.cpp)
add_library(bitdeck STATIC ${BITDECK_SOURCES})
target_include_directories(bitdeck PUBLIC third_party/bitdeck)
target_link_libraries(bitdeck PUBLIC PNG::PNG JPEG::JPEG)

# Folder-in, folder-out colorizer. Qt Core only (manifest.json).
add_library(colorize STATIC src/colorize.cpp)
target_include_directories(colorize PUBLIC src)
target_link_libraries(colorize PUBLIC bitdeck Qt6::Core)

qt_add_executable(TlutPreview
    WIN32 MACOSX_BUNDLE
    src/main.cpp
    src/main_window.cpp
    src/main_window.h
)
target_link_libraries(TlutPreview PRIVATE colorize Qt6::Widgets)

enable_testing()
add_executable(colorize_tests tests/colorize_tests.cpp)
target_link_libraries(colorize_tests PRIVATE colorize)
add_test(NAME colorize_tests COMMAND colorize_tests)
