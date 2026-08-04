#include "pf/m4.h"
#include "pf/sim.h"

#include <inttypes.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define PF_TRACE_MEMORY_BYTES 4096U
#define PF_TRACE_MEMORY_ALIGNMENT 64U

typedef struct pf_trace_storage
{
    alignas(PF_TRACE_MEMORY_ALIGNMENT) uint8_t state[PF_TRACE_MEMORY_BYTES];
    alignas(PF_TRACE_MEMORY_ALIGNMENT) uint8_t scratch[PF_TRACE_MEMORY_BYTES];
} pf_trace_storage;

static int fail_status(const char *operation, pf_status status)
{
    (void)fprintf(
        stderr,
        "m4-movement-trace=fail operation=%s status=%s\n",
        operation,
        pf_status_name(status));
    return 1;
}

int main(void)
{
    pf_trace_storage storage;
    pf_m4_content content;
    pf_content_view view;
    pf_sim_config config;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    int32_t origin_x_q16;
    uint32_t trace_frame = UINT32_C(0);
    int input_x;
    pf_status status;

    (void)memset(&storage, 0, sizeof(storage));
    status = pf_m4_default_content(&content);
    if (status != PF_STATUS_OK)
    {
        return fail_status("default-content", status);
    }
    status = pf_m4_make_content_view(&content, &view);
    if (status != PF_STATUS_OK)
    {
        return fail_status("content-view", status);
    }
    status = pf_sim_default_config(
        &config,
        UINT8_C(2),
        PF_SIM_MODE_DUEL);
    if (status != PF_STATUS_OK)
    {
        return fail_status("default-config", status);
    }
    config.max_ticks = UINT64_C(100000);
    config.stock_count = UINT8_C(0);
    status = pf_sim_init(
        storage.state,
        sizeof(storage.state),
        storage.scratch,
        sizeof(storage.scratch),
        &view,
        &config,
        &sim);
    if (status != PF_STATUS_OK)
    {
        return fail_status("init", status);
    }
    status = pf_sim_reset(sim, UINT64_C(2));
    if (status != PF_STATUS_OK)
    {
        return fail_status("reset", status);
    }
    status = pf_m4_inspect(sim, &inspection);
    if (status != PF_STATUS_OK)
    {
        return fail_status("inspect-origin", status);
    }
    origin_x_q16 = inspection.players[0].position_x_q16;

    (void)puts(
        "trace_frame,input_x,tick,action_state,action_ticks,facing,grounded,"
        "dash_direction,previous_strong_direction,position_x_q16_from_origin,"
        "velocity_x_q16");
    while (scanf("%d", &input_x) == 1)
    {
        pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
        pf_tick_result result;

        if (input_x < (int)INT16_MIN || input_x > (int)INT16_MAX)
        {
            (void)fprintf(
                stderr,
                "m4-movement-trace=fail operation=input-range frame=%" PRIu32
                " value=%d\n",
                trace_frame,
                input_x);
            return 1;
        }
        (void)memset(inputs, 0, sizeof(inputs));
        inputs[0].tick = inspection.tick;
        inputs[0].schema_version = PF_SIM_INPUT_SCHEMA_VERSION;
        inputs[0].player_slot = UINT8_C(0);
        inputs[0].main_stick_x = (int16_t)input_x;
        inputs[1].tick = inspection.tick;
        inputs[1].schema_version = PF_SIM_INPUT_SCHEMA_VERSION;
        inputs[1].player_slot = UINT8_C(1);
        status = pf_sim_tick(sim, inputs, (size_t)2, &result);
        if (status != PF_STATUS_OK)
        {
            return fail_status("tick", status);
        }
        status = pf_m4_inspect(sim, &inspection);
        if (status != PF_STATUS_OK)
        {
            return fail_status("inspect", status);
        }
        (void)printf(
            "%" PRIu32 ",%d,%" PRIu64 ",%u,%u,%d,%u,%d,%d,%" PRId32
            ",%" PRId32 "\n",
            trace_frame,
            input_x,
            inspection.tick,
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (int)inspection.players[0].facing,
            (unsigned int)inspection.players[0].grounded,
            (int)inspection.players[0].dash_direction,
            (int)inspection.players[0].previous_strong_direction,
            inspection.players[0].position_x_q16 - origin_x_q16,
            inspection.players[0].velocity_x_q16);
        ++trace_frame;
    }
    if (ferror(stdin) != 0)
    {
        (void)fprintf(
            stderr,
            "m4-movement-trace=fail operation=read-input\n");
        return 1;
    }
    return 0;
}
