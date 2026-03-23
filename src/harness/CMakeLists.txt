add_library(act_harness STATIC
    harness.cpp
    tool_registry.cpp
    permission_manager.cpp
    context_manager.cpp
    tools/file_read_tool.cpp
    tools/file_write_tool.cpp
    tools/file_edit_tool.cpp
    tools/grep_tool.cpp
    tools/glob_tool.cpp
    tools/shell_exec_tool.cpp
    tools/git_status_tool.cpp
    tools/git_diff_tool.cpp
)

target_include_directories(act_harness
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(act_harness
    PUBLIC
        act_services
)

target_compile_features(act_harness PUBLIC cxx_std_20)
