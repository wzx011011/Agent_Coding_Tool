add_library(act_vscode_protocol STATIC
    vscode_protocol.cpp
)

target_include_directories(act_vscode_protocol
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(act_vscode_protocol
    PUBLIC
        act_framework
)

target_compile_features(act_vscode_protocol PUBLIC cxx_std_20)
