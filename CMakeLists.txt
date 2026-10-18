cmake_minimum_required(VERSION 3.18)

project(CS3_GS_Sorting_Benchmarks
    VERSION 1.0.0
    DESCRIPTION "CS3 and generating-sequence hybrid sorting benchmarks"
    LANGUAGES C
)

# The project uses GNU extensions such as __uint128_t.
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

option(CS3_GS_ENABLE_OPENMP "Enable OpenMP where it is available" ON)
option(CS3_GS_BUILD_FASM "Build the optional FASM-backed benchmark when FASM is installed" ON)
option(CS3_GS_BUILD_EXTRA_TOOLS "Build generator and the standalone CS-sort comparison tool" ON)

find_package(Threads REQUIRED)
if(CS3_GS_ENABLE_OPENMP)
    find_package(OpenMP QUIET COMPONENTS C)
endif()

set(CS3_GS_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(CS3_GS_CORE_SOURCES
    src/sorts_c.c
    src/sorts_instrumented.c
    src/killers.c
    src/mylib_counted.c
    src/mylib_fast.c
)

function(cs3_gs_configure_target target_name)
    target_include_directories(${target_name} PRIVATE "${CS3_GS_INCLUDE_DIR}")
    # MyLib uses a dedicated counted translation unit for menu 12 / mode 11.2.
    # The fast twin locally defines ENABLE_FAST_BENCH inside mylib_fast.c only.
    target_compile_definitions(${target_name} PRIVATE ENABLE_ATOMIC_COUNTERS=1)
    target_link_libraries(${target_name} PRIVATE Threads::Threads m)

    if(OpenMP_C_FOUND)
        target_link_libraries(${target_name} PRIVATE OpenMP::OpenMP_C)
    endif()

    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target_name} PRIVATE
            -Wall
            -Wextra
            -Wformat=2
            -Wstrict-prototypes
        )
    endif()
endfunction()

# FASM objects are assembled as ELF64 and contain non-PIE PC-relative relocations
# (for example calls to pthread_create/pthread_join). Some Linux toolchains enable
# PIE by default, so every executable that links these objects must explicitly be
# compiled and linked as non-PIE. Do not apply this to C-only targets.
function(cs3_gs_configure_fasm_target target_name)
    cs3_gs_configure_target(${target_name})
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target_name} PRIVATE -fno-pie)
        target_link_options(${target_name} PRIVATE -no-pie)
    endif()
endfunction()

# Main command-line benchmark, equivalent to Makefile's build/compare_sort_algorithms.
add_executable(compare_sort_algorithms
    src/compare_sort_algorithms.c
    ${CS3_GS_CORE_SOURCES}
)
cs3_gs_configure_target(compare_sort_algorithms)

# Correctness tests for CS3-GS and its instrumented C implementation.
add_executable(test_cs3_gs
    tests/test_cs3_gs.c
    src/sorts_c.c
    src/sorts_instrumented.c
)
cs3_gs_configure_target(test_cs3_gs)

enable_testing()
add_test(NAME cs3_gs_correctness COMMAND test_cs3_gs)

# Verifies that MyLib sorting adapters produce sorted output and nonzero
# comparison/write counters (RadixSort has zero key comparisons by design).
add_executable(test_mylib_counters
    tests/test_mylib_counters.c
    src/mylib_counted.c
)
cs3_gs_configure_target(test_mylib_counters)
add_test(NAME mylib_operation_counters COMMAND test_mylib_counters)

# Standalone performance comparison: CS3, CS3-GS and libc qsort.
add_executable(benchmark_cs3_gs
    tests/benchmark_cs3_gs.c
    src/sorts_c.c
)
cs3_gs_configure_target(benchmark_cs3_gs)

# Convenience target to execute the correctness suite.
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_cs3_gs
    USES_TERMINAL
)

# Build the legacy interactive main.c as an object-only target, as the old Makefile
# does. The current source snapshot calls run_ai_benchmark(),
# run_performance_benchmark(), and run_complexity_benchmark(), whose implementations
# are not present in the supplied project. Therefore this target intentionally does
# not attempt to link a legacy executable.
add_library(legacy_main_check OBJECT EXCLUDE_FROM_ALL src/main.c)
target_include_directories(legacy_main_check PRIVATE "${CS3_GS_INCLUDE_DIR}")
if(OpenMP_C_FOUND)
    target_link_libraries(legacy_main_check PRIVATE OpenMP::OpenMP_C)
endif()
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(legacy_main_check PRIVATE -Wall -Wextra)
endif()
add_custom_target(check_main DEPENDS legacy_main_check)

if(CS3_GS_BUILD_EXTRA_TOOLS)
    add_executable(generator src/generator.c)
    target_link_libraries(generator PRIVATE m)

    # The standalone tool uses the original mylib.cs_sort implementation.
    add_executable(compare_cs_sort
        src/compare_cs_sort.c
        src/sorts_c.c
        src/mylib_counted.c
        src/killers.c
    )
    cs3_gs_configure_target(compare_cs_sort)
endif()

# Optional FASM-backed benchmark (x86-64 ELF only). When FASM is absent, the regular
# C benchmark and tests remain available; CMake simply omits this target.
if(CS3_GS_BUILD_FASM AND UNIX AND NOT APPLE AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    find_program(FASM_EXECUTABLE NAMES fasm FASM)
    if(FASM_EXECUTABLE)
        set(FASM_SORTS_OBJECT "${CMAKE_CURRENT_BINARY_DIR}/sorts_fasm.o")
        set(FASM_INSTRUMENTED_OBJECT "${CMAKE_CURRENT_BINARY_DIR}/sorts_instrumented_fasm.o")

        add_custom_command(
            OUTPUT "${FASM_SORTS_OBJECT}"
            COMMAND "${FASM_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/src/sorts.asm" "${FASM_SORTS_OBJECT}"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/sorts.asm"
            COMMENT "Assembling sorts.asm with FASM"
            VERBATIM
        )
        add_custom_command(
            OUTPUT "${FASM_INSTRUMENTED_OBJECT}"
            COMMAND "${FASM_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/src/sorts_instrumented.asm" "${FASM_INSTRUMENTED_OBJECT}"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/sorts_instrumented.asm"
            COMMENT "Assembling sorts_instrumented.asm with FASM"
            VERBATIM
        )
        set_source_files_properties(
            "${FASM_SORTS_OBJECT}" "${FASM_INSTRUMENTED_OBJECT}"
            PROPERTIES GENERATED TRUE EXTERNAL_OBJECT TRUE
        )

        add_executable(compare_sort_algorithms_fasm
            src/compare_sort_algorithms.c
            ${CS3_GS_CORE_SOURCES}
            "${FASM_SORTS_OBJECT}"
            "${FASM_INSTRUMENTED_OBJECT}"
        )
        target_compile_definitions(compare_sort_algorithms_fasm PRIVATE HAVE_SORTS_FASM=1)
        cs3_gs_configure_fasm_target(compare_sort_algorithms_fasm)
        message(STATUS "FASM found: compare_sort_algorithms_fasm target is enabled (non-PIE)")

        # Restore the original interactive application. ai_benchmark.c provides
        # run_ai_benchmark(), run_performance_benchmark(), and
        # run_complexity_benchmark(), while compare_sort_algorithms.c is compiled
        # with SORT_COMPARE_EMBEDDED to provide the menu function without a second main().
        add_library(legacy_compare_menu OBJECT src/compare_sort_algorithms.c)

        target_compile_definitions(legacy_compare_menu PRIVATE
            SORT_COMPARE_EMBEDDED=1
            HAVE_SORTS_FASM=1
        )

        target_include_directories(legacy_compare_menu PRIVATE
            "${CS3_GS_INCLUDE_DIR}"
        )
        if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(legacy_compare_menu PRIVATE -Wall -Wextra)
        endif()

        add_executable(sorting_app
            src/main.c
            src/ai_benchmark.c
            ${CS3_GS_CORE_SOURCES}
            $<TARGET_OBJECTS:legacy_compare_menu>
            "${FASM_SORTS_OBJECT}"
            "${FASM_INSTRUMENTED_OBJECT}"
        )
        cs3_gs_configure_fasm_target(sorting_app)
        target_compile_definitions(sorting_app PRIVATE HAVE_SORTS_FASM=1)
        message(STATUS "FASM found: sorting_app (interactive legacy menu, non-PIE) is enabled")
    else()
        message(STATUS "FASM not found: building C-only targets. Install FASM to enable sorting_app and the ASM benchmark.")
    endif()
endif()

# Keep the short, discoverable target names from the Makefile.
add_custom_target(benchmark DEPENDS compare_sort_algorithms)
add_custom_target(benchmark_hybrid DEPENDS benchmark_cs3_gs)
