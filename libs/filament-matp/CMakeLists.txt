cmake_minimum_required(VERSION 3.19)
project(matp)

set(TARGET matp)
set(PUBLIC_HDR_DIR include)

# ==================================================================================================
# Sources and headers
# ==================================================================================================
set(PUBLIC_HDRS
        include/filament-matp/Config.h
        include/filament-matp/MaterialParser.h
)

set(SRCS
        src/DirIncluder.h
        src/JsonishLexeme.h
        src/JsonishLexer.h
        src/JsonishParser.h
        src/Lexeme.h
        src/Lexer.h
        src/MaterialLexeme.h
        src/MaterialLexer.h
        src/ParametersProcessor.h
        src/DirIncluder.cpp
        src/JsonishLexer.cpp
        src/JsonishParser.cpp
        src/MaterialLexer.cpp
        src/MaterialParser.cpp
        src/ParametersProcessor.cpp
)

# ==================================================================================================
# Include and target definitions
# ==================================================================================================
include_directories(${PUBLIC_HDR_DIR})

add_library(${TARGET} STATIC ${PUBLIC_HDRS} ${SRCS})

target_include_directories(${TARGET} PUBLIC ${PUBLIC_HDR_DIR})
target_include_directories(${TARGET} PRIVATE ${filamat_SOURCE_DIR}/src)

set_target_properties(${TARGET} PROPERTIES FOLDER Libs)
target_link_libraries(${TARGET} getopt filamat filabridge utils)

# ==================================================================================================
# Installation
# ==================================================================================================
install(TARGETS ${TARGET} ARCHIVE DESTINATION lib/${DIST_DIR})
install(DIRECTORY ${PUBLIC_HDR_DIR}/ DESTINATION include)

# ==================================================================================================
# Tests
# ==================================================================================================
project(test_matp)
set(TARGET test_matp)
set(SRCS
    tests/test_matp.cpp
    tests/test_includer.cpp
    tests/TestMaterialParser.h
    tests/test_compute_material.cpp
)

add_executable(${TARGET} ${SRCS})

target_link_libraries(${TARGET} matp gtest)

set_target_properties(test_matp PROPERTIES FOLDER Tests)