#include "m2_replay_fixture.h"

#include <stdint.h>
#include <string.h>

pf_content_view pf_m2_replay_make_content(void)
{
    pf_content_view content;
    uint32_t byte_index;

    (void)memset(&content, 0, sizeof(content));
    content.struct_size = (uint32_t)sizeof(content);
    content.schema_version = PF_SIM_CONTENT_SCHEMA_VERSION;
    for (byte_index = UINT32_C(0);
         byte_index < (uint32_t)sizeof(content.content_hash.bytes);
         ++byte_index)
    {
        content.content_hash.bytes[byte_index] =
            (uint8_t)(byte_index * UINT32_C(13) + UINT32_C(7));
    }
    return content;
}

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick)
{
    uint32_t player_index;

    (void)memset(
        inputs,
        0,
        sizeof(*inputs) * (size_t)PF_M2_REPLAY_PLAYERS);
    for (player_index = UINT32_C(0);
         player_index < (uint32_t)PF_M2_REPLAY_PLAYERS;
         ++player_index)
    {
        inputs[player_index].tick = tick;
        inputs[player_index].schema_version =
            PF_SIM_INPUT_SCHEMA_VERSION;
        inputs[player_index].player_slot = (uint8_t)player_index;
    }

    inputs[0].main_stick_x =
        (tick % UINT64_C(17)) < UINT64_C(11)
            ? INT16_C(32767)
            : INT16_C(0);
    inputs[1].main_stick_x =
        (tick % UINT64_C(19)) < UINT64_C(13)
            ? INT16_MIN
            : INT16_C(0);
    inputs[2].main_stick_x =
        (tick % UINT64_C(23)) < UINT64_C(12)
            ? INT16_C(20480)
            : INT16_C(-16384);
    inputs[3].main_stick_x =
        (tick % UINT64_C(29)) < UINT64_C(15)
            ? INT16_C(-24576)
            : INT16_C(12288);

    if (tick == UINT64_C(8) || tick == UINT64_C(87))
    {
        inputs[0].buttons |= PF_INPUT_BUTTON_JUMP;
    }
    if (tick == UINT64_C(21) || tick == UINT64_C(103))
    {
        inputs[1].buttons |= PF_INPUT_BUTTON_JUMP;
    }
    if (tick == UINT64_C(34) || tick == UINT64_C(119))
    {
        inputs[2].buttons |= PF_INPUT_BUTTON_JUMP;
    }
    if (tick == UINT64_C(55) || tick == UINT64_C(141))
    {
        inputs[3].buttons |= PF_INPUT_BUTTON_JUMP;
    }
    if (tick + UINT64_C(1) == PF_M2_REPLAY_TICKS)
    {
        inputs[3].buttons |= PF_INPUT_BUTTON_FORFEIT;
    }
}
