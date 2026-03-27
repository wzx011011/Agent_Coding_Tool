add_library(act_framework STATIC
    agent_loop.cpp
    agent_scheduler.cpp
    channel.cpp
    cli_repl.cpp
    command_registry.cpp
    eval_runner.cpp
    execution_lane.cpp
    feishu_channel.cpp
    framework.cpp
    input_dispatcher.cpp
    interactive_session_controller.cpp
    interactive_session_state.cpp
    markdown_formatter.cpp
    patch_transaction.cpp
    resume_manager.cpp
    runtime_event_logger.cpp
    runtime_trace_store.cpp
    session_manager.cpp
    stream_formatter.cpp
    skill_catalog.cpp
    skill_loader.cpp
    subagent_manager.cpp
    task_graph.cpp
    terminal_style.cpp
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
