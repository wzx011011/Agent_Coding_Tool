include(GoogleTest)

# All tests need access to src/ for core headers
set(ACT_SOURCE_DIR ${PROJECT_SOURCE_DIR}/src)

# Default per-test timeout to prevent hangs (e.g. QEventLoop without QCoreApplication)
set(ACT_TEST_TIMEOUT 10 CACHE STRING "Per-test timeout in seconds")

# Defer test discovery to test-time to avoid DLL resolution issues at build time
# (VS/MSBuild post-build steps run test exes without vcpkg DLLs in PATH)
set(ACT_TEST_DISCOVERY_MODE PRE_TEST)

# Core tests
add_executable(act_core_tests
    core/RuntimeVersionTest.cpp
    core/CoreTypesTest.cpp
)
target_include_directories(act_core_tests PRIVATE ${ACT_SOURCE_DIR})
target_link_libraries(act_core_tests PRIVATE act_core Qt6::Core GTest::gtest_main)
gtest_discover_tests(act_core_tests
    PROPERTIES TIMEOUT ${ACT_TEST_TIMEOUT}
    DISCOVERY_MODE ${ACT_TEST_DISCOVERY_MODE}
)

# Framework tests
add_executable(act_framework_tests
    framework/SmokeTest.cpp
    framework/AgentLoopTest.cpp
    framework/AgentSchedulerTest.cpp
    framework/CliReplTest.cpp
    framework/EvalRunnerTest.cpp
    framework/ExecutionLaneTest.cpp
    framework/MarkdownFormatterTest.cpp
    framework/PatchTransactionTest.cpp
    framework/ResumeManagerTest.cpp
    framework/SkillCatalogTest.cpp
    framework/RuntimeTraceTest.cpp
    framework/TerminalStyleTest.cpp
)
target_link_libraries(act_framework_tests PRIVATE act_framework GTest::gtest_main)
gtest_discover_tests(act_framework_tests
    PROPERTIES TIMEOUT ${ACT_TEST_TIMEOUT}
    DISCOVERY_MODE ${ACT_TEST_DISCOVERY_MODE}
)

# Interactive session tests
add_executable(act_cli_tests
    cli/test_main.cpp
    cli/InteractiveSessionStateTest.cpp
    cli/InteractiveSessionControllerTest.cpp
)
target_include_directories(act_cli_tests PRIVATE ${PROJECT_SOURCE_DIR} ${ACT_SOURCE_DIR})
target_link_libraries(act_cli_tests PRIVATE act_framework Qt6::Core GTest::gtest)
gtest_discover_tests(act_cli_tests
    PROPERTIES TIMEOUT ${ACT_TEST_TIMEOUT}
    DISCOVERY_MODE ${ACT_TEST_DISCOVERY_MODE}
)

# Subagent manager tests (separate target to avoid MSVC C1903)
add_executable(act_subagent_tests
    framework/SubagentManagerTest.cpp
)
target_link_libraries(act_subagent_tests PRIVATE act_framework GTest::gtest_main)
set_source_files_properties(framework/SubagentManagerTest.cpp PROPERTIES
    CXX_SCAN_FOR_MODULES OFF
)
gtest_discover_tests(act_subagent_tests
    PROPERTIES TIMEOUT ${ACT_TEST_TIMEOUT}
    DISCOVERY_MODE ${ACT_TEST_DISCOVERY_MODE}
)

# Task graph tests
add_executable(act_task_graph_tests
    framework/TaskGraphTest.cpp
)
target_link_libraries(act_task_graph_tests PRIVATE act_framework GTest::gtest_main)
set_source_files_properties(framework/TaskGraphTest.cpp PROPERTIES
    CXX_SCAN_FOR_MODULES OFF
)
gtest_discover_tests(act_task_graph_tests
    PROPERTIES TIMEOUT ${ACT_TEST_TIMEOUT}
    DISCOVERY_MODE ${ACT_TEST_DISCOVERY_MODE}
)

# Skill loader tests (separate target — toml++ heavy headers cause MSVC C1903)
add_executable(act_skill_loader_tests
    framework/SkillLoaderTest.cpp
)
target_link_libraries(act_skill_loader_tests PRIVATE act_framework GTest::gtest_main)
# Skip C++ module scanning for this file — toml++ headers overwhelm MSVC DDI
set_source_files_properties(framework/SkillLoaderTest.cpp PROPERTIES
    SKIP_UNITY_BUILD_INCLUSION ON
    CXX_SCAN_FOR_MODULES OFF
)
gtest_discover_tests(act_skill_loader_tests
    PROPERTIES TIMEOUT ${ACT_TEST_TIMEOUT}
    DISCOVERY_MODE ${ACT_TEST_DISCOVERY_MODE}
)

# Harness tests
add_executable(act_harness_tests
    harness/SmokeTest.cpp
    harness/ToolInterfaceTest.cpp
    harness/ToolRegistryTest.cpp
    harness/ReadOnlyToolsTest.cpp
    harness/WriteToolsTest.cpp
    harness/ShellSecurityTest.cpp
    harness/ContextManagerTest.cpp
    harness/GitToolsTest.cpp
    harness/GitCommitToolTest.cpp
    harness/GitLogToolTest.cpp
    harness/GitBranchToolTest.cpp
    harness/DiffViewToolTest.cpp
    harness/RepoMapToolTest.cpp
    harness/PermissionPromptTest.cpp
    harness/FileDeleteToolTest.cpp
    harness/DirectoryToolTest.cpp
    harness/BuildToolTest.cpp
    harness/TestRunnerToolTest.cpp
    harness/AskUserToolTest.cpp
    harness/DiagnosticToolTest.cpp
    harness/TodoWriteToolTest.cpp
    harness/SubagentToolTest.cpp
)
target_link_libraries(act_harness_tests PRIVATE act_harness GTest::gtest_main)
gtest_discover_tests(act_harness_tests
    PROPERTIES TIMEOUT ${ACT_TEST_TIMEOUT}
    DISCOVERY_MODE ${ACT_TEST_DISCOVERY_MODE}
)

# Services tests
add_executable(act_services_tests
    services/SmokeTest.cpp
    services/ServiceInterfaceTest.cpp
    services/ConfigManagerTest.cpp
    services/AIEngineTest.cpp
    services/FallbackChainTest.cpp
)
target_link_libraries(act_services_tests PRIVATE act_services GTest::gtest_main)
gtest_discover_tests(act_services_tests
    PROPERTIES TIMEOUT ${ACT_TEST_TIMEOUT}
    DISCOVERY_MODE ${ACT_TEST_DISCOVERY_MODE}
)

# Anthropic converter tests (separate target to avoid MSVC C1903)
add_executable(act_anthropic_converter_tests
    services/AnthropicConverterTest.cpp
)
target_link_libraries(act_anthropic_converter_tests PRIVATE act_services GTest::gtest_main)
set_source_files_properties(services/AnthropicConverterTest.cpp PROPERTIES
    CXX_SCAN_FOR_MODULES OFF
)
gtest_discover_tests(act_anthropic_converter_tests
    PROPERTIES TIMEOUT ${ACT_TEST_TIMEOUT}
    DISCOVERY_MODE ${ACT_TEST_DISCOVERY_MODE}
)

# OpenAI compat converter tests (separate target to avoid MSVC C1903)
add_executable(act_openai_compat_converter_tests
    services/OpenAICompatConverterTest.cpp
)
target_link_libraries(act_openai_compat_converter_tests PRIVATE act_services GTest::gtest_main)
set_source_files_properties(services/OpenAICompatConverterTest.cpp PROPERTIES
    CXX_SCAN_FOR_MODULES OFF
)
gtest_discover_tests(act_openai_compat_converter_tests
    PROPERTIES TIMEOUT ${ACT_TEST_TIMEOUT}
    DISCOVERY_MODE ${ACT_TEST_DISCOVERY_MODE}
)

# Responses converter tests
add_executable(act_responses_converter_tests
    services/ResponsesConverterTest.cpp
)
target_link_libraries(act_responses_converter_tests PRIVATE act_services GTest::gtest_main)
set_source_files_properties(services/ResponsesConverterTest.cpp PROPERTIES
    CXX_SCAN_FOR_MODULES OFF
)
gtest_discover_tests(act_responses_converter_tests
    PROPERTIES TIMEOUT ${ACT_TEST_TIMEOUT}
    DISCOVERY_MODE ${ACT_TEST_DISCOVERY_MODE}
)

# Infrastructure tests
add_executable(act_infrastructure_tests
    infrastructure/SmokeTest.cpp
    infrastructure/InterfaceTest.cpp
    infrastructure/SseParserTest.cpp
)
target_link_libraries(act_infrastructure_tests PRIVATE act_infrastructure GTest::gtest_main)
gtest_discover_tests(act_infrastructure_tests
    PROPERTIES TIMEOUT ${ACT_TEST_TIMEOUT}
    DISCOVERY_MODE ${ACT_TEST_DISCOVERY_MODE}
)

# HttpNetwork tests
add_executable(act_http_network_tests
    infrastructure/HttpNetworkTest.cpp
)
target_link_libraries(act_http_network_tests PRIVATE act_infrastructure GTest::gtest_main)
set_source_files_properties(infrastructure/HttpNetworkTest.cpp PROPERTIES
    CXX_SCAN_FOR_MODULES OFF
)
gtest_discover_tests(act_http_network_tests
    PROPERTIES TIMEOUT ${ACT_TEST_TIMEOUT}
    DISCOVERY_MODE ${ACT_TEST_DISCOVERY_MODE}
)

# Integration tests
add_executable(act_integration_tests
    integration/SmokeTest.cpp
    integration/RegressionTest.cpp
    integration/EndToEndCliTest.cpp
)
target_link_libraries(act_integration_tests PRIVATE act_framework GTest::gtest_main)
gtest_discover_tests(act_integration_tests
    PROPERTIES TIMEOUT ${ACT_TEST_TIMEOUT}
    DISCOVERY_MODE ${ACT_TEST_DISCOVERY_MODE}
)

# Live provider integration tests (needs real API keys)
add_executable(act_live_provider_tests
    integration/LiveProviderTest.cpp
)
target_link_libraries(act_live_provider_tests PRIVATE act_services GTest::gtest_main)
set_source_files_properties(integration/LiveProviderTest.cpp PROPERTIES
    CXX_SCAN_FOR_MODULES OFF
)
# Longer timeout for real network calls
gtest_discover_tests(act_live_provider_tests
    PROPERTIES TIMEOUT 30
    DISCOVERY_MODE ${ACT_TEST_DISCOVERY_MODE}
)
