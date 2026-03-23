add_executable(aictl
    main.cpp
)

target_include_directories(aictl
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(aictl
    PRIVATE
        act_core
        act_framework
        act_services
        Qt6::Core
)

target_compile_features(aictl PRIVATE cxx_std_20)
