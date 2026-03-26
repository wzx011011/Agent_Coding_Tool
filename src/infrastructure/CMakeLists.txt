find_package(httplib CONFIG REQUIRED)

add_library(act_infrastructure STATIC
    feishu_protocol.cpp
    feishu_rest_client.cpp
    feishu_ws_client.cpp
    infrastructure.cpp
    http_network.cpp
    sse_parser.cpp
)

target_include_directories(act_infrastructure
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(act_infrastructure
    PUBLIC
        Qt6::Core
        httplib::httplib
        ixwebsocket::ixwebsocket
        spdlog::spdlog
)

target_compile_features(act_infrastructure PUBLIC cxx_std_20)
