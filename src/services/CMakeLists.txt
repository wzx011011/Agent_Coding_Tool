add_library(act_services STATIC
    services.cpp
)

target_include_directories(act_services
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(act_services
    PUBLIC
        act_core
        act_infrastructure
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        tomlplusplus::tomlplusplus
)

target_compile_features(act_services PUBLIC cxx_std_20)
