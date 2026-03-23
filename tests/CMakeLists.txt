include(GoogleTest)

# All tests need access to src/ for core headers
set(ACT_SOURCE_DIR ${PROJECT_SOURCE_DIR}/src)

# Default per-test timeout to prevent hangs (e.g. QEventLoop without QCoreApplication)
set(ACT_TEST_TIMEOUT 10 CACHE STRING "Per-test timeout in seconds")

# Core tests
add_executable(act_core_tests
    core/RuntimeVersionTest.cpp
    core/CoreTypesTest.cpp
)
target_include_directories(act_core_tests PRIVATE ${ACT_SOURCE_DIR})
target_link_libraries(act_core_tests PRIVATE act_core Qt6::Core GTest::gtest_main)
gtest_discover_tests(act_core_tests PROPERTIES TIMEOUT ${ACT_TEST_TIMEOUT})

# Framework tests
add_executable(act_framework_tests
    framework/SmokeTest.cpp
)
target_link_libraries(act_framework_tests PRIVATE act_framework GTest::gtest_main)
gtest_discover_tests(act_framework_tests PROPERTIES TIMEOUT ${ACT_TEST_TIMEOUT})

# Harness tests
add_executable(act_harness_tests
    harness/SmokeTest.cpp
    harness/ToolInterfaceTest.cpp
    harness/ToolRegistryTest.cpp
    harness/ReadOnlyToolsTest.cpp
    harness/WriteToolsTest.cpp
    harness/ShellSecurityTest.cpp
    harness/ContextManagerTest.cpp
)
target_link_libraries(act_harness_tests PRIVATE act_harness GTest::gtest_main)
gtest_discover_tests(act_harness_tests PROPERTIES TIMEOUT ${ACT_TEST_TIMEOUT})

# Services tests
add_executable(act_services_tests
    services/SmokeTest.cpp
    services/ServiceInterfaceTest.cpp
    services/ConfigManagerTest.cpp
    services/AIEngineTest.cpp
)
target_link_libraries(act_services_tests PRIVATE act_services GTest::gtest_main)
gtest_discover_tests(act_services_tests PROPERTIES TIMEOUT ${ACT_TEST_TIMEOUT})

# Infrastructure tests
add_executable(act_infrastructure_tests
    infrastructure/SmokeTest.cpp
    infrastructure/InterfaceTest.cpp
)
target_link_libraries(act_infrastructure_tests PRIVATE act_infrastructure GTest::gtest_main)
gtest_discover_tests(act_infrastructure_tests PROPERTIES TIMEOUT ${ACT_TEST_TIMEOUT})

# Integration tests
add_executable(act_integration_tests
    integration/SmokeTest.cpp
)
target_link_libraries(act_integration_tests PRIVATE act_framework GTest::gtest_main)
gtest_discover_tests(act_integration_tests PROPERTIES TIMEOUT ${ACT_TEST_TIMEOUT})
