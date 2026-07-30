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
option(
    PF_BUILD_RL_PYTHON_LIBRARY
    "Build the native shared library consumed by the Python RL wrapper"
    ON)
option(
    PF_BUILD_DETERMINISM_CORPUS
    "Build the native/WebAssembly replay determinism corpus"
    ON)
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
set(
    PF_SQLITE_SOURCE_DIR
    ""
    CACHE PATH
    "Path to the checksum-verified SQLite amalgamation")
set(
    PF_TRACY_SOURCE_DIR
    ""
    CACHE PATH
    "Path to the checksum-verified Tracy source tree")

if(PF_ENABLE_PROFILING)
    if(NOT EXISTS "${PF_TRACY_SOURCE_DIR}/public/tracy/TracyC.h" OR
       NOT EXISTS "${PF_TRACY_SOURCE_DIR}/public/TracyClient.cpp")
        message(FATAL_ERROR
            "PF_ENABLE_PROFILING requires the locked Tracy source. "
            "Run tools/bootstrap.sh or tools/bootstrap.ps1.")
    endif()
    enable_language(CXX)
    set(TRACY_STATIC ON CACHE BOOL "Build static Tracy client" FORCE)
    set(TRACY_ENABLE ON CACHE BOOL "Enable Tracy profiling" FORCE)
    set(TRACY_NO_BROADCAST ON CACHE BOOL "Disable Tracy LAN broadcast" FORCE)
    set(
        TRACY_NO_VSYNC_CAPTURE
        ON
        CACHE BOOL
        "Disable profile-only VSync capture"
        FORCE)
    add_subdirectory(
        "${PF_TRACY_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}/third_party/tracy"
        EXCLUDE_FROM_ALL)
endif()

set(
    PF_SIM_SOURCES
    src/sim/sim.c
    src/sim/sim_combat.c
    src/sim/sim_content.c
    src/sim/sim_internal.h
    src/sim/sim_movement.c
    src/sim/sim_sha256.c
    src/sim/sim_sha256.h
    src/sim/sim_rl.c
    src/sim/sim_replay.c
    src/sim/sim_snapshot.c
    src/sim/sim_tick.c)

function(pf_configure_sim_target target output_name)
    target_include_directories(
        ${target}
        PUBLIC
            "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
    target_compile_features(${target} PUBLIC c_std_17)
    set_target_properties(
        ${target}
        PROPERTIES
            C_EXTENSIONS OFF
            OUTPUT_NAME "${output_name}")
    pf_enable_project_warnings(${target})
    pf_apply_build_mode(${target})
    pf_assert_deterministic_target(${target})
endfunction()

add_library(sim STATIC ${PF_SIM_SOURCES})
add_library(pf::sim ALIAS sim)
pf_configure_sim_target(sim pf_sim)

if(PF_BUILD_RL_PYTHON_LIBRARY AND NOT EMSCRIPTEN)
    add_library(sim_rl_shared SHARED ${PF_SIM_SOURCES})
    add_library(pf::sim_rl_shared ALIAS sim_rl_shared)
    pf_configure_sim_target(sim_rl_shared pf_sim_rl)
    set_target_properties(
        sim_rl_shared
        PROPERTIES
            WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

if(PF_BUILD_DETERMINISM_CORPUS)
    add_executable(
        replay_corpus
        src/checkpoint/m2_replay_fixture.c
        tests/sim/test_replay_corpus.c)
    target_link_libraries(replay_corpus PRIVATE sim)
    target_include_directories(
        replay_corpus
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src/checkpoint"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/sim")
    target_compile_features(replay_corpus PRIVATE c_std_17)
    set_target_properties(
        replay_corpus
        PROPERTIES
            C_EXTENSIONS OFF
            OUTPUT_NAME pf_replay_corpus)
    pf_enable_project_warnings(replay_corpus)
    pf_apply_build_mode(replay_corpus)
    if(EMSCRIPTEN)
        target_link_options(
            replay_corpus
            PRIVATE "-sENVIRONMENT=node")
    endif()
endif()

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
        set(pf_web_shell
            "${CMAKE_CURRENT_SOURCE_DIR}/src/web_client/shell.html")
        target_compile_definitions(${target} PRIVATE PF_PRODUCT_BROWSER=1)
        target_link_options(
            ${target}
            PRIVATE
                "-sASSERTIONS=1"
                "-sEXPORTED_FUNCTIONS=_main,_pf_web_m4_playtest_step,_pf_web_m4_playtest_reset"
                "--js-library=${pf_web_adapter}"
                "--shell-file=${pf_web_shell}")
        set_property(
            TARGET ${target}
            APPEND
            PROPERTY LINK_DEPENDS
                "${pf_web_adapter}"
                "${pf_web_shell}")
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
    if(EMSCRIPTEN)
        target_sources(
            web_client
            PRIVATE
                src/checkpoint/m2_replay_fixture.c
                src/web_client/m4_playtest.c
                src/web_client/replay_checkpoint.c)
        target_include_directories(
            web_client
            PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/src/checkpoint"
                "${CMAKE_CURRENT_SOURCE_DIR}/src/web_client")
    else()
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
    if(NOT EXISTS "${PF_SQLITE_SOURCE_DIR}/sqlite3.c" OR
       NOT EXISTS "${PF_SQLITE_SOURCE_DIR}/sqlite3.h")
        message(FATAL_ERROR
            "PF_BUILD_BENCHMARKS requires the locked SQLite source. "
            "Run tools/bootstrap.sh or tools/bootstrap.ps1.")
    endif()

    add_library(
        pf_sqlite3
        STATIC
            "${PF_SQLITE_SOURCE_DIR}/sqlite3.c")
    target_include_directories(
        pf_sqlite3
        PUBLIC
            "${PF_SQLITE_SOURCE_DIR}")
    target_compile_definitions(
        pf_sqlite3
        PRIVATE
            SQLITE_DEFAULT_MEMSTATUS=0
            SQLITE_DQS=0
            SQLITE_OMIT_DEPRECATED
            SQLITE_OMIT_LOAD_EXTENSION
            SQLITE_OMIT_SHARED_CACHE
            SQLITE_TEMP_STORE=2
            SQLITE_THREADSAFE=0)
    set_target_properties(
        pf_sqlite3
        PROPERTIES
            C_EXTENSIONS ON
            C_STANDARD 17
            C_STANDARD_REQUIRED ON)
    pf_apply_build_mode(pf_sqlite3)

    add_executable(
        benchmarks
        src/benchmarks/benchmark.h
        src/benchmarks/history.c
        src/benchmarks/main.c
        src/benchmarks/scenarios.c
        src/checkpoint/m2_replay_fixture.c)
    target_link_libraries(
        benchmarks
        PRIVATE
            sim
            pf_sqlite3)
    if(PF_ENABLE_PROFILING)
        target_link_libraries(benchmarks PRIVATE Tracy::TracyClient)
        target_compile_definitions(benchmarks PRIVATE PF_TRACY_ENABLED=1)
    else()
        target_compile_definitions(benchmarks PRIVATE PF_TRACY_ENABLED=0)
    endif()
    target_include_directories(
        benchmarks
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src/benchmarks"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/checkpoint")
    target_compile_features(benchmarks PRIVATE c_std_17)
    target_compile_definitions(
        benchmarks
        PRIVATE
            "PF_BENCHMARK_SCHEMA_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}/performance/database/schema.sql\"")
    if(MSVC)
        target_compile_definitions(
            benchmarks
            PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_definitions(
            benchmarks
            PRIVATE _POSIX_C_SOURCE=200809L)
    endif()
    set_target_properties(
        benchmarks
        PROPERTIES
            C_EXTENSIONS OFF
            OUTPUT_NAME pf_benchmarks)
    pf_enable_project_warnings(benchmarks)
    pf_apply_build_mode(benchmarks)
endif()

if(PF_BUILD_VERIFIER)
    add_executable(verifier src/verifier/main.c)
    target_link_libraries(
        verifier
        PRIVATE
            sim
            presentation)
    if(MSVC)
        target_compile_definitions(
            verifier
            PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
    target_compile_features(verifier PRIVATE c_std_17)
    set_target_properties(
        verifier
        PROPERTIES
            C_EXTENSIONS OFF
            OUTPUT_NAME pf_verifier)
    pf_enable_project_warnings(verifier)
    pf_apply_build_mode(verifier)
endif()

if(BUILD_TESTING)
    add_test(
        NAME docs.m4_technique_registry
        COMMAND
            "${CMAKE_COMMAND}"
            "-DPF_M4_TECHNIQUE_REGISTRY=${CMAKE_CURRENT_SOURCE_DIR}/docs/product/m4_advanced_technique_registry.md"
            -P
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyM4TechniqueRegistry.cmake")
    set_tests_properties(
        docs.m4_technique_registry
        PROPERTIES PASS_REGULAR_EXPRESSION
                   "m4-technique-registry=pass rows=61")

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
                       "sim-snapshot=pass bytes=577 hash_algorithm=sha256")

    add_executable(m4_movement_test tests/sim/test_m4_movement.c)
    target_link_libraries(m4_movement_test PRIVATE sim)
    target_compile_features(m4_movement_test PRIVATE c_std_17)
    set_target_properties(m4_movement_test PROPERTIES C_EXTENSIONS OFF)
    pf_enable_project_warnings(m4_movement_test)
    pf_apply_build_mode(m4_movement_test)

    add_test(NAME sim.m4_movement COMMAND m4_movement_test)
    set_tests_properties(
        sim.m4_movement
        PROPERTIES PASS_REGULAR_EXPRESSION
                   "m4-movement=pass content_schema=13 deterministic_ticks=20000 movement_invariants=94")

    add_executable(m4_combat_test tests/sim/test_m4_combat.c)
    target_link_libraries(m4_combat_test PRIVATE sim)
    target_compile_features(m4_combat_test PRIVATE c_std_17)
    set_target_properties(m4_combat_test PROPERTIES C_EXTENSIONS OFF)
    pf_enable_project_warnings(m4_combat_test)
    pf_apply_build_mode(m4_combat_test)

    add_test(NAME sim.m4_combat COMMAND m4_combat_test)
    set_tests_properties(
        sim.m4_combat
        PROPERTIES PASS_REGULAR_EXPRESSION
                   "m4-combat=pass content_schema=13 deterministic_ticks=20000 combat_invariants=110")

    add_executable(
        m4_web_playtest_test
        src/web_client/m4_playtest.c
        tests/web/test_m4_playtest.c)
    target_link_libraries(m4_web_playtest_test PRIVATE sim)
    target_include_directories(
        m4_web_playtest_test
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/web_client")
    target_compile_features(m4_web_playtest_test PRIVATE c_std_17)
    set_target_properties(
        m4_web_playtest_test
        PROPERTIES C_EXTENSIONS OFF)
    pf_enable_project_warnings(m4_web_playtest_test)
    pf_apply_build_mode(m4_web_playtest_test)

    add_test(NAME web.m4_playtest COMMAND m4_web_playtest_test)
    set_tests_properties(
        web.m4_playtest
        PROPERTIES PASS_REGULAR_EXPRESSION
                   "m4-browser-adapter=pass walk_axis=13500 dash_axis=32767 input_probe=1 air_facing_probe=1 combat_probe=1 reaction_probe=1 shield_probe=1 powershield_cancel_probe=1 tumble_probe=1 floor_recovery_probe=1 surface_tech_probe=1 air_dodge_probe=1 ground_dodge_probe=1 aerial_l_cancel_probe=1")

    add_executable(rl_api_test tests/sim/test_rl_api.c)
    target_link_libraries(rl_api_test PRIVATE sim)
    target_compile_features(rl_api_test PRIVATE c_std_17)
    set_target_properties(rl_api_test PROPERTIES C_EXTENSIONS OFF)
    pf_enable_project_warnings(rl_api_test)
    pf_apply_build_mode(rl_api_test)

    add_test(NAME sim.rl_api COMMAND rl_api_test)
    set_tests_properties(
        sim.rl_api
        PROPERTIES PASS_REGULAR_EXPRESSION
                   "rl-api=pass compact_values=36 batch_environments=6 reward_q16=65536 engagement_limit_q16=16384 schema=2")

    if(PF_BUILD_DETERMINISM_CORPUS)
        add_test(NAME sim.replay_corpus COMMAND replay_corpus)
        set_tests_properties(
            sim.replay_corpus
            PROPERTIES PASS_REGULAR_EXPRESSION
                       "sim-replay=pass ticks=180 players=4 bytes=31269")
    endif()

    if(PF_BUILD_HEADLESS)
        add_test(NAME headless.smoke COMMAND headless --smoke)
        set_tests_properties(
            headless.smoke
            PROPERTIES PASS_REGULAR_EXPRESSION
                       "headless-smoke=pass sim_abi=2 tick_hz=60")

        add_test(
            NAME headless.throughput_smoke
            COMMAND headless --throughput-smoke)
        set_tests_properties(
            headless.throughput_smoke
            PROPERTIES PASS_REGULAR_EXPRESSION
                       "headless-throughput=pass environments=64")
    endif()

    if(PF_BUILD_BENCHMARKS)
        add_test(
            NAME benchmarks.self_test
            COMMAND benchmarks --self-test)
        set_tests_properties(
            benchmarks.self_test
            PROPERTIES PASS_REGULAR_EXPRESSION
                       "benchmarks-self-test=pass scenarios=13 available=9 sqlite=3.53.4 schema=1")
    endif()

    if(PF_BUILD_VERIFIER)
        add_test(
            NAME verifier.self_test
            COMMAND verifier --self-test)
        set_tests_properties(
            verifier.self_test
            PROPERTIES PASS_REGULAR_EXPRESSION
                       "verifier-self-test=pass checks=3 seeded_defects=4")
    endif()
endif()
