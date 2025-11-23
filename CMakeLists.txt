cmake_minimum_required(VERSION 3.28)
project(AINoiseSuppressionLinux 
    VERSION 2.0.0 
    LANGUAGES CXX
    DESCRIPTION "A Real Time AI Based Noise Suppression Client"
)

# Require C++23
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Check Libraries
find_package(ALSA REQUIRED)

# Add include/ Directory
include_directories(${PROJECT_SOURCE_DIR}/include)

# Add Custom Library (without main.cpp)
add_library(NeuralMicLib 
    src/Utils/MicReader.cpp
)

# Add Executable
add_executable(NeuralMic
    src/main.cpp
)

# Link libraries to executable
target_link_libraries(NeuralMic
    NeuralMicLib
    ${ALSA_LIBRARIES}  
)

# Include directories for library
target_include_directories(NeuralMicLib PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${ALSA_INCLUDE_DIRS}
)

# Include directories for executable
target_include_directories(NeuralMic PRIVATE 
    ${PROJECT_SOURCE_DIR}/include
    ${ALSA_INCLUDE_DIRS}
)

# Print configuration summary
message(STATUS "")
message(STATUS "═══════════════════════════════════════════")
message(STATUS "  Configuration Summary")
message(STATUS "═══════════════════════════════════════════")
message(STATUS "  Project:          ${PROJECT_NAME} ${PROJECT_VERSION}")
message(STATUS "  Build Type:       ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard:     C++23")
message(STATUS "  Compiler:         ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Install Prefix:   ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
message(STATUS "  Features:")
message(STATUS "    GTK Version:    ${GTK3_VERSION}${GTK4_VERSION}")
message(STATUS "    CUDA Support:   ${USE_CUDA}")
message(STATUS "    ONNX Runtime:   ${USE_ONNXRUNTIME}")
message(STATUS "    TensorRT:       ${USE_TENSORRT}")
message(STATUS "    LTO Enabled:    ${ENABLE_LTO}")
message(STATUS "")
message(STATUS "  Sanitizers:")
message(STATUS "    AddressSan:     ${ENABLE_ASAN}")
message(STATUS "    ThreadSan:      ${ENABLE_TSAN}")
message(STATUS "    MemorySan:      ${ENABLE_MSAN}")
message(STATUS "═══════════════════════════════════════════")
message(STATUS "")