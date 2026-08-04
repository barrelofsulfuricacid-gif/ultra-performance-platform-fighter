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
    inputs[0].main_stick_y =
        (tick % UINT64_C(16)) >= UINT64_C(8) &&
                (tick % UINT64_C(16)) < UINT64_C(12)
            ? INT16_MIN
            : INT16_C(0);
    inputs[1].main_stick_y =
        (tick % UINT64_C(18)) >= UINT64_C(9) &&
                (tick % UINT64_C(18)) < UINT64_C(13)
            ? INT16_MIN
            : INT16_C(0);
    inputs[2].main_stick_y =
        (tick % UINT64_C(20)) >= UINT64_C(10) &&
                (tick % UINT64_C(20)) < UINT64_C(14)
            ? INT16_MIN
            : INT16_C(0);
    inputs[3].main_stick_y =
        (tick % UINT64_C(22)) >= UINT64_C(11) &&
                (tick % UINT64_C(22)) < UINT64_C(15)
            ? INT16_MIN
            : INT16_C(0);

    if (tick % UINT64_C(41) == UINT64_C(7))
    {
        inputs[0].left_trigger = UINT16_MAX;
    }
    if (tick % UINT64_C(43) == UINT64_C(11))
    {
        inputs[1].left_trigger = UINT16_MAX;
    }
    if (tick % UINT64_C(47) == UINT64_C(13))
    {
        inputs[2].left_trigger = UINT16_MAX;
    }
    if (tick % UINT64_C(53) == UINT64_C(17))
    {
        inputs[3].left_trigger = UINT16_MAX;
    }

    if (tick < UINT64_C(27))
    {
        inputs[0].main_stick_x = INT16_C(32767);
        inputs[1].main_stick_x = INT16_C(-32767);
        inputs[2].main_stick_x = INT16_C(32767);
        inputs[3].main_stick_x = INT16_C(-32767);
    }
    if (tick == UINT64_C(0))
    {
        inputs[0].left_trigger = UINT16_MAX;
        inputs[1].main_stick_y = INT16_MAX;
        inputs[1].left_trigger = UINT16_MAX;
    }

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
    if (tick == UINT64_C(144))
    {
        inputs[3].main_stick_x = INT16_C(0);
        inputs[3].main_stick_y = INT16_C(8192);
        inputs[3].left_trigger = UINT16_MAX;
    }
    if (tick == UINT64_C(155))
    {
        inputs[3].buttons |= PF_INPUT_BUTTON_JUMP;
    }
    if (tick == UINT64_C(158))
    {
        inputs[3].main_stick_x = INT16_C(0);
        inputs[3].main_stick_y = -INT16_C(8192);
        inputs[3].left_trigger = UINT16_MAX;
    }
    if (tick % UINT64_C(13) == UINT64_C(4))
    {
        inputs[0].buttons |= PF_INPUT_BUTTON_ATTACK;
    }
    if (tick % UINT64_C(17) == UINT64_C(6))
    {
        inputs[1].buttons |= PF_INPUT_BUTTON_ATTACK;
    }
    if (tick % UINT64_C(19) == UINT64_C(8))
    {
        inputs[2].buttons |= PF_INPUT_BUTTON_ATTACK;
    }
    if (tick % UINT64_C(23) == UINT64_C(10))
    {
        inputs[3].buttons |= PF_INPUT_BUTTON_ATTACK;
    }
    if (tick == UINT64_C(27))
    {
        inputs[0].buttons |= PF_INPUT_BUTTON_ATTACK;
        inputs[2].buttons |= PF_INPUT_BUTTON_ATTACK;
    }
    if (tick >= UINT64_C(20) && tick <= UINT64_C(50))
    {
        inputs[2].main_stick_x = INT16_C(0);
        inputs[2].main_stick_y = INT16_C(0);
        inputs[2].buttons = UINT64_C(0);
        inputs[2].left_trigger = UINT16_C(0);
        inputs[3].main_stick_x =
            tick < UINT64_C(35) ? INT16_MIN : INT16_C(0);
        inputs[3].main_stick_y = INT16_C(0);
        inputs[3].buttons =
            tick == UINT64_C(34)
                ? PF_INPUT_BUTTON_ATTACK
                : UINT64_C(0);
        inputs[3].left_trigger = UINT16_C(0);
    }
    if (tick == UINT64_C(39))
    {
        inputs[2].main_stick_x = INT16_MAX;
    }
    if (tick == UINT64_C(46))
    {
        inputs[2].buttons = PF_INPUT_BUTTON_ATTACK;
    }
    if (tick == UINT64_C(50))
    {
        inputs[3].main_stick_x = INT16_MIN;
    }
    if (tick == UINT64_C(72))
    {
        inputs[1].main_stick_x = INT16_MAX;
    }
    if (tick <= UINT64_C(5))
    {
        inputs[2].main_stick_x = INT16_C(0);
        inputs[2].main_stick_y = INT16_C(0);
        inputs[2].buttons =
            tick == UINT64_C(0)
                ? PF_INPUT_BUTTON_JUMP
                : UINT64_C(0);
        inputs[2].left_trigger =
            tick == UINT64_C(5) ? UINT16_MAX : UINT16_C(0);
    }
    if (tick + UINT64_C(1) == PF_M2_REPLAY_TICKS)
    {
        inputs[3].buttons |= PF_INPUT_BUTTON_FORFEIT;
    }
}
