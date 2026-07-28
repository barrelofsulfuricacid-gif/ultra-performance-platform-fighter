cmake_minimum_required(VERSION 4.4)

project(pf_tracy_capture LANGUAGES C CXX)

set(PF_TRACY_SOURCE_DIR "" CACHE PATH "Locked Tracy source")
set(PF_CAPSTONE_SOURCE_DIR "" CACHE PATH "Locked Capstone source")
set(PF_PPQSORT_SOURCE_DIR "" CACHE PATH "Locked PPQSort source")
set(PF_ZSTD_SOURCE_DIR "" CACHE PATH "Locked Zstd source")

foreach(required_path
        "${PF_TRACY_SOURCE_DIR}/capture/src/capture.cpp"
        "${PF_CAPSTONE_SOURCE_DIR}/include/capstone/capstone.h"
        "${PF_PPQSORT_SOURCE_DIR}/include/ppqsort.h"
        "${PF_ZSTD_SOURCE_DIR}/lib/zstd.h")
    if(NOT EXISTS "${required_path}")
        message(FATAL_ERROR "Missing locked capture source: ${required_path}")
    endif()
endforeach()

find_package(Threads REQUIRED)

set(CAPSTONE_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(CAPSTONE_BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(CAPSTONE_BUILD_LEGACY_TESTS OFF CACHE BOOL "" FORCE)
set(CAPSTONE_BUILD_CSTOOL OFF CACHE BOOL "" FORCE)
set(CAPSTONE_BUILD_CSTEST OFF CACHE BOOL "" FORCE)
set(CAPSTONE_INSTALL OFF CACHE BOOL "" FORCE)
set(CAPSTONE_ARCHITECTURE_DEFAULT OFF CACHE BOOL "" FORCE)
set(CAPSTONE_ARM_SUPPORT ON CACHE BOOL "" FORCE)
set(CAPSTONE_AARCH64_SUPPORT ON CACHE BOOL "" FORCE)
set(CAPSTONE_X86_SUPPORT ON CACHE BOOL "" FORCE)
add_subdirectory(
    "${PF_CAPSTONE_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/capstone"
    EXCLUDE_FROM_ALL)

set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
set(ZSTD_LEGACY_SUPPORT OFF CACHE BOOL "" FORCE)
add_subdirectory(
    "${PF_ZSTD_SOURCE_DIR}/build/cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/zstd"
    EXCLUDE_FROM_ALL)

add_library(pf_ppqsort INTERFACE)
target_include_directories(
    pf_ppqsort
    INTERFACE
        "${PF_PPQSORT_SOURCE_DIR}/include")
target_link_libraries(pf_ppqsort INTERFACE Threads::Threads)

set(pf_tracy_common "${PF_TRACY_SOURCE_DIR}/public/common")
set(pf_tracy_server "${PF_TRACY_SOURCE_DIR}/server")
add_library(
    pf_tracy_server
    STATIC
        "${pf_tracy_common}/tracy_lz4.cpp"
        "${pf_tracy_common}/tracy_lz4hc.cpp"
        "${pf_tracy_common}/TracySocket.cpp"
        "${pf_tracy_common}/TracyStackFrames.cpp"
        "${pf_tracy_common}/TracySystem.cpp"
        "${pf_tracy_server}/TracyMemory.cpp"
        "${pf_tracy_server}/TracyMmap.cpp"
        "${pf_tracy_server}/TracyPrint.cpp"
        "${pf_tracy_server}/TracySysUtil.cpp"
        "${pf_tracy_server}/TracyTaskDispatch.cpp"
        "${pf_tracy_server}/TracyTextureCompression.cpp"
        "${pf_tracy_server}/TracyThreadCompress.cpp"
        "${pf_tracy_server}/TracyWorker.cpp")
target_compile_features(pf_tracy_server PUBLIC cxx_std_20)
target_compile_definitions(pf_tracy_server PUBLIC TRACY_NO_STATISTICS)
target_include_directories(
    pf_tracy_server
    PUBLIC
        "${pf_tracy_common}"
        "${pf_tracy_server}"
        "${PF_CAPSTONE_SOURCE_DIR}/include/capstone")
target_link_libraries(
    pf_tracy_server
    PUBLIC
        Threads::Threads
        capstone_static
        libzstd_static
        pf_ppqsort
        "${CMAKE_DL_LIBS}")

add_library(
    pf_tracy_getopt
    STATIC
        "${PF_TRACY_SOURCE_DIR}/getopt/getopt.c")
target_include_directories(
    pf_tracy_getopt
    PUBLIC
        "${PF_TRACY_SOURCE_DIR}/getopt")

add_executable(
    pf_tracy_capture
    "${PF_TRACY_SOURCE_DIR}/capture/src/capture.cpp")
target_compile_features(pf_tracy_capture PRIVATE cxx_std_20)
target_link_libraries(
    pf_tracy_capture
    PRIVATE
        pf_tracy_server
        pf_tracy_getopt)

if(MSVC)
    target_compile_definitions(
        pf_tracy_server
        PUBLIC
            NOMINMAX
            WIN32_LEAN_AND_MEAN
            _DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR)
    target_link_libraries(pf_tracy_server PUBLIC ws2_32 dbghelp)
endif()
