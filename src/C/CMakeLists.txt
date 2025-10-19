#########################
#       Preamble        #
#########################

cmake_minimum_required(VERSION 3.20)

project(TTM)

# Figure out platform
if(CMAKE_SYSTEM_NAME STREQUAL "MSYS"   )
    set(ISMSYS ON  )
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "CYGWIN" )
    set(ISCYGWIN ON)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(ISWIN ON   )
endif()

function(diff BASELINE TESTDATA RESULTVAR)
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E compare_files ${BASELINE} ${TESTDATA}
        RESULT_VARIABLE RESULT
    )
    if(RESULT EQUAL 0 OR RESULT EQUAL 1)
        set(${RESULTVAR} ${RESULT})
    else()
        message("Diff error")
    endif()
endfunction()

if((NOT ISWIN) AND (NOT ISMSYS) AND (NOT ISCYGWIN))
  set(CTEST_MEMORYCHECK_COMMAND valgrind CACHE STRING "")
endif()

# Set variable to define the build type.
include(GenerateExportHeader)

#########################
#       Build           #
#########################

add_executable(ttm ttm.c)
#target_compile_options(ttm PRIVATE /Zc:preprocessor)

#########################
#       Testing         #
#########################

enable_testing()
include(CTest)

# Test arguments

set(TESTRS ${CMAKE_SOURCE_DIR}/test.rs)
set(TESTARGV a b c)
set(TESTARGS -& -Pshowcall -T -d1 -dt -f ${TESTRS} ${TESTARGV})

# Define tests
add_test(NAME test_all
        COMMAND ${CMAKE_SOURCE_DIR}/ttmtest.sh ${CMAKE_SOURCE_DIR}/test_all
		${CMAKE_CURRENT_BINARY_DIR}/ttm
		-p ${CMAKE_SOURCE_DIR}/test_all.ttm ${TESTARGS})

add_test(NAME test_misc
        COMMAND ${CMAKE_SOURCE_DIR}/ttmtest.sh ${CMAKE_SOURCE_DIR}/test_misc
		${CMAKE_CURRENT_BINARY_DIR}/ttm
		-p ${CMAKE_SOURCE_DIR}/test_misc.ttm ${TESTARGS})

################################
################################
