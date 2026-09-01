cmake_minimum_required(VERSION 3.30)

project(
    PhotoBooth
    VERSION 0.7.0
    DESCRIPTION "Photo booth with image processing pipeline (IMGS.361)"
    LANGUAGES CXX
)


# -----------------------------------------------------------------------------
# Build configuration
# -----------------------------------------------------------------------------

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(
        CMAKE_BUILD_TYPE
        RelWithDebInfo
        CACHE STRING
        "Choose the type of build."
        FORCE
    )
endif()

# -----------------------------------------------------------------------------
# Dependencies
# -----------------------------------------------------------------------------

find_package(
    OpenCV
    REQUIRED
    COMPONENTS
        core
        videoio
        highgui
        imgcodecs
        imgproc
)

find_package(
    Boost
    REQUIRED
    COMPONENTS
        program_options
)

find_package(
    Eigen3
    REQUIRED
    NO_MODULE
)

include(FetchContent)

FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
)

FetchContent_MakeAvailable(tomlplusplus)

FetchContent_Declare(
    portable_file_dialogs
    GIT_REPOSITORY https://github.com/samhocevar/portable-file-dialogs.git
    GIT_TAG        c12ea8c
    SOURCE_SUBDIR  _do_not_build
)

FetchContent_MakeAvailable(portable_file_dialogs)

add_library(portable_file_dialogs INTERFACE)

target_include_directories(
    portable_file_dialogs
    INTERFACE
        ${portable_file_dialogs_SOURCE_DIR}
)

find_program(GNUPLOT_EXECUTABLE gnuplot REQUIRED)

message(STATUS "Found gnuplot: ${GNUPLOT_EXECUTABLE}")


# -----------------------------------------------------------------------------
# Dependency validation
# -----------------------------------------------------------------------------

# Explicitly support OpenCV 4.x and 5.x.
if(OpenCV_VERSION VERSION_LESS 4.0)
    message(
        FATAL_ERROR
        "PhotoBooth requires OpenCV 4 or OpenCV 5. "
        "Found OpenCV ${OpenCV_VERSION}."
    )
endif()

if(OpenCV_VERSION VERSION_GREATER_EQUAL 6.0)
    message(
        FATAL_ERROR
        "PhotoBooth has not yet been tested with OpenCV 6 or later. "
        "Found OpenCV ${OpenCV_VERSION}."
    )
endif()

message(STATUS "Using OpenCV ${OpenCV_VERSION}")


# -----------------------------------------------------------------------------
# Project-wide target settings
# -----------------------------------------------------------------------------

add_library(photo_booth_warnings INTERFACE)
add_library(PhotoBooth::warnings ALIAS photo_booth_warnings)

if(MSVC)
    target_compile_options(
        photo_booth_warnings
        INTERFACE
            /W4
            /permissive-
    )
else()
    target_compile_options(
        photo_booth_warnings
        INTERFACE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

# Boost.Program_options and Eigen are intentionally pre-provisioned for
# student applications even though the starter applications do not yet use
# them directly.
add_library(photo_booth_app_dependencies INTERFACE)
add_library(PhotoBooth::app_dependencies ALIAS photo_booth_app_dependencies)

target_link_libraries(
    photo_booth_app_dependencies
    INTERFACE
        Boost::program_options
        Eigen3::Eigen
)


# -----------------------------------------------------------------------------
# Core library
# -----------------------------------------------------------------------------

add_library(
    photo_booth_core
    src/AppConfig.cpp
    src/FileSelection.cpp
    src/ImageCapture.cpp
    src/ImageProcessing.cpp
    src/PlotDisplay.cpp
    src/SpectrumDisplay.cpp
)

add_library(PhotoBooth::core ALIAS photo_booth_core)

target_compile_definitions(
    photo_booth_core
    PRIVATE
        GNUPLOT_EXECUTABLE="${GNUPLOT_EXECUTABLE}"
)

target_compile_features(
    photo_booth_core
    PUBLIC
        cxx_std_20
)

target_include_directories(
    photo_booth_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(
    photo_booth_core
    PUBLIC
        ${OpenCV_LIBS}
    PRIVATE
        tomlplusplus::tomlplusplus
        portable_file_dialogs
        PhotoBooth::warnings
)


# -----------------------------------------------------------------------------
# Applications
# -----------------------------------------------------------------------------

# Minimal live-preview example: acquire a frame and display it directly,
# without an explicit image-processing stage.
add_executable(
    live_preview
    apps/live_preview.cpp
)

target_link_libraries(
    live_preview
    PRIVATE
        PhotoBooth::core
        PhotoBooth::app_dependencies
        PhotoBooth::warnings
)

# Main student application: the semester project baseline with an explicit
# image-processing pipeline.
add_executable(
    photo_booth
    apps/photo_booth.cpp
)

target_link_libraries(
    photo_booth
    PRIVATE
        PhotoBooth::core
        PhotoBooth::app_dependencies
        PhotoBooth::warnings
)

# Small camera utility retained as a diagnostic/example program.
add_executable(
    capture_single_image
    apps/capture_single_image.cpp
)

target_link_libraries(
    capture_single_image
    PRIVATE
        PhotoBooth::core
        PhotoBooth::app_dependencies
        PhotoBooth::warnings
)


# Keep project targets in strict ISO C++20 mode rather than compiler-specific
# GNU/MSVC extension modes.
set_target_properties(
    photo_booth_core
    live_preview
    photo_booth
    capture_single_image
    PROPERTIES
        CXX_EXTENSIONS OFF
)
