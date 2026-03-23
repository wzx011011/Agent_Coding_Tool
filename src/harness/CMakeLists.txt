add_library(act_harness STATIC
    harness.cpp
    tool_registry.cpp
    tools/file_read_tool.cpp
    tools/grep_tool.cpp
    tools/glob_tool.cpp
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
