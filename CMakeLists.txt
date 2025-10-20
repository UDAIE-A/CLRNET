cmake_minimum_required(VERSION 3.16)
project(CLRNet LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(clrnet_runtime STATIC
    src/runtime/ScriptRuntime.cpp
)

target_include_directories(clrnet_runtime
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(clrnet_runtime PUBLIC cxx_std_20)

add_executable(clrnet
    src/host/CLRNetHost.cpp
)

target_link_libraries(clrnet
    PRIVATE
        clrnet_runtime
)

include(CTest)
if(BUILD_TESTING)
    add_test(
        NAME clrnet_hello_dry_run
        COMMAND clrnet run ${CMAKE_SOURCE_DIR}/examples/scripts/hello.clr --dry-run --quiet
    )
endif()
