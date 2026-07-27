cmake_minimum_required(VERSION 3.24)

project(m0_movement_playtest LANGUAGES C)

option(M0_BUILD_INTERACTIVE "Build the SDL3 movement playtest" ON)
option(M0_FETCH_SDL3 "Fetch the pinned SDL3 archive when not installed" ON)

add_library(m0_movement_model STATIC movement_model.c)
target_include_directories(m0_movement_model PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(m0_movement_model PUBLIC c_std_17)

if(MSVC)
    target_compile_options(m0_movement_model PRIVATE /W4 /WX)
else()
    target_compile_options(
        m0_movement_model PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_executable(m0_movement_verify movement_verify.c)
target_link_libraries(m0_movement_verify PRIVATE m0_movement_model)
if(NOT MSVC)
    target_link_libraries(m0_movement_verify PRIVATE m)
    target_compile_options(
        m0_movement_verify PRIVATE -Wall -Wextra -Wpedantic -Werror)
else()
    target_compile_options(m0_movement_verify PRIVATE /W4 /WX)
endif()

include(CTest)
add_test(NAME m0_movement_verify COMMAND m0_movement_verify)

if(M0_BUILD_INTERACTIVE)
    find_package(SDL3 3.4.12 EXACT CONFIG QUIET)
    if(NOT SDL3_FOUND)
        if(NOT M0_FETCH_SDL3)
            message(FATAL_ERROR
                "SDL3 3.4.12 was not found and M0_FETCH_SDL3 is OFF")
        endif()
        include(FetchContent)
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON CACHE BOOL "" FORCE)
        set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
        set(SDL_TESTS OFF CACHE BOOL "" FORCE)
        set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(
            SDL3
            URL
                "https://github.com/libsdl-org/SDL/releases/download/release-3.4.12/SDL3-3.4.12.tar.gz"
            URL_HASH
                "SHA256=f07b958a9ac5020fb7a44cadb957f658b2149c3c8abb4f63145fac9303249db7"
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
        FetchContent_MakeAvailable(SDL3)
    endif()

    add_executable(m0_movement_playtest movement_playtest.c)
    target_link_libraries(
        m0_movement_playtest PRIVATE m0_movement_model SDL3::SDL3)
    if(MSVC)
        target_compile_options(m0_movement_playtest PRIVATE /W4 /WX)
    else()
        target_compile_options(
            m0_movement_playtest PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endif()
