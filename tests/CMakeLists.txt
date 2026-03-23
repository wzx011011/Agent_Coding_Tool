include(GoogleTest)

# All tests need access to src/ for core headers
set(ACT_SOURCE_DIR ${PROJECT_SOURCE_DIR}/src)

# Core tests
add_executable(act_core_tests
    core/RuntimeVersionTest.cpp
    core/CoreTypesTest.cpp
)
target_include_directories(act_core_tests PRIVATE ${ACT_SOURCE_DIR})
target_link_libraries(act_core_tests PRIVATE act_core Qt6::Core GTest::gtest_main)
gtest_discover_tests(act_core_tests)

# Framework tests
add_executable(act_framework_tests
    framework/SmokeTest.cpp
)
target_link_libraries(act_framework_tests PRIVATE act_framework GTest::gtest_main)
gtest_discover_tests(act_framework_tests)

# Harness tests
add_executable(act_harness_tests
    harness/SmokeTest.cpp
    harness/ToolInterfaceTest.cpp
)
target_link_libraries(act_harness_tests PRIVATE act_harness GTest::gtest_main)
gtest_discover_tests(act_harness_tests)

# Services tests
add_executable(act_services_tests
    services/SmokeTest.cpp
    services/ServiceInterfaceTest.cpp
)
target_link_libraries(act_services_tests PRIVATE act_services GTest::gtest_main)
gtest_discover_tests(act_services_tests)

# Infrastructure tests
add_executable(act_infrastructure_tests
    infrastructure/SmokeTest.cpp
    infrastructure/InterfaceTest.cpp
)
target_link_libraries(act_infrastructure_tests PRIVATE act_infrastructure GTest::gtest_main)
gtest_discover_tests(act_infrastructure_tests)

# Integration tests
add_executable(act_integration_tests
    integration/SmokeTest.cpp
)
target_link_libraries(act_integration_tests PRIVATE act_framework GTest::gtest_main)
gtest_discover_tests(act_integration_tests)
