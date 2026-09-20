cmake_minimum_required(VERSION 3.10)
project(SortBenchmark C)

set(CMAKE_C_STANDARD 99)

find_package(OpenMP REQUIRED)
find_package(Threads REQUIRED)
find_program(FASM_EXECUTABLE fasm REQUIRED)

set(ASM_SRC ${CMAKE_CURRENT_SOURCE_DIR}/sorts.asm)
set(ASM_OBJ ${CMAKE_CURRENT_BINARY_DIR}/sorts.o)

add_custom_command(
    OUTPUT ${ASM_OBJ}
    COMMAND ${FASM_EXECUTABLE} ${ASM_SRC} ${ASM_OBJ}
    DEPENDS ${ASM_SRC}
    COMMENT "Компиляция ассемблерного модуля sorts_2.asm с помощью FASM..."
)

set_source_files_properties(${ASM_OBJ} PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
add_executable(sort_test main.c sorts_c.c ${ASM_OBJ})
target_link_libraries(sort_test PRIVATE OpenMP::OpenMP_C Threads::Threads -no-pie m)
target_compile_options(sort_test PRIVATE -O3)
