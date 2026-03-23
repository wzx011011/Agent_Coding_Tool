cmake_minimum_required(VERSION 3.28)

if(NOT DEFINED ENV{VCPKG_ROOT})
    message(FATAL_ERROR
        "VCPKG_ROOT environment variable is not set.\n"
        "Set it to your vcpkg checkout before configuring ACT.")
endif()

set(CMAKE_TOOLCHAIN_FILE
    "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
    CACHE FILEPATH "vcpkg toolchain")

if(NOT EXISTS "${CMAKE_TOOLCHAIN_FILE}")
    message(FATAL_ERROR
        "vcpkg toolchain file was not found at:\n  ${CMAKE_TOOLCHAIN_FILE}\n"
        "Verify VCPKG_ROOT points to a valid vcpkg checkout.")
endif()

if(NOT DEFINED Qt6_DIR AND "${CMAKE_PREFIX_PATH}" STREQUAL "" AND "$ENV{CMAKE_PREFIX_PATH}" STREQUAL "")
    message(FATAL_ERROR
        "Qt 6.10.2 was not located.\n"
        "Set Qt6_DIR or CMAKE_PREFIX_PATH to the Qt 6.10.2 installation prefix before configuring.")
endif()

project(ACT VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.10.2 REQUIRED COMPONENTS Core)
find_package(GTest REQUIRED)
find_package(spdlog REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(tomlplusplus REQUIRED)

# Core (standalone, not in layer chain)
add_subdirectory(src)

# Layer 5: Infrastructure
add_subdirectory(src/infrastructure)

# Layer 4: Core Services
add_subdirectory(src/services)

# Layer 3: Agent Harness
add_subdirectory(src/harness)

# Layer 2: Agent Framework
add_subdirectory(src/framework)

# Layer 1: Presentation
add_subdirectory(src/presentation/vscode-protocol)

# CLI executable
add_subdirectory(app/cli)

# Tests
enable_testing()
add_subdirectory(tests)
