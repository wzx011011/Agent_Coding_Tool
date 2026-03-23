add_library(act_framework STATIC
    agent_loop.cpp
    cli_repl.cpp
    framework.cpp
    runtime_event_logger.cpp
    runtime_trace_store.cpp
    skill_catalog.cpp
    skill_loader.cpp
    subagent_manager.cpp
    task_graph.cpp
)

target_include_directories(act_framework
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(act_framework
    PUBLIC
        act_harness
)

target_compile_features(act_framework PUBLIC cxx_std_20)
