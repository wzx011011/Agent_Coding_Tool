add_library(act_core STATIC
    core/runtime_version.cpp
    core/runtime_version.h
    core/runtime_event.cpp
    core/runtime_event.h
    core/types.h
    core/error_codes.h
    core/enums.h
)

target_include_directories(act_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(act_core
    PUBLIC
        Qt6::Core
)

target_compile_features(act_core PUBLIC cxx_std_20)
