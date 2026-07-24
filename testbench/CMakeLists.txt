cmake_minimum_required(VERSION 3.28.3)

project(testbench)

include(../common.cmake)

set(SOURCES main.cpp)

add_executable(${PROJECT_NAME} ${SOURCES})

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
    target_include_directories(testbench_realtypes PRIVATE ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(testbench_realtypes
        PRIVATE ${OpenCV_LIBS} Eigen3::Eigen)
    # Debug-friendly, and drop -Werror like the simulator target above.
    target_compile_options(testbench_realtypes PRIVATE
        "$<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wno-error;-g;-O0>")
    message(STATUS "testbench: realtypes fixture enabled (OpenCV + Eigen found)")
else()
    message(STATUS
        "testbench: realtypes fixture skipped -- need OpenCV and Eigen3 "
        "(OpenCV_FOUND=${OpenCV_FOUND} Eigen3_FOUND=${Eigen3_FOUND})")
endif()
