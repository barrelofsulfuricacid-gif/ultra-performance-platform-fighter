cmake_minimum_required(VERSION 4.4)

project(
    ultra_performance_platform_fighter
    VERSION 0.1.0
    DESCRIPTION "Deterministic authored-C platform fighter"
    LANGUAGES C)

include(CTest)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(PFBuildModes)
include(PFDeterministicBoundary)
include(PFProjectWarnings)

option(PF_BUILD_HEADLESS "Build the renderer-free headless product" ON)
option(PF_BUILD_NATIVE_CLIENT "Build the native-client product boundary" ON)
option(PF_BUILD_WEB_CLIENT "Build the web-client product boundary" ON)
option(PF_BUILD_TOOLS "Build the content/development tools boundary" ON)
option(PF_BUILD_BENCHMARKS "Build the benchmark product boundary" ON)
option(PF_BUILD_VERIFIER "Build the verifier product boundary" ON)
option(PF_ENABLE_SANITIZERS "Enable the supported address/UB sanitizers" OFF)
option(PF_ENABLE_PROFILING "Preserve frame pointers and profiling seams" OFF)

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
pf_apply_build_mode(sim)
pf_assert_deterministic_target(sim)

if(PF_BUILD_HEADLESS)
    add_executable(headless src/headless/main.c)
    target_link_libraries(headless PRIVATE sim)
    target_compile_features(headless PRIVATE c_std_17)
    set_target_properties(headless PROPERTIES C_EXTENSIONS OFF)
    pf_enable_project_warnings(headless)
    pf_apply_build_mode(headless)

    get_target_property(pf_headless_link_libraries headless LINK_LIBRARIES)
    if(NOT "${pf_headless_link_libraries}" STREQUAL "sim")
        message(FATAL_ERROR
            "The headless target may explicitly link only sim; got: "
            "${pf_headless_link_libraries}")
    endif()
endif()

function(pf_add_smoke_product target product_name)
    add_executable(${target} src/product/product_main.c)
    target_link_libraries(${target} PRIVATE sim)
    target_compile_features(${target} PRIVATE c_std_17)
    target_compile_definitions(
        ${target}
        PRIVATE
            "PF_PRODUCT_NAME=\"${product_name}\"")
    set_target_properties(${target} PROPERTIES C_EXTENSIONS OFF)
    pf_enable_project_warnings(${target})
    pf_apply_build_mode(${target})

    get_target_property(pf_product_link_libraries ${target} LINK_LIBRARIES)
    if(NOT "${pf_product_link_libraries}" STREQUAL "sim")
        message(FATAL_ERROR
            "${target} may explicitly link only sim in the M1 boundary "
            "smoke; got: ${pf_product_link_libraries}")
    endif()

    if(EMSCRIPTEN AND "${target}" STREQUAL "web_client")
        set(pf_web_adapter
            "${CMAKE_CURRENT_SOURCE_DIR}/src/web_client/web_adapter.js")
        target_compile_definitions(${target} PRIVATE PF_PRODUCT_BROWSER=1)
        target_link_options(
            ${target}
            PRIVATE
                "-sASSERTIONS=1"
                "--js-library=${pf_web_adapter}")
        set_property(
            TARGET ${target}
            APPEND
            PROPERTY LINK_DEPENDS "${pf_web_adapter}")
        set_target_properties(${target} PROPERTIES SUFFIX ".html")
    else()
        target_compile_definitions(${target} PRIVATE PF_PRODUCT_BROWSER=0)
    endif()

    if(BUILD_TESTING AND
       NOT (EMSCRIPTEN AND "${target}" STREQUAL "web_client"))
        add_test(NAME ${target}.smoke COMMAND ${target} --smoke)
        set_tests_properties(
            ${target}.smoke
            PROPERTIES PASS_REGULAR_EXPRESSION
                       "${product_name}-smoke=pass sim_abi=1 tick_hz=60")
    endif()
endfunction()

if(PF_BUILD_NATIVE_CLIENT)
    pf_add_smoke_product(native_client "native-client")
    set_target_properties(
        native_client
        PROPERTIES OUTPUT_NAME pf_native_client)
endif()

if(PF_BUILD_WEB_CLIENT)
    pf_add_smoke_product(web_client "web-client")
    if(NOT EMSCRIPTEN)
        set_target_properties(
            web_client
            PROPERTIES OUTPUT_NAME pf_web_client_host_smoke)
    endif()
endif()

if(PF_BUILD_TOOLS)
    pf_add_smoke_product(tools "tools")
    set_target_properties(tools PROPERTIES OUTPUT_NAME pf_tools)
endif()

if(PF_BUILD_BENCHMARKS)
    pf_add_smoke_product(benchmarks "benchmarks")
    set_target_properties(
        benchmarks
        PROPERTIES OUTPUT_NAME pf_benchmarks)
endif()

if(PF_BUILD_VERIFIER)
    pf_add_smoke_product(verifier "verifier")
    set_target_properties(verifier PROPERTIES OUTPUT_NAME pf_verifier)
endif()

if(BUILD_TESTING)
    add_executable(sim_contract_test tests/sim/test_sim_contract.c)
    target_link_libraries(sim_contract_test PRIVATE sim)
    target_compile_features(sim_contract_test PRIVATE c_std_17)
    set_target_properties(sim_contract_test PROPERTIES C_EXTENSIONS OFF)
    pf_enable_project_warnings(sim_contract_test)
    pf_apply_build_mode(sim_contract_test)

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
