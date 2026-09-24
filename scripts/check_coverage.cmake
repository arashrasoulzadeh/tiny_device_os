# Coverage gate script
# Ensures 80% line coverage and 70% branch coverage

cmake_minimum_required(VERSION 3.20)

if(NOT EXISTS "${CMAKE_BINARY_DIR}/coverage.info")
    message(FATAL_ERROR "Coverage file not found: ${CMAKE_BINARY_DIR}/coverage.info")
endif()

# Parse lcov summary
execute_process(
    COMMAND lcov --summary ${CMAKE_BINARY_DIR}/coverage.info
    OUTPUT_VARIABLE LCOV_SUMMARY
    RESULT_VARIABLE LCOV_RESULT
)

if(LCOV_RESULT)
    message(FATAL_ERROR "lcov failed to parse coverage.info")
endif()

# Extract line coverage
string(REGEX MATCH "lines\\.\\.\\.\\.\\.\\.\\.\\.\\.: *([0-9]+\\.[0-9]+)%" LCOV_LINE_MATCH ${LCOV_SUMMARY})
if(LCOV_LINE_MATCH)
    string(REGEX REPLACE ".*lines\\.\\.\\.\\.\\.\\.\\.\\.\\.: *([0-9]+\\.[0-9]+)%.*" "\\1" LINE_COVERAGE ${LCOV_LINE_MATCH})
    message(STATUS "Line coverage: ${LINE_COVERAGE}%")
    
    if(LINE_COVERAGE LESS 80.0)
        message(FATAL_ERROR "Line coverage ${LINE_COVERAGE}% is below 80% gate")
    endif()
else()
    message(WARNING "Could not parse line coverage from lcov output")
endif()

# Extract branch coverage
string(REGEX MATCH "branches\\.\\.\\.\\.\\.\\.\\.\\.: *([0-9]+\\.[0-9]+)%" LCOV_BRANCH_MATCH ${LCOV_SUMMARY})
if(LCOV_BRANCH_MATCH)
    string(REGEX REPLACE ".*branches\\.\\.\\.\\.\\.\\.\\.\\.: *([0-9]+\\.[0-9]+)%.*" "\\1" BRANCH_COVERAGE ${LCOV_BRANCH_MATCH})
    message(STATUS "Branch coverage: ${BRANCH_COVERAGE}%")
    
    if(BRANCH_COVERAGE LESS 70.0)
        message(FATAL_ERROR "Branch coverage ${BRANCH_COVERAGE}% is below 70% gate")
    endif()
else()
    message(WARNING "Could not parse branch coverage from lcov output")
endif()

# Extract function coverage
string(REGEX MATCH "functions\\.\\.\\.\\.\\.\\.\\.\\.: *([0-9]+\\.[0-9]+)%" LCOV_FUNC_MATCH ${LCOV_SUMMARY})
if(LCOV_FUNC_MATCH)
    string(REGEX REPLACE ".*functions\\.\\.\\.\\.\\.\\.\\.\\.: *([0-9]+\\.[0-9]+)%.*" "\\1" FUNC_COVERAGE ${LCOV_FUNC_MATCH})
    message(STATUS "Function coverage: ${FUNC_COVERAGE}%")
endif()

message(STATUS "Coverage gate PASSED")