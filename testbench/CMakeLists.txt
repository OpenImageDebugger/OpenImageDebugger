cmake_minimum_required(VERSION 3.28.3)

project(testbench)

include(../common.cmake)

set(SOURCES main.cpp)

# Every fixture lands directly in the build directory, whatever the
# generator. The $<1:...> wrapper is what stops a multi-config generator
# (Visual Studio) from appending its own per-config subdirectory, so the
# launch configs in .vscode/ can name one path.
set(TESTBENCH_OUTPUT_DIR "$<1:${CMAKE_BINARY_DIR}>")

add_executable(${PROJECT_NAME} ${SOURCES})
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TESTBENCH_OUTPUT_DIR}")

# Remove -Werror for testbench (test code doesn't need strict error handling)
# We need to override the -Werror from common.cmake
target_compile_options(${PROJECT_NAME} PRIVATE
    "$<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wno-error>")

target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)

# Optional fixture linking real OpenCV + Eigen, holding one live instance of
# every built-in buffer type for manual plotting under gdb/lldb. Built only
# when both libraries are found, so the dependency-free simulator above still
# builds everywhere.
find_package(OpenCV QUIET)
find_package(Eigen3 QUIET)
if(OpenCV_FOUND AND Eigen3_FOUND)
    add_executable(testbench_realtypes realtypes.cpp)
    set_target_properties(testbench_realtypes PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${TESTBENCH_OUTPUT_DIR}")
    target_include_directories(testbench_realtypes PRIVATE ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(testbench_realtypes
        PRIVATE ${OpenCV_LIBS} Eigen3::Eigen Threads::Threads)
    # Debug-friendly, and drop -Werror like the simulator target above.
    target_compile_options(testbench_realtypes PRIVATE
        "$<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wno-error;-g;-O0>")
    message(STATUS "testbench: realtypes fixture enabled (OpenCV + Eigen found)")
else()
    message(STATUS
        "testbench: realtypes fixture skipped -- need OpenCV and Eigen3 "
        "(OpenCV_FOUND=${OpenCV_FOUND} Eigen3_FOUND=${Eigen3_FOUND})")
endif()

# The issue #1102 sample. Eigen only: it must build wherever Eigen does,
# with or without OpenCV.
if(Eigen3_FOUND)
    add_executable(testbench_inherited_eigen inherited_eigen.cpp)
    set_target_properties(testbench_inherited_eigen PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${TESTBENCH_OUTPUT_DIR}")
    target_link_libraries(testbench_inherited_eigen PRIVATE Eigen3::Eigen)
    # The sample declares a local it never reads -- that IS the buffer the
    # reporter plots -- so /W4 C4101 plus the repo-wide /WX would fail the
    # Windows build on the one line the fixture exists for.
    target_compile_options(testbench_inherited_eigen PRIVATE
        "$<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wno-error;-g;-O0>"
        "$<$<CXX_COMPILER_ID:MSVC>:/WX-;/Zi;/Od>")
    message(STATUS "testbench: inherited_eigen fixture enabled (Eigen found)")
endif()
