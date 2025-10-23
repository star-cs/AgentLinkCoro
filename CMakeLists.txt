cmake_minimum_required(VERSION 3.10)
project(AgentLinkCoro CXX C ASM)
enable_language(ASM)

include(cmake/utils.cmake)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) 

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(
    -rdynamic
    -O0                     # O0 debug（开发），O2 release（编译会慢很多）
    -ggdb
    -Wall
    -Wno-builtin-macro-redefined
    -Wno-deprecated
    -Wno-error=deprecated-declarations
    -Werror
    -Wno-unused-function
)

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

include_directories(.)
include_directories(/usr/local/include)
include_directories(/usr/include/jsoncpp)
include_directories(src)

link_directories(/usr/local/lib)

find_package(OpenSSL REQUIRED)
if(OPENSSL_FOUND)
    include_directories(${OPENSSL_INCLUDE_DIR})
endif()
find_package(Protobuf)
if(Protobuf_FOUND)
    include_directories(${Protobuf_INCLUDE_DIRS})
endif()

file(GLOB_RECURSE SOURCES src/*.cpp src/*.cc src/*.c src/*.S src/*.s)

add_library(agentlinkcoro SHARED ${SOURCES})
force_redefine_file_macro_for_sources(agentlinkcoro)

set(LIBS 
    agentlinkcoro
    dl
    tbb
    pthread
    yaml-cpp
    jsoncpp
    ${OPENSSL_LIBRARIES}
    ${PROTOBUF_LIBRARIES}
    event
    jemalloc
)

x_add_executable(main "src/main.cc" agentlinkcoro ${LIBS})
set_target_properties(main PROPERTIES OUTPUT_NAME "main")

add_subdirectory(tests)
