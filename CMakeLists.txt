cmake_minimum_required(VERSION 4.4)

project(
    ultra_performance_platform_fighter
    VERSION 0.1.0
    DESCRIPTION "Deterministic authored-C platform fighter"
    LANGUAGES C)

include(CTest)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(PFProjectWarnings)

option(PF_BUILD_HEADLESS "Build the renderer-free headless product" ON)

add_library(sim STATIC src/sim/sim.c)
add_library(pf::sim ALIAS sim)
target_include_directories(
    sim
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
target_compile_features(sim PUBLIC c_std_17)
set_target_properties(
    sim
    PROPERTIES
        C_EXTENSIONS OFF
        OUTPUT_NAME pf_sim)
pf_enable_project_warnings(sim)

get_target_property(pf_sim_link_libraries sim LINK_LIBRARIES)
if(pf_sim_link_libraries)
    message(FATAL_ERROR
        "The deterministic sim target must not link external libraries: "
        "${pf_sim_link_libraries}")
endif()

if(PF_BUILD_HEADLESS)
    add_executable(headless src/headless/main.c)
    target_link_libraries(headless PRIVATE sim)
    target_compile_features(headless PRIVATE c_std_17)
    set_target_properties(headless PROPERTIES C_EXTENSIONS OFF)
    pf_enable_project_warnings(headless)

    get_target_property(pf_headless_link_libraries headless LINK_LIBRARIES)
    if(NOT "${pf_headless_link_libraries}" STREQUAL "sim")
        message(FATAL_ERROR
            "The headless target may explicitly link only sim; got: "
            "${pf_headless_link_libraries}")
    endif()
endif()

if(BUILD_TESTING)
    add_executable(sim_contract_test tests/sim/test_sim_contract.c)
    target_link_libraries(sim_contract_test PRIVATE sim)
    target_compile_features(sim_contract_test PRIVATE c_std_17)
    set_target_properties(sim_contract_test PROPERTIES C_EXTENSIONS OFF)
    pf_enable_project_warnings(sim_contract_test)

    add_test(NAME sim.contract COMMAND sim_contract_test)
    set_tests_properties(
        sim.contract
        PROPERTIES PASS_REGULAR_EXPRESSION
                   "sim-contract=pass abi=1 tick_hz=60")

    if(PF_BUILD_HEADLESS)
        add_test(NAME headless.smoke COMMAND headless --smoke)
        set_tests_properties(
            headless.smoke
            PROPERTIES PASS_REGULAR_EXPRESSION
                       "headless-smoke=pass sim_abi=1 tick_hz=60")
    endif()
endif()
