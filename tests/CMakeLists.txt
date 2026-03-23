include(GoogleTest)

add_executable(act_runtime_tests
    core/RuntimeVersionTest.cpp
)

target_link_libraries(act_runtime_tests
    PRIVATE
        act_runtime
        GTest::gtest_main
)

gtest_discover_tests(act_runtime_tests)