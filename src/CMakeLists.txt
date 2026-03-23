add_library(act_runtime STATIC
    core/runtime_version.cpp
)

target_include_directories(act_runtime
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(act_runtime
    PUBLIC
        Qt6::Core
)

target_compile_features(act_runtime PUBLIC cxx_std_20)