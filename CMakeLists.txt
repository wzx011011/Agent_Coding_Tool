cmake_minimum_required(VERSION 3.28)

set(_ACT_VCPKG_TOOLCHAIN_FROM_CACHE "${CMAKE_TOOLCHAIN_FILE}")
set(_ACT_VCPKG_TOOLCHAIN "")

if(_ACT_VCPKG_TOOLCHAIN_FROM_CACHE AND EXISTS "${_ACT_VCPKG_TOOLCHAIN_FROM_CACHE}")
    set(_ACT_VCPKG_TOOLCHAIN "${_ACT_VCPKG_TOOLCHAIN_FROM_CACHE}")
elseif(DEFINED ENV{VCPKG_ROOT} AND NOT "$ENV{VCPKG_ROOT}" STREQUAL "")
    set(_ACT_VCPKG_TOOLCHAIN "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
endif()

if(NOT _ACT_VCPKG_TOOLCHAIN)
    message(FATAL_ERROR
        "Unable to determine the vcpkg toolchain file.\n"
        "Either configure once with VCPKG_ROOT set, or set CMAKE_TOOLCHAIN_FILE to an existing vcpkg.cmake path.")
endif()

set(CMAKE_TOOLCHAIN_FILE
    "${_ACT_VCPKG_TOOLCHAIN}"
    CACHE FILEPATH "vcpkg toolchain")

if(NOT EXISTS "${CMAKE_TOOLCHAIN_FILE}")
    message(FATAL_ERROR
        "vcpkg toolchain file was not found at:\n  ${CMAKE_TOOLCHAIN_FILE}\n"
        "Verify VCPKG_ROOT points to a valid vcpkg checkout or update CMAKE_TOOLCHAIN_FILE.")
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

set(CMAKE_AUTOMOC ON)

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

# --- Install (aictl + runtime DLLs) ---
include(GNUInstallDirs)

install(TARGETS aictl RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Vcpkg runtime DLLs
set(_VCPKG_INSTALLED_DIR "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows")
install(FILES
    ${_VCPKG_INSTALLED_DIR}/bin/tomlplusplus-3.dll
    ${_VCPKG_INSTALLED_DIR}/bin/spdlog.dll
    ${_VCPKG_INSTALLED_DIR}/bin/fmt.dll
    ${_VCPKG_INSTALLED_DIR}/bin/libcrypto-3-x64.dll
    ${_VCPKG_INSTALLED_DIR}/bin/libssl-3-x64.dll
    ${_VCPKG_INSTALLED_DIR}/bin/brotlicommon.dll
    ${_VCPKG_INSTALLED_DIR}/bin/brotlidec.dll
    ${_VCPKG_INSTALLED_DIR}/bin/brotlienc.dll
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Qt6 runtime DLLs are handled by windeployqt6 in _build.bat deploy step.
