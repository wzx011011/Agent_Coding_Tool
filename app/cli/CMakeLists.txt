add_executable(aictl
    main.cpp
)

target_include_directories(aictl
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(aictl
    PRIVATE
        act_core
        act_framework
        act_services
        Qt6::Core
)

target_compile_features(aictl PRIVATE cxx_std_20)

# --- VS debugger settings ---
if(CMAKE_GENERATOR MATCHES "Visual Studio")
    set_target_properties(aictl PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        VS_DEBUGGER_ENVIRONMENT
            "PATH=${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/bin\;E:\\Qt6.10\\bin\;%PATH%"
    )
endif()

# Deploy Qt DLLs next to the executable after build
# Note: Qt 6.10 only has Release DLLs, so always use --release
find_program(WINDEPLOYQT6 windeployqt6 HINTS "${Qt6_DIR}/../../../bin")
if(WINDEPLOYQT6)
    add_custom_command(TARGET aictl POST_BUILD
        COMMAND "${WINDEPLOYQT6}" --no-translations --no-opengl-sw --release "$<TARGET_FILE:aictl>"
        COMMENT "Deploying Qt runtime DLLs via windeployqt6"
    )
endif()
