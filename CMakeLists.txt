cmake_minimum_required(VERSION 3.28)
project(phu-compressor VERSION 0.1.0 LANGUAGES C CXX)

# Global settings (before fetching, so they also apply to fetched dependencies)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# JUCE feature flags and build options; included before the fetch so that the
# options and compile definitions also reach JUCE and phu-audio-lib targets.
include(cmake/juce_config.cmake)

# Fetch dependencies (reusable helpers in cmake/PhuDependencies.cmake)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(PhuDependencies)
phu_fetch_juce(TAG 9.0.0)
phu_fetch_audio_lib(TAG main)

# Warnings/sanitizers/coverage; included after the fetch so they only apply to this project
include(cmake/compiler_flags.cmake)

add_subdirectory(lib)
add_subdirectory(src)
