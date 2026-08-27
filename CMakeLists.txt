cmake_minimum_required(VERSION 3.16)
project(flowmaticc LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(FLOWMATIC_SANITIZE "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

set(FLOWMATIC_LIB_SOURCES
    src/arena.c
    src/word.c
    src/source.c
    src/diagnostic.c
    src/lexer.c
    src/parser.c
    src/data_design.c
    src/semantic.c
    src/control_flow.c
    src/translation.c
    src/selection.c
    src/allocation.c
    src/processing.c
    src/c_backend.c
    src/edited_record.c
    src/x1.c
    src/dump.c
    src/program.c
    src/unit.c
    src/compile.c
)

add_library(flowmatic_runtime STATIC src/runtime.c)
target_include_directories(flowmatic_runtime
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_library(flowmatic STATIC ${FLOWMATIC_LIB_SOURCES})
target_include_directories(flowmatic
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(flowmatic PUBLIC flowmatic_runtime)

add_executable(flowmaticc src/cli.c)
target_link_libraries(flowmaticc PRIVATE flowmatic)
target_include_directories(flowmaticc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

function(flowmatic_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wshadow -Wstrict-prototypes
        )
    endif()
endfunction()

flowmatic_warnings(flowmatic)
flowmatic_warnings(flowmatic_runtime)
flowmatic_warnings(flowmaticc)

if(FLOWMATIC_SANITIZE)
    if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(flowmatic PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(flowmatic PRIVATE -fsanitize=address,undefined)
        target_compile_options(flowmatic_runtime PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(flowmatic_runtime PRIVATE -fsanitize=address,undefined)
        target_compile_options(flowmaticc PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(flowmaticc PRIVATE -fsanitize=address,undefined)
    else()
        message(WARNING "FLOWMATIC_SANITIZE is not supported for this compiler")
    endif()
endif()

enable_testing()

set(FLOWMATIC_TEST_SOURCES
    tests/unit/test_word.c
    tests/unit/test_lexer.c
    tests/unit/test_parser.c
    tests/unit/test_data_design.c
    tests/unit/test_semantic.c
    tests/unit/test_x1.c
    tests/unit/test_phases.c
    tests/unit/test_negative.c
    tests/unit/test_runtime_arith.c
)

foreach(test_src ${FLOWMATIC_TEST_SOURCES})
    get_filename_component(test_name ${test_src} NAME_WE)
    add_executable(${test_name} ${test_src} tests/unit/test_harness.c)
    target_link_libraries(${test_name} PRIVATE flowmatic flowmatic_runtime)
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/unit
    )
    flowmatic_warnings(${test_name})
    if(FLOWMATIC_SANITIZE AND CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${test_name} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${test_name} PRIVATE -fsanitize=address,undefined)
    endif()
    add_test(NAME ${test_name} COMMAND ${test_name})
    set_tests_properties(${test_name} PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endforeach()

add_executable(test_integration tests/integration/test_integration.c tests/unit/test_harness.c)
target_link_libraries(test_integration PRIVATE flowmatic flowmatic_runtime)
target_include_directories(test_integration PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/unit
)
flowmatic_warnings(test_integration)
if(FLOWMATIC_SANITIZE AND CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_integration PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test_integration PRIVATE -fsanitize=address,undefined)
endif()
add_test(NAME test_integration COMMAND test_integration)
set_tests_properties(test_integration PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

set(FLOWMATIC_FIXTURE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures)
add_test(NAME cli_help COMMAND flowmaticc --help)
add_test(NAME cli_version COMMAND flowmaticc --version)
add_test(
    NAME cli_check_sample1
    COMMAND flowmaticc --check
            --design ${FLOWMATIC_FIXTURE_DIR}/manual/sample1/inventory.dd
            --design ${FLOWMATIC_FIXTURE_DIR}/manual/sample1/price.dd
            --design ${FLOWMATIC_FIXTURE_DIR}/manual/sample1/priced-inv.dd
            --design ${FLOWMATIC_FIXTURE_DIR}/manual/sample1/unpriced-inv.dd
            --code ${FLOWMATIC_FIXTURE_DIR}/manual/sample1/code.fm
)
add_test(
    NAME cli_check_sample2
    COMMAND flowmaticc --check
            --design ${FLOWMATIC_FIXTURE_DIR}/manual/sample1/inventory.dd
            --design ${FLOWMATIC_FIXTURE_DIR}/manual/sample1/price.dd
            --design ${FLOWMATIC_FIXTURE_DIR}/manual/sample1/priced-inv.dd
            --design ${FLOWMATIC_FIXTURE_DIR}/manual/sample1/unpriced-inv.dd
            --design ${FLOWMATIC_FIXTURE_DIR}/manual/sample2/error.dd
            --w-storage ${FLOWMATIC_FIXTURE_DIR}/manual/sample2/wstorage.dd
            --directory ${FLOWMATIC_FIXTURE_DIR}/manual/sample2/directory.dir
            --code ${FLOWMATIC_FIXTURE_DIR}/manual/sample2/code.fm
)
add_test(
    NAME cli_check_sample3
    COMMAND flowmaticc --check
            --design ${FLOWMATIC_FIXTURE_DIR}/manual/sample1/inventory.dd
            --design ${FLOWMATIC_FIXTURE_DIR}/manual/sample1/price.dd
            --design ${FLOWMATIC_FIXTURE_DIR}/manual/sample1/priced-inv.dd
            --design ${FLOWMATIC_FIXTURE_DIR}/manual/sample1/unpriced-inv.dd
            --w-storage ${FLOWMATIC_FIXTURE_DIR}/manual/sample3/wstorage.dd
            --directory ${FLOWMATIC_FIXTURE_DIR}/manual/sample3/directory.dir
            --code ${FLOWMATIC_FIXTURE_DIR}/manual/sample3/code.fm
            --x1 ${FLOWMATIC_FIXTURE_DIR}/manual/sample3/x1.sec
)
add_test(
    NAME cli_dump_sample1
    COMMAND flowmaticc
            --dump-op1 ${CMAKE_CURRENT_BINARY_DIR}/op1.txt
            --dump-op2 ${CMAKE_CURRENT_BINARY_DIR}/op2.txt
            --dump-op3 ${CMAKE_CURRENT_BINARY_DIR}/op3.txt
            --dump-library ${CMAKE_CURRENT_BINARY_DIR}/lib.txt
            --edited-record ${CMAKE_CURRENT_BINARY_DIR}/edited.txt
            --design ${FLOWMATIC_FIXTURE_DIR}/manual/sample1/inventory.dd
            --design ${FLOWMATIC_FIXTURE_DIR}/manual/sample1/price.dd
            --design ${FLOWMATIC_FIXTURE_DIR}/manual/sample1/priced-inv.dd
            --design ${FLOWMATIC_FIXTURE_DIR}/manual/sample1/unpriced-inv.dd
            --code ${FLOWMATIC_FIXTURE_DIR}/manual/sample1/code.fm
)
add_test(
    NAME cli_sample3_compile_without_hooks
    COMMAND flowmaticc --compile
            --design ${FLOWMATIC_FIXTURE_DIR}/manual/sample1/inventory.dd
            --design ${FLOWMATIC_FIXTURE_DIR}/manual/sample1/price.dd
            --design ${FLOWMATIC_FIXTURE_DIR}/manual/sample1/priced-inv.dd
            --design ${FLOWMATIC_FIXTURE_DIR}/manual/sample1/unpriced-inv.dd
            --w-storage ${FLOWMATIC_FIXTURE_DIR}/manual/sample3/wstorage.dd
            --directory ${FLOWMATIC_FIXTURE_DIR}/manual/sample3/directory.dir
            --code ${FLOWMATIC_FIXTURE_DIR}/manual/sample3/code.fm
            --x1 ${FLOWMATIC_FIXTURE_DIR}/manual/sample3/x1.sec
            -o ${CMAKE_CURRENT_BINARY_DIR}/sample3.c
)
set_tests_properties(cli_sample3_compile_without_hooks PROPERTIES WILL_FAIL TRUE)
