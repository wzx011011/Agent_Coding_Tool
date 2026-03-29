add_library(act_services STATIC
    services.cpp
    config_manager.cpp
    config_manager.h
    anthropic_provider.cpp
    anthropic_provider.h
    openai_compat_provider.cpp
    openai_compat_provider.h
    responses_provider.cpp
    responses_provider.h
    anthropic_converter.cpp
    anthropic_converter.h
    openai_compat_converter.cpp
    openai_compat_converter.h
    responses_converter.cpp
    responses_converter.h
    ai_engine.cpp
    ai_engine.h
    interfaces.h
    llm_provider.h
)

target_include_directories(act_services
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(act_services
    PUBLIC
        act_core
        act_infrastructure
        nlohmann_json::nlohmann_json
        tomlplusplus::tomlplusplus
)

target_compile_features(act_services PUBLIC cxx_std_20)
