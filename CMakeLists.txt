cmake_minimum_required (VERSION 3.15)
set(CMAKE_EXPORT_COMPILE_COMMANDS On)

project(LamaInterpreter LANGUAGES CXX C VERSION 0.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED On)

include(cmake/utils.cmake)

set(SOURCES
    ${CMAKE_SOURCE_DIR}/src/Main.cpp
    ${CMAKE_SOURCE_DIR}/src/Interpreter.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})
enable_warnings(${PROJECT_NAME})

set(LAMA_SRCS
    Lama/runtime/runtime.c
    Lama/runtime/gc.c
)
add_library(lama-runtime ${LAMA_SRCS})

target_include_directories(lama-runtime PRIVATE Lama/runtime)
target_link_libraries(${PROJECT_NAME} PRIVATE lama-runtime)

# Lama cannot be built on 64-bit system. So, we need to be compiled like 32-bit library
target_compile_options(lama-runtime PRIVATE "-m32")
target_compile_options(${PROJECT_NAME} PRIVATE "-m32")
target_link_options(${PROJECT_NAME} PRIVATE "-m32" "-fwhole-program")

set_property(TARGET ${PROJECT_NAME}
             PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)


# Lama runtime directory with essential runtime primitives
target_include_directories(${PROJECT_NAME} PRIVATE Lama/runtime)


if(CMAKE_EXPORT_COMPILE_COMMANDS)
  set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES 
      ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
endif()