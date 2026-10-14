cmake_minimum_required(VERSION 3.20)
project(AurivisSimdBenchmarks CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

include(FetchContent)
set(HWY_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(HWY_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
set(HWY_ENABLE_CONTRIB OFF CACHE BOOL "" FORCE)
set(HWY_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_Declare(highway GIT_REPOSITORY https://github.com/google/highway.git GIT_TAG 1.4.0 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(highway)

add_executable(BenchAurivis BenchAurivis.cpp)
target_include_directories(BenchAurivis PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../aurivis_simd/include ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(BenchHighway BenchHighway.cpp)
target_include_directories(BenchHighway PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(BenchHighway PRIVATE hwy)