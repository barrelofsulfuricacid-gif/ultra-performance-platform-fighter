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
option(PF_ENABLE_SDL_CLIENT "Build the native client against locked SDL3" OFF)
option(
    PF_SDL_UNIX_CONSOLE_BUILD
    "Allow a Unix SDL build without X11/Wayland (restricted environments only)"
    OFF)
set(
    PF_SDL_SOURCE_DIR
    ""
    CACHE PATH
    "Path to the checksum-verified SDL3 source tree")

add_library(
    sim
    STATIC
        src/sim/sim.c
        src/sim/sim_internal.h
        src/sim/sim_sha256.c
        src/sim/sim_sha256.h
        src/sim/sim_snapshot.c
        src/sim/sim_tick.c)
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

add_library(presentation STATIC src/presentation/render_packet.c)
add_library(pf::presentation ALIAS presentation)
target_include_directories(
    presentation
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
target_compile_features(presentation PUBLIC c_std_17)
set_target_properties(
    presentation
    PROPERTIES
        C_EXTENSIONS OFF
        OUTPUT_NAME pf_presentation)
pf_enable_project_warnings(presentation)
pf_apply_build_mode(presentation)

if(PF_ENABLE_SDL_CLIENT)
    if(NOT EXISTS "${PF_SDL_SOURCE_DIR}/CMakeLists.txt" OR
       NOT EXISTS "${PF_SDL_SOURCE_DIR}/include/SDL3/SDL_version.h")
        message(FATAL_ERROR
            "PF_ENABLE_SDL_CLIENT requires the locked SDL3 source. "
            "Run tools/bootstrap.sh or tools/bootstrap.ps1.")
    endif()

    set(SDL_SHARED OFF CACHE BOOL "Do not build shared SDL in-tree" FORCE)
    set(SDL_STATIC ON CACHE BOOL "Build static SDL in-tree" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "Do not build SDL test support" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "Do not build SDL tests" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "Do not build SDL examples" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "Do not install in-tree SDL" FORCE)
    set(
        SDL_UNIX_CONSOLE_BUILD
        "${PF_SDL_UNIX_CONSOLE_BUILD}"
        CACHE BOOL
        "Allow SDL without a Unix desktop video backend"
        FORCE)
    add_subdirectory(
        "${PF_SDL_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}/third_party/sdl"
        EXCLUDE_FROM_ALL)

    if(NOT TARGET SDL3::SDL3-static)
        message(FATAL_ERROR "Locked SDL3 did not provide SDL3::SDL3-static")
    endif()
endif()

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

function(pf_add_smoke_product target product_name render_probe)
    add_executable(${target} src/product/product_main.c)
    target_link_libraries(${target} PRIVATE sim)
    if(render_probe)
        target_link_libraries(${target} PRIVATE presentation)
        set(pf_expected_product_links "sim;presentation")
    else()
        set(pf_expected_product_links "sim")
    endif()
    target_compile_features(${target} PRIVATE c_std_17)
    target_compile_definitions(
        ${target}
        PRIVATE
            "PF_PRODUCT_NAME=\"${product_name}\""
            "PF_PRODUCT_RENDER_PROBE=${render_probe}")
    set_target_properties(${target} PROPERTIES C_EXTENSIONS OFF)
    pf_enable_project_warnings(${target})
    pf_apply_build_mode(${target})

    get_target_property(pf_product_link_libraries ${target} LINK_LIBRARIES)
    if(NOT "${pf_product_link_libraries}" STREQUAL
       "${pf_expected_product_links}")
        message(FATAL_ERROR
            "${target} has unexpected explicit M1 boundary links; "
            "got: ${pf_product_link_libraries}")
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
                       "${product_name}-smoke=pass sim_abi=2 tick_hz=60")
    endif()
endfunction()

if(PF_BUILD_NATIVE_CLIENT)
    if(PF_ENABLE_SDL_CLIENT)
        add_executable(native_client src/native_client/main.c)
        target_link_libraries(
            native_client
            PRIVATE
                sim
                presentation
                SDL3::SDL3-static)
        target_compile_features(native_client PRIVATE c_std_17)
        set_target_properties(native_client PROPERTIES C_EXTENSIONS OFF)
        pf_enable_project_warnings(native_client)
        pf_apply_build_mode(native_client)

        if(BUILD_TESTING)
            add_test(NAME native_client.smoke COMMAND native_client --smoke)
            set_tests_properties(
                native_client.smoke
                PROPERTIES PASS_REGULAR_EXPRESSION
                           "native-client-smoke=pass sim_abi=2 tick_hz=60 sdl=3.4.12 event=pass gamepad=pass geometry=pass batch_draws=1")
        endif()
    else()
        pf_add_smoke_product(native_client "native-client" 0)
    endif()
    set_target_properties(
        native_client
        PROPERTIES OUTPUT_NAME pf_native_client)
endif()

if(PF_BUILD_WEB_CLIENT)
    pf_add_smoke_product(web_client "web-client" 1)
    if(NOT EMSCRIPTEN)
        set_target_properties(
            web_client
            PROPERTIES OUTPUT_NAME pf_web_client_host_smoke)
    endif()
endif()

if(PF_BUILD_TOOLS)
    pf_add_smoke_product(tools "tools" 0)
    set_target_properties(tools PROPERTIES OUTPUT_NAME pf_tools)
endif()

if(PF_BUILD_BENCHMARKS)
    pf_add_smoke_product(benchmarks "benchmarks" 0)
    set_target_properties(
        benchmarks
        PROPERTIES OUTPUT_NAME pf_benchmarks)
endif()

if(PF_BUILD_VERIFIER)
    pf_add_smoke_product(verifier "verifier" 0)
    set_target_properties(verifier PROPERTIES OUTPUT_NAME pf_verifier)
endif()

if(BUILD_TESTING)
    add_executable(
        render_packet_test
        tests/presentation/test_render_packet.c)
    target_link_libraries(render_packet_test PRIVATE presentation)
    target_compile_features(render_packet_test PRIVATE c_std_17)
    set_target_properties(render_packet_test PROPERTIES C_EXTENSIONS OFF)
    pf_enable_project_warnings(render_packet_test)
    pf_apply_build_mode(render_packet_test)

    add_test(NAME presentation.render_packet COMMAND render_packet_test)
    set_tests_properties(
        presentation.render_packet
        PROPERTIES PASS_REGULAR_EXPRESSION
                   "render-packet=pass vertices=12 texture=2x2")

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
                   "sim-contract=pass abi=2 tick_hz=60")

    add_executable(sim_world_test tests/sim/test_sim_world.c)
    target_link_libraries(sim_world_test PRIVATE sim)
    target_compile_features(sim_world_test PRIVATE c_std_17)
    set_target_properties(sim_world_test PROPERTIES C_EXTENSIONS OFF)
    pf_enable_project_warnings(sim_world_test)
    pf_apply_build_mode(sim_world_test)

    add_test(NAME sim.world COMMAND sim_world_test)
    set_tests_properties(
        sim.world
        PROPERTIES PASS_REGULAR_EXPRESSION
                   "sim-world=pass players=4 deterministic_ticks=180")

    add_executable(
        sim_snapshot_test
        tests/sim/test_sim_snapshot.c)
    target_link_libraries(sim_snapshot_test PRIVATE sim)
    target_include_directories(
        sim_snapshot_test
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/sim")
    target_compile_features(sim_snapshot_test PRIVATE c_std_17)
    set_target_properties(
        sim_snapshot_test
        PROPERTIES C_EXTENSIONS OFF)
    pf_enable_project_warnings(sim_snapshot_test)
    pf_apply_build_mode(sim_snapshot_test)

    add_test(NAME sim.snapshot COMMAND sim_snapshot_test)
    set_tests_properties(
        sim.snapshot
        PROPERTIES PASS_REGULAR_EXPRESSION
                   "sim-snapshot=pass bytes=305 hash_algorithm=sha256")

    if(PF_BUILD_HEADLESS)
        add_test(NAME headless.smoke COMMAND headless --smoke)
        set_tests_properties(
            headless.smoke
            PROPERTIES PASS_REGULAR_EXPRESSION
                       "headless-smoke=pass sim_abi=2 tick_hz=60")
    endif()
endif()
