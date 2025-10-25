cmake_minimum_required(VERSION 3.10)
project(AgentLinkCoro CXX C ASM)
enable_language(ASM)

include(cmake/utils.cmake)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) 

set(CMAKE_CXX_STANDARD 17)
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

include_directories(/usr/local/include) 
include_directories(src)
link_directories(/usr/local/lib)

find_package(yaml-cpp REQUIRED)
find_package(jsoncpp REQUIRED)
find_package(protobuf REQUIRED)
find_package(Libevent REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(jemalloc REQUIRED)
find_package(zookeeper-client-c REQUIRED)
find_package(prometheus-cpp REQUIRED)
find_package(RdKafka REQUIRED)
# find_package(hiredis REQUIRED)
# find_package(libmysqlclient REQUIRED)
# find_package(SQLite3 REQUIRED)

# Collect source files
file(GLOB_RECURSE SOURCES src/*.cpp src/*.cc src/*.c src/*.S src/*.s)

# Generate protobuf sources
set(PROTOBUF_SOURCES "")
protobufmaker(src/base/net/ns/ns_protobuf.proto PROTOBUF_SOURCES ${CMAKE_CURRENT_BINARY_DIR})
protobufmaker(src/base/net/proto/logserver.proto PROTOBUF_SOURCES ${CMAKE_CURRENT_BINARY_DIR})

# Generate ragel sources
set(RAGEL_SOURCES "")
ragelmaker(src/base/net/http/http11_parser.rl RAGEL_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/base/net/http)
ragelmaker(src/base/net/http/httpclient_parser.rl RAGEL_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/base/net/http)
ragelmaker(src/base/net/uri.rl RAGEL_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/base/net)

# Combine all sources
set(ALL_SOURCES ${SOURCES} ${PROTOBUF_SOURCES} ${RAGEL_SOURCES})

add_library(agentlinkcoro SHARED ${ALL_SOURCES})
force_redefine_file_macro_for_sources(agentlinkcoro)
target_include_directories(agentlinkcoro PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/src/base/net/ns
    ${CMAKE_CURRENT_BINARY_DIR}/src/base/net/proto
    ${PROTOBUF_INCLUDE_DIRS}
)

target_link_libraries(agentlinkcoro PUBLIC
    dl
    tbb
    pthread
    hiredis_vip
    mysqlclient
    sqlite3
    yaml-cpp::yaml-cpp 
    JsonCpp::JsonCpp
    protobuf::protobuf 
    libevent::libevent 
    openssl::openssl 
    jemalloc::jemalloc 
    zookeeper-client-c::zookeeper-client-c 
    prometheus-cpp::prometheus-cpp
    RdKafka::rdkafka++
)

x_add_executable(main "src/main.cc" "agentlinkcoro")
set_target_properties(main PROPERTIES OUTPUT_NAME "main")

add_subdirectory(tests)