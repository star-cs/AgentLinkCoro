file(GLOB_RECURSE SOURCES test_*.cc)
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin/tests)

foreach(test ${SOURCES})
    get_filename_component(test_name ${test} NAME_WE)
    x_add_executable(${test_name} ${test} agentlinkcoro "${LIBS}")
endforeach()
