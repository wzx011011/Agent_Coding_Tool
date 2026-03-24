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

# --- VS debugger: use Windows Terminal for F5 ---
if(CMAKE_GENERATOR MATCHES "Visual Studio")
    # wt.exe is a Windows Store app alias, not in system PATH
    find_program(WT_EXE wt.exe
        HINTS "$ENV{LOCALAPPDATA}/Microsoft/WindowsApps"
    )
    if(WT_EXE)
        set_target_properties(aictl PROPERTIES
            VS_DEBUGGER_COMMAND "${WT_EXE}"
            VS_DEBUGGER_COMMAND_ARGUMENTS "-p \"PowerShell\" $<TARGET_FILE:aictl>"
            VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            VS_DEBUGGER_ENVIRONMENT
                "PATH=${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/bin\;E:\\Qt6.10\\bin\;%PATH%\;CHCP=65001"
        )
    endif()
endif()

# Deploy Qt DLLs next to the executable after build
find_program(WINDEPLOYQT6 windeployqt6 HINTS "${Qt6_DIR}/../../../bin")
if(WINDEPLOYQT6)
    add_custom_command(TARGET aictl POST_BUILD
        COMMAND "${WINDEPLOYQT6}" --no-translations --no-opengl-sw --release "$<TARGET_FILE:aictl>"
        COMMENT "Deploying Qt runtime DLLs via windeployqt6"
    )
endif()
