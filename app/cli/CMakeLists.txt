add_executable(aictl
    main.cpp
)

target_link_libraries(aictl
    PRIVATE
        act_runtime
        Qt6::Core
)

target_compile_features(aictl PRIVATE cxx_std_20)