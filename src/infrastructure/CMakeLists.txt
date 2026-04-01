add_library(act_infrastructure STATIC
    feishu_protocol.cpp
    feishu_protocol.h
    feishu_rest_client.cpp
    feishu_rest_client.h
    feishu_ws_client.cpp
    feishu_ws_client.h
    infrastructure.cpp
    http_network.cpp
    http_network.h
    mcp_client.cpp
    mcp_client.h
    mcp_transport.cpp
    mcp_transport.h
    sse_parser.cpp
    sse_parser.h
    interfaces.h
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
