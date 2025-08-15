cmake_minimum_required(VERSION 3.20)

project(TTM)

add_executable(ttm ttm.c)
target_compile_options(ttm PRIVATE /Zc:preprocessor)

enable_testing()

set(TESTRS test.rs)
set(TESTARGV a b c)
set(TESTARGS -& -Pshowcall -T -d1 -dt -f ${TESTRS} ${TESTARGV})

add_test(NAME test_all.ttm  COMMAND ttm -p ${CMAKE_CURRENT_SOURCE_DIR}/test_all.ttm  ${TESTARGS})
add_test(NAME test_misc.ttm COMMAND ttm -p ${CMAKE_CURRENT_SOURCE_DIR}/test_misc.ttm ${TESTARGS})
