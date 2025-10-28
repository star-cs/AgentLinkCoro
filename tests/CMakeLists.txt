# 查找所有测试源文件（test_*.cc）
file(GLOB_RECURSE SOURCES test_*.cc)

# 循环生成测试可执行文件，输出到 bin/tests
foreach(test ${SOURCES})
    get_filename_component(test_name ${test} NAME_WE)
    # 跳过 test_module（避免冲突）
    if(test_name STREQUAL "test_module")
        continue()
    endif()
    # 生成测试可执行文件（假设 x_add_executable 是自定义的可执行文件生成函数，类似 add_executable）
    x_add_executable(${test_name} ${test} "agentlinkcoro")
    # Linux 下可执行文件输出目录：bin/tests
    set_target_properties(${test_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/tests
    )
endforeach()

# 生成共享库 my_module（Linux 下为 .so 文件）
add_library(my_module SHARED base/test_module.cc)
force_redefine_file_macro_for_sources(my_module)
target_include_directories(my_module PUBLIC agentlinkcoro)
target_link_libraries(my_module PUBLIC agentlinkcoro)

# Linux 下共享库（.so）输出目录：bin/module
set_target_properties(my_module PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/module
)