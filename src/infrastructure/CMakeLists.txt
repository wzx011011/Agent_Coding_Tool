add_library(act_infrastructure STATIC
    infrastructure.cpp
)

target_include_directories(act_infrastructure
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(act_infrastructure
    PUBLIC
        Qt6::Core
)

target_compile_features(act_infrastructure PUBLIC cxx_std_20)
