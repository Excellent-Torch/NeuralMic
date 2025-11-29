cmake_minimum_required(VERSION 3.28)
project(NeuralMic
    VERSION 2.0.0 
    LANGUAGES CXX
    DESCRIPTION "Real-time AI-based noise suppression for Linux"
)

# C++ Standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================================
# OPTIONS
# ============================================================================

option(FETCH_ONNXRUNTIME "Download and build ONNX Runtime from source" ON)

# ============================================================================
# DEPENDENCIES
# ============================================================================

# ALSA (Audio) - Required
find_package(ALSA REQUIRED)

# ONNX Runtime
if(FETCH_ONNXRUNTIME)
    message(STATUS "Downloading ONNX Runtime...")
    include(FetchContent)
    FetchContent_Declare(onnxruntime
        URL https://github.com/microsoft/onnxruntime/releases/download/v1.23.2/onnxruntime-linux-x64-1.23.2.tgz
        SOURCE_DIR ${CMAKE_BINARY_DIR}/_deps/onnxruntime
    )
    FetchContent_MakeAvailable(onnxruntime)
    set(ONNXRUNTIME_INCLUDE_DIR ${CMAKE_BINARY_DIR}/_deps/onnxruntime/include)
    set(ONNXRUNTIME_LIB ${CMAKE_BINARY_DIR}/_deps/onnxruntime/lib/libonnxruntime.so)
    set(HAVE_ONNXRUNTIME TRUE)
    message(STATUS "ONNX Runtime Downloaded!")
else()
    # Try to find system-installed ONNX Runtime
    find_package(onnxruntime CONFIG QUIET)
    if(onnxruntime_FOUND)
        message(STATUS "ONNX Runtime Found!")
        set(HAVE_ONNXRUNTIME TRUE)
        set(ONNXRUNTIME_TARGET onnxruntime::onnxruntime)
    else()
        message(STATUS "ONNX Runtime not found")
        message(STATUS "  Use: cmake .. -DFETCH_ONNXRUNTIME=ON  (to auto-download)")
        set(HAVE_ONNXRUNTIME FALSE)
    endif()
endif()

# ============================================================================
# LIBRARY: NeuralMicLib
# ============================================================================

add_library(NeuralMicLib
    src/Utils/MicReader.cpp
    src/Core/OnnxInference.cpp
)

target_include_directories(NeuralMicLib PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${ALSA_INCLUDE_DIRS}
)

target_link_libraries(NeuralMicLib PUBLIC
    ${ALSA_LIBRARIES}
)

# Link ONNX Runtime if available
if(HAVE_ONNXRUNTIME)
    if(FETCH_ONNXRUNTIME)
        target_include_directories(NeuralMicLib PUBLIC ${ONNXRUNTIME_INCLUDE_DIR})
        target_link_libraries(NeuralMicLib PUBLIC ${ONNXRUNTIME_LIB})
    else()
        target_link_libraries(NeuralMicLib PUBLIC ${ONNXRUNTIME_TARGET})
    endif()
    target_compile_definitions(NeuralMicLib PUBLIC HAVE_ONNXRUNTIME=1)
endif()

# Install LAME: sudo apt-get install libmp3lame-dev (on Debian/Ubuntu)
# Link LAME library
find_library(LAME_LIB mp3lame)
if(LAME_LIB)
    target_compile_definitions(NeuralMicLib PRIVATE USE_MP3)
    target_link_libraries(NeuralMicLib PRIVATE ${LAME_LIB})
    message(STATUS "MP3 support enabled")
else()
    message(WARNING "LAME not found, MP3 disabled")
endif()

# ============================================================================
# EXECUTABLE: NeuralMic
# ============================================================================

add_executable(NeuralMic
    src/main.cpp
)

target_include_directories(NeuralMic PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(NeuralMic PRIVATE
    NeuralMicLib
)

# ============================================================================
# BUILD INFO
# ============================================================================

message(STATUS "")
message(STATUS "═══════════════════════════════════════════")
message(STATUS "  Build Configuration")
message(STATUS "═══════════════════════════════════════════")
message(STATUS "  Project:        ${PROJECT_NAME} ${PROJECT_VERSION}")
message(STATUS "  C++ Standard:   ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler:       ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "  Build Type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "")
message(STATUS "  Dependencies:")
message(STATUS "    ALSA:         ✓ FOUND")
message(STATUS "    ONNX Runtime: ${HAVE_ONNXRUNTIME}")
message(STATUS "═══════════════════════════════════════════")
message(STATUS "")