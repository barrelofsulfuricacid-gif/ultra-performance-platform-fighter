#include "native_playtest.h"

#include "pf/m4.h"
#include "pf/sim.h"

#include <SDL3/SDL.h>

#include <inttypes.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define PF_NATIVE_DEFAULT_PLAYER_COUNT UINT8_C(2)
#define PF_NATIVE_SEED UINT64_C(0x424154544c454649)
#define PF_NATIVE_MAX_DEVICES 8
#define PF_NATIVE_TICK_NS UINT64_C(16666667)
#define PF_NATIVE_MAYFLASH_VENDOR UINT16_C(0x0079)
#define PF_NATIVE_MAYFLASH_PRODUCT UINT16_C(0x1843)
#define PF_NATIVE_STICK_DEADZONE INT32_C(6553)
#define PF_NATIVE_STICK_RANGE INT32_C(24575)
#define PF_NATIVE_KEYBOARD_AXIS INT16_C(32767)

typedef enum pf_native_device_kind
{
    PF_NATIVE_DEVICE_GAMEPAD = 1,
    PF_NATIVE_DEVICE_MAYFLASH_PC = 2
} pf_native_device_kind;

typedef struct pf_native_device
{
    SDL_JoystickID id;
    SDL_Gamepad *gamepad;
    SDL_Joystick *joystick;
    pf_native_device_kind kind;
} pf_native_device;

typedef struct pf_native_session
{
    struct content content;
    pf_content_view content_view;
    pf_sim_config config;
    pf_memory_requirements memory;
    pf_sim *sim;
    void *state_memory;
    void *scratch_memory;
    pf_native_device devices[PF_NATIVE_MAX_DEVICES];
    SDL_JoystickID player_device_ids[PF_SIM_MAX_PLAYERS];
    uint8_t player_count;
    uint8_t mode;
    uint8_t stock_count;
    int device_count;
    int paused;
    int step_requested;
    int collision_inspector;
} pf_native_session;

static int pf_native_result_format_smoke(void);

typedef struct pf_native_view
{
    float x;
    float y;
    float scale;
} pf_native_view;

static int pf_native_mayflash_has_activity(SDL_Joystick *joystick);

static int pf_native_fail(const char *stage)
{
    (void)fprintf(
        stderr,
        "native-playtest=fail stage=%s error=%s\n",
        stage,
        SDL_GetError());
    return 1;
}

static int16_t pf_native_normalize_axis(Sint16 raw)
{
    int32_t value = (int32_t)raw;
    int32_t magnitude = value < INT32_C(0) ? -value : value;

    if (magnitude <= PF_NATIVE_STICK_DEADZONE)
    {
        return INT16_C(0);
    }
    magnitude = magnitude * INT16_MAX / PF_NATIVE_STICK_RANGE;
    if (magnitude > INT16_MAX)
    {
        magnitude = INT16_MAX;
    }
    return (int16_t)(value < INT32_C(0) ? -magnitude : magnitude);
}

static uint16_t pf_native_normalize_mayflash_trigger(Sint16 raw)
{
    const int32_t value = (int32_t)raw + INT32_C(32768);
    const int32_t deadzone = INT32_C(9830);

    if (value <= deadzone)
    {
        return UINT16_C(0);
    }
    return (uint16_t)(
        (value - deadzone) * (int32_t)UINT16_MAX /
        (INT32_C(65535) - deadzone));
}

static uint16_t pf_native_normalize_gamepad_trigger(Sint16 raw)
{
    if (raw <= INT16_C(0))
    {
        return UINT16_C(0);
    }
    return (uint16_t)((uint32_t)(uint16_t)raw * UINT32_C(2));
}

static void pf_native_apply_gamepad_button(
    SDL_GamepadType type,
    SDL_GamepadButton button,
    pf_input_frame *input)
{
    const int gamecube = type == SDL_GAMEPAD_TYPE_GAMECUBE;

    switch (button)
    {
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
        input->main_stick_x = INT16_MIN;
        break;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
        input->main_stick_x = INT16_MAX;
        break;
    case SDL_GAMEPAD_BUTTON_DPAD_UP:
        input->main_stick_y = INT16_MIN;
        break;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
        input->main_stick_y = INT16_MAX;
        break;
    case SDL_GAMEPAD_BUTTON_SOUTH:
        input->buttons |= PF_INPUT_BUTTON_ATTACK;
        break;
    case SDL_GAMEPAD_BUTTON_EAST:
        input->buttons |= gamecube != 0
                              ? PF_INPUT_BUTTON_JUMP
                              : PF_INPUT_BUTTON_STRONG_ATTACK;
        break;
    case SDL_GAMEPAD_BUTTON_WEST:
        input->buttons |= gamecube != 0
                              ? PF_INPUT_BUTTON_SPECIAL
                              : PF_INPUT_BUTTON_JUMP;
        break;
    case SDL_GAMEPAD_BUTTON_NORTH:
        input->buttons |= gamecube != 0
                              ? PF_INPUT_BUTTON_JUMP
                              : PF_INPUT_BUTTON_SPECIAL;
        break;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
        if (gamecube == 0)
        {
            input->left_trigger = UINT16_MAX;
        }
        break;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
        if (gamecube != 0)
        {
            input->buttons |= PF_INPUT_BUTTON_ATTACK;
            input->left_trigger = UINT16_MAX;
        }
        else
        {
            input->right_trigger = UINT16_MAX;
        }
        break;
    case SDL_GAMEPAD_BUTTON_MISC3:
        if (gamecube != 0)
        {
            input->left_trigger = UINT16_MAX;
        }
        break;
    case SDL_GAMEPAD_BUTTON_MISC4:
        if (gamecube != 0)
        {
            input->right_trigger = UINT16_MAX;
        }
        break;
    case SDL_GAMEPAD_BUTTON_BACK:
        if (gamecube == 0)
        {
            input->buttons |= PF_INPUT_BUTTON_TAUNT;
        }
        break;
    case SDL_GAMEPAD_BUTTON_START:
        if (gamecube != 0)
        {
            input->buttons |= PF_INPUT_BUTTON_TAUNT;
        }
        break;
    default:
        break;
    }
}

int native_playtest_smoke(void)
{
    static const struct pf_native_mapping_case
    {
        SDL_GamepadType type;
        SDL_GamepadButton button;
        uint64_t buttons;
        uint16_t left_trigger;
        uint16_t right_trigger;
    } cases[] = {
        {SDL_GAMEPAD_TYPE_GAMECUBE,
         SDL_GAMEPAD_BUTTON_SOUTH,
         PF_INPUT_BUTTON_ATTACK,
         0u,
         0u},
        {SDL_GAMEPAD_TYPE_GAMECUBE,
         SDL_GAMEPAD_BUTTON_WEST,
         PF_INPUT_BUTTON_SPECIAL,
         0u,
         0u},
        {SDL_GAMEPAD_TYPE_GAMECUBE,
         SDL_GAMEPAD_BUTTON_EAST,
         PF_INPUT_BUTTON_JUMP,
         0u,
         0u},
        {SDL_GAMEPAD_TYPE_GAMECUBE,
         SDL_GAMEPAD_BUTTON_NORTH,
         PF_INPUT_BUTTON_JUMP,
         0u,
         0u},
        {SDL_GAMEPAD_TYPE_GAMECUBE,
         SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
         PF_INPUT_BUTTON_ATTACK,
         UINT16_MAX,
         0u},
        {SDL_GAMEPAD_TYPE_GAMECUBE,
         SDL_GAMEPAD_BUTTON_MISC3,
         0u,
         UINT16_MAX,
         0u},
        {SDL_GAMEPAD_TYPE_GAMECUBE,
         SDL_GAMEPAD_BUTTON_MISC4,
         0u,
         0u,
         UINT16_MAX},
        {SDL_GAMEPAD_TYPE_GAMECUBE,
         SDL_GAMEPAD_BUTTON_START,
         PF_INPUT_BUTTON_TAUNT,
         0u,
         0u},
        {SDL_GAMEPAD_TYPE_GAMECUBE,
         SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,
         0u,
         0u,
         0u},
        {SDL_GAMEPAD_TYPE_STANDARD,
         SDL_GAMEPAD_BUTTON_SOUTH,
         PF_INPUT_BUTTON_ATTACK,
         0u,
         0u},
        {SDL_GAMEPAD_TYPE_STANDARD,
         SDL_GAMEPAD_BUTTON_EAST,
         PF_INPUT_BUTTON_STRONG_ATTACK,
         0u,
         0u},
        {SDL_GAMEPAD_TYPE_STANDARD,
         SDL_GAMEPAD_BUTTON_WEST,
         PF_INPUT_BUTTON_JUMP,
         0u,
         0u},
        {SDL_GAMEPAD_TYPE_STANDARD,
         SDL_GAMEPAD_BUTTON_NORTH,
         PF_INPUT_BUTTON_SPECIAL,
         0u,
         0u},
        {SDL_GAMEPAD_TYPE_STANDARD,
         SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,
         0u,
         UINT16_MAX,
         0u},
        {SDL_GAMEPAD_TYPE_STANDARD,
         SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
         0u,
         0u,
         UINT16_MAX},
        {SDL_GAMEPAD_TYPE_STANDARD,
         SDL_GAMEPAD_BUTTON_BACK,
         PF_INPUT_BUTTON_TAUNT,
         0u,
         0u},
        {SDL_GAMEPAD_TYPE_STANDARD,
         SDL_GAMEPAD_BUTTON_START,
         0u,
         0u,
         0u}};
    static const struct pf_native_dpad_case
    {
        SDL_GamepadButton button;
        int16_t main_stick_x;
        int16_t main_stick_y;
    } dpad_cases[] = {
        {SDL_GAMEPAD_BUTTON_DPAD_LEFT, INT16_MIN, INT16_C(0)},
        {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, INT16_MAX, INT16_C(0)},
        {SDL_GAMEPAD_BUTTON_DPAD_UP, INT16_C(0), INT16_MIN},
        {SDL_GAMEPAD_BUTTON_DPAD_DOWN, INT16_C(0), INT16_MAX}};
    pf_input_frame input;
    size_t index;

    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); ++index)
    {
        (void)memset(&input, 0, sizeof(input));
        pf_native_apply_gamepad_button(
            cases[index].type,
            cases[index].button,
            &input);
        if (input.buttons != cases[index].buttons ||
            input.left_trigger != cases[index].left_trigger ||
            input.right_trigger != cases[index].right_trigger)
        {
            return 0;
        }
    }
    for (index = 0u;
         index < sizeof(dpad_cases) / sizeof(dpad_cases[0]);
         ++index)
    {
        (void)memset(&input, 0, sizeof(input));
        pf_native_apply_gamepad_button(
            SDL_GAMEPAD_TYPE_STANDARD,
            dpad_cases[index].button,
            &input);
        if (input.main_stick_x != dpad_cases[index].main_stick_x ||
            input.main_stick_y != dpad_cases[index].main_stick_y ||
            input.buttons != UINT64_C(0) ||
            input.left_trigger != UINT16_C(0) ||
            input.right_trigger != UINT16_C(0))
        {
            return 0;
        }
    }
    (void)memset(&input, 0, sizeof(input));
    input.left_trigger = UINT16_C(1234);
    input.right_trigger = UINT16_C(5678);
    pf_native_apply_gamepad_button(
        SDL_GAMEPAD_TYPE_GAMECUBE,
        SDL_GAMEPAD_BUTTON_SOUTH,
        &input);
    return input.buttons == PF_INPUT_BUTTON_ATTACK &&
           input.left_trigger == UINT16_C(1234) &&
           input.right_trigger == UINT16_C(5678) &&
           pf_native_result_format_smoke() != 0;
}

static void pf_native_close_devices(pf_native_session *session)
{
    int index;

    for (index = 0; index < session->device_count; ++index)
    {
        SDL_CloseGamepad(session->devices[index].gamepad);
        SDL_CloseJoystick(session->devices[index].joystick);
    }
    (void)memset(session->devices, 0, sizeof(session->devices));
    (void)memset(
        session->player_device_ids,
        0,
        sizeof(session->player_device_ids));
    session->device_count = 0;
}

static void pf_native_refresh_devices(pf_native_session *session)
{
    SDL_JoystickID *ids;
    int count = 0;
    int index;

    pf_native_close_devices(session);
    ids = SDL_GetJoysticks(&count);
    if (ids == NULL)
    {
        return;
    }

    for (index = 0;
         index < count && session->device_count < PF_NATIVE_MAX_DEVICES;
         ++index)
    {
        pf_native_device *device =
            &session->devices[session->device_count];
        const Uint16 vendor = SDL_GetJoystickVendorForID(ids[index]);
        const Uint16 product = SDL_GetJoystickProductForID(ids[index]);

        device->id = ids[index];
        if (vendor == PF_NATIVE_MAYFLASH_VENDOR &&
            product == PF_NATIVE_MAYFLASH_PRODUCT)
        {
            device->joystick = SDL_OpenJoystick(ids[index]);
            if (device->joystick != NULL)
            {
                device->kind = PF_NATIVE_DEVICE_MAYFLASH_PC;
                ++session->device_count;
            }
        }
        else if (SDL_IsGamepad(ids[index]))
        {
            device->gamepad = SDL_OpenGamepad(ids[index]);
            if (device->gamepad != NULL)
            {
                device->kind = PF_NATIVE_DEVICE_GAMEPAD;
                ++session->device_count;
            }
        }
    }
    SDL_free(ids);
}

static int pf_native_active_device_count(const pf_native_session *session)
{
    int active = 0;
    int index;

    for (index = 0; index < (int)session->player_count; ++index)
    {
        if (session->player_device_ids[index] != UINT32_C(0))
        {
            ++active;
        }
    }
    return active;
}

static int pf_native_mayflash_has_activity(SDL_Joystick *joystick)
{
    int axis_index;
    int button_index;

    for (button_index = 0; button_index < 16; ++button_index)
    {
        if (SDL_GetJoystickButton(joystick, button_index))
        {
            return 1;
        }
    }
    if (SDL_GetJoystickHat(joystick, 0) != SDL_HAT_CENTERED)
    {
        return 1;
    }
    for (axis_index = 0; axis_index < 4; ++axis_index)
    {
        const int32_t raw =
            (int32_t)SDL_GetJoystickAxis(joystick, axis_index);
        if (raw < -PF_NATIVE_STICK_DEADZONE ||
            raw > PF_NATIVE_STICK_DEADZONE)
        {
            return 1;
        }
    }
    return SDL_GetJoystickAxis(joystick, 4) > INT16_C(-30000) ||
           SDL_GetJoystickAxis(joystick, 5) > INT16_C(-30000);
}

static void pf_native_read_mayflash(
    SDL_Joystick *joystick,
    pf_input_frame *input)
{
    const Uint8 hat = SDL_GetJoystickHat(joystick, 0);
    int16_t main_x = pf_native_normalize_axis(
        SDL_GetJoystickAxis(joystick, 0));
    int16_t main_y = pf_native_normalize_axis(
        SDL_GetJoystickAxis(joystick, 1));

    input->secondary_stick_x = pf_native_normalize_axis(
        SDL_GetJoystickAxis(joystick, 2));
    input->secondary_stick_y = pf_native_normalize_axis(
        SDL_GetJoystickAxis(joystick, 3));
    input->left_trigger = pf_native_normalize_mayflash_trigger(
        SDL_GetJoystickAxis(joystick, 4));
    input->right_trigger = pf_native_normalize_mayflash_trigger(
        SDL_GetJoystickAxis(joystick, 5));

    if (SDL_GetJoystickButton(joystick, 12) ||
        (hat & SDL_HAT_UP) != UINT8_C(0))
    {
        main_y = INT16_MIN;
    }
    if (SDL_GetJoystickButton(joystick, 13) ||
        (hat & SDL_HAT_RIGHT) != UINT8_C(0))
    {
        main_x = INT16_MAX;
    }
    if (SDL_GetJoystickButton(joystick, 14) ||
        (hat & SDL_HAT_DOWN) != UINT8_C(0))
    {
        main_y = INT16_MAX;
    }
    if (SDL_GetJoystickButton(joystick, 15) ||
        (hat & SDL_HAT_LEFT) != UINT8_C(0))
    {
        main_x = INT16_MIN;
    }
    input->main_stick_x = main_x;
    input->main_stick_y = main_y;

    if (SDL_GetJoystickButton(joystick, 1))
    {
        input->buttons |= PF_INPUT_BUTTON_ATTACK;
    }
    if (SDL_GetJoystickButton(joystick, 2))
    {
        input->buttons |= PF_INPUT_BUTTON_SPECIAL;
    }
    if (SDL_GetJoystickButton(joystick, 0) ||
        SDL_GetJoystickButton(joystick, 3))
    {
        input->buttons |= PF_INPUT_BUTTON_JUMP;
    }
    if (SDL_GetJoystickButton(joystick, 4))
    {
        input->left_trigger = UINT16_MAX;
    }
    if (SDL_GetJoystickButton(joystick, 5))
    {
        input->right_trigger = UINT16_MAX;
    }
    if (SDL_GetJoystickButton(joystick, 7))
    {
        input->buttons |= PF_INPUT_BUTTON_ATTACK;
        input->left_trigger = UINT16_MAX;
    }
    if (SDL_GetJoystickButton(joystick, 9))
    {
        input->buttons |= PF_INPUT_BUTTON_TAUNT;
    }
}

static void pf_native_read_gamepad(
    SDL_Gamepad *gamepad,
    pf_input_frame *input)
{
    const SDL_GamepadType type = SDL_GetGamepadType(gamepad);
    int button;

    input->main_stick_x = pf_native_normalize_axis(
        SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTX));
    input->main_stick_y = pf_native_normalize_axis(
        SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTY));
    input->secondary_stick_x = pf_native_normalize_axis(
        SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTX));
    input->secondary_stick_y = pf_native_normalize_axis(
        SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTY));
    input->left_trigger = pf_native_normalize_gamepad_trigger(
        SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER));
    input->right_trigger = pf_native_normalize_gamepad_trigger(
        SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER));
    for (button = 0; button < (int)SDL_GAMEPAD_BUTTON_COUNT; ++button)
    {
        if (SDL_GetGamepadButton(gamepad, (SDL_GamepadButton)button))
        {
            pf_native_apply_gamepad_button(
                type,
                (SDL_GamepadButton)button,
                input);
        }
    }
}

static void pf_native_overlay_keyboard(
    const bool *keys,
    int player,
    pf_input_frame *input)
{
    const SDL_Scancode left =
        player == 0 ? SDL_SCANCODE_A : SDL_SCANCODE_LEFT;
    const SDL_Scancode right =
        player == 0 ? SDL_SCANCODE_D : SDL_SCANCODE_RIGHT;
    const SDL_Scancode up =
        player == 0 ? SDL_SCANCODE_W : SDL_SCANCODE_UP;
    const SDL_Scancode down =
        player == 0 ? SDL_SCANCODE_S : SDL_SCANCODE_DOWN;

    if (keys[left])
    {
        input->main_stick_x = -PF_NATIVE_KEYBOARD_AXIS;
    }
    if (keys[right])
    {
        input->main_stick_x = PF_NATIVE_KEYBOARD_AXIS;
    }
    if (keys[up])
    {
        input->main_stick_y = -PF_NATIVE_KEYBOARD_AXIS;
    }
    if (keys[down])
    {
        input->main_stick_y = PF_NATIVE_KEYBOARD_AXIS;
    }

    if (player == 0)
    {
        if (keys[SDL_SCANCODE_SPACE])
        {
            input->buttons |= PF_INPUT_BUTTON_JUMP;
        }
        if (keys[SDL_SCANCODE_J])
        {
            input->buttons |= PF_INPUT_BUTTON_ATTACK;
        }
        if (keys[SDL_SCANCODE_K])
        {
            input->buttons |= PF_INPUT_BUTTON_STRONG_ATTACK;
        }
        if (keys[SDL_SCANCODE_I])
        {
            input->buttons |= PF_INPUT_BUTTON_SPECIAL;
        }
        if (keys[SDL_SCANCODE_L])
        {
            input->left_trigger = UINT16_MAX;
        }
    }
    else
    {
        if (keys[SDL_SCANCODE_RCTRL])
        {
            input->buttons |= PF_INPUT_BUTTON_JUMP;
        }
        if (keys[SDL_SCANCODE_KP_1])
        {
            input->buttons |= PF_INPUT_BUTTON_ATTACK;
        }
        if (keys[SDL_SCANCODE_KP_2])
        {
            input->buttons |= PF_INPUT_BUTTON_STRONG_ATTACK;
        }
        if (keys[SDL_SCANCODE_KP_3])
        {
            input->buttons |= PF_INPUT_BUTTON_SPECIAL;
        }
        if (keys[SDL_SCANCODE_RSHIFT])
        {
            input->left_trigger = UINT16_MAX;
        }
    }
}

static void pf_native_collect_inputs(
    pf_native_session *session,
    uint64_t tick,
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS])
{
    const bool *keys = SDL_GetKeyboardState(NULL);
    int device_index;
    int player;

    (void)memset(inputs, 0, sizeof(*inputs) * PF_SIM_MAX_PLAYERS);
    for (player = 0; player < (int)session->player_count; ++player)
    {
        inputs[player].tick = tick;
        inputs[player].schema_version = PF_SIM_INPUT_SCHEMA_VERSION;
        inputs[player].player_slot = (uint8_t)player;
    }

    for (device_index = 0;
         device_index < session->device_count;
         ++device_index)
    {
        pf_native_device *device = &session->devices[device_index];
        int assigned_player = -1;

        for (player = 0; player < (int)session->player_count; ++player)
        {
            if (session->player_device_ids[player] == device->id)
            {
                assigned_player = player;
                break;
            }
        }
        if (assigned_player < 0 &&
            (device->kind == PF_NATIVE_DEVICE_GAMEPAD ||
             pf_native_mayflash_has_activity(device->joystick)))
        {
            for (player = 0; player < (int)session->player_count; ++player)
            {
                if (session->player_device_ids[player] == UINT32_C(0))
                {
                    session->player_device_ids[player] = device->id;
                    assigned_player = player;
                    break;
                }
            }
        }
        if (assigned_player < 0)
        {
            continue;
        }
        if (device->kind == PF_NATIVE_DEVICE_MAYFLASH_PC)
        {
            pf_native_read_mayflash(
                device->joystick,
                &inputs[assigned_player]);
        }
        else
        {
            pf_native_read_gamepad(
                device->gamepad,
                &inputs[assigned_player]);
        }
    }

    for (player = 0;
         player < (int)session->player_count && player < 2;
         ++player)
    {
        pf_native_overlay_keyboard(keys, player, &inputs[player]);
    }
}

static int pf_native_reset(pf_native_session *session)
{
    const pf_status status = pf_sim_reset(session->sim, PF_NATIVE_SEED);

    if (status != PF_STATUS_OK)
    {
        (void)fprintf(
            stderr,
            "native-playtest=fail stage=reset status=%s\n",
            pf_status_name(status));
        return 0;
    }
    return 1;
}

static int pf_native_reinitialize_sim(pf_native_session *session)
{
    pf_status status;

    if (session->sim != NULL)
    {
        (void)pf_sim_deinit(session->sim);
        session->sim = NULL;
    }
    SDL_aligned_free(session->scratch_memory);
    SDL_aligned_free(session->state_memory);
    session->scratch_memory = NULL;
    session->state_memory = NULL;
    status = pf_sim_default_config(
        &session->config,
        session->player_count,
        (pf_sim_mode)session->mode);
    if (status != PF_STATUS_OK)
    {
        (void)fprintf(
            stderr,
            "native-playtest=fail stage=default-config status=%s\n",
            pf_status_name(status));
        return 0;
    }
    session->config.max_ticks = UINT64_C(216000);
    session->config.stock_count = session->stock_count;
    status = pf_sim_query_memory(&session->config, &session->memory);
    if (status != PF_STATUS_OK)
    {
        (void)fprintf(
            stderr,
            "native-playtest=fail stage=query-memory status=%s\n",
            pf_status_name(status));
        return 0;
    }

    session->state_memory = SDL_aligned_alloc(
        session->memory.state_alignment,
        session->memory.state_bytes);
    session->scratch_memory = SDL_aligned_alloc(
        session->memory.scratch_alignment,
        session->memory.scratch_bytes);
    if (session->state_memory == NULL || session->scratch_memory == NULL)
    {
        return 0;
    }

    status = pf_sim_init(
        session->state_memory,
        session->memory.state_bytes,
        session->scratch_memory,
        session->memory.scratch_bytes,
        &session->content_view,
        &session->config,
        &session->sim);
    if (status != PF_STATUS_OK)
    {
        (void)fprintf(
            stderr,
            "native-playtest=fail stage=sim-init status=%s\n",
            pf_status_name(status));
        return 0;
    }
    return pf_native_reset(session);
}

static int pf_native_initialize_sim(pf_native_session *session)
{
    pf_status status;

    status = reference_stage_content(
        PF_M4_REFERENCE_STAGE_BATTLEFIELD,
        &session->content);
    if (status != PF_STATUS_OK)
    {
        return 0;
    }
    status = make_content_view(
        &session->content,
        &session->content_view);
    if (status != PF_STATUS_OK)
    {
        return 0;
    }
    session->player_count = PF_NATIVE_DEFAULT_PLAYER_COUNT;
    session->mode = (uint8_t)PF_SIM_MODE_DUEL;
    session->stock_count = PF_SIM_DEFAULT_STOCK_COUNT;
    return pf_native_reinitialize_sim(session);
}

static int pf_native_tick(pf_native_session *session)
{
    struct inspection inspection;
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
    pf_tick_result result;
    pf_status status;

    status = inspect(session->sim, &inspection);
    if (status != PF_STATUS_OK)
    {
        return 0;
    }
    if (inspection.terminated != UINT8_C(0) ||
        inspection.truncated != UINT8_C(0))
    {
        return 1;
    }
    pf_native_collect_inputs(session, inspection.tick, inputs);
    status = pf_sim_tick(
        session->sim,
        inputs,
        (size_t)session->player_count,
        &result);
    if (status != PF_STATUS_OK)
    {
        (void)fprintf(
            stderr,
            "native-playtest=fail stage=tick status=%s\n",
            pf_status_name(status));
        return 0;
    }
    return result.fault_flags == UINT32_C(0);
}

static pf_native_view pf_native_make_view(int width, int height)
{
    const float world_left = -19.0F;
    const float world_right = 19.0F;
    const float world_top = -8.0F;
    const float world_bottom = 34.0F;
    const float margin = 28.0F;
    const float hud = 90.0F;
    const float scale_x =
        ((float)width - margin * 2.0F) / (world_right - world_left);
    const float scale_y =
        ((float)height - hud - margin * 2.0F) / (world_bottom - world_top);
    pf_native_view view;

    view.scale = scale_x < scale_y ? scale_x : scale_y;
    view.x = ((float)width - (world_right - world_left) * view.scale) *
                 0.5F -
             world_left * view.scale;
    view.y = hud + margin - world_top * view.scale;
    return view;
}

static SDL_FPoint pf_native_world_point(
    pf_native_view view,
    float x_f32,
    float y_f32)
{
    SDL_FPoint point;

    point.x = view.x + x_f32 * view.scale;
    point.y = view.y + y_f32 * view.scale;
    return point;
}

static void pf_native_set_color(
    SDL_Renderer *renderer,
    Uint8 red,
    Uint8 green,
    Uint8 blue,
    Uint8 alpha)
{
    (void)SDL_SetRenderDrawColor(renderer, red, green, blue, alpha);
}

static void pf_native_draw_thick_line(
    SDL_Renderer *renderer,
    SDL_FPoint a,
    SDL_FPoint b,
    float thickness)
{
    int offset;
    const int radius = (int)(thickness * 0.5F);

    for (offset = -radius; offset <= radius; ++offset)
    {
        (void)SDL_RenderLine(
            renderer,
            a.x,
            a.y + (float)offset,
            b.x,
            b.y + (float)offset);
    }
}

static void pf_native_draw_stage(
    SDL_Renderer *renderer,
    pf_native_view view,
    int collision_inspector)
{
    uint16_t line_count = UINT16_C(0);
    uint16_t index;

    if (reference_stage_geometry_line_count(
            PF_M4_REFERENCE_STAGE_BATTLEFIELD,
            &line_count) != PF_STATUS_OK)
    {
        return;
    }

    for (index = UINT16_C(0); index < line_count; ++index)
    {
        reference_stage_line line;
        SDL_FPoint start;
        SDL_FPoint end;

        if (reference_stage_geometry_line(
                PF_M4_REFERENCE_STAGE_BATTLEFIELD,
                index,
                &line) != PF_STATUS_OK)
        {
            continue;
        }
        if (collision_inspector != 0)
        {
            switch ((reference_stage_line_kind)line.kind)
            {
            case PF_M4_REFERENCE_STAGE_LINE_FLOOR:
                pf_native_set_color(renderer, 93u, 227u, 222u, 255u);
                break;
            case PF_M4_REFERENCE_STAGE_LINE_CEILING:
                pf_native_set_color(renderer, 177u, 145u, 255u, 255u);
                break;
            case PF_M4_REFERENCE_STAGE_LINE_LEFT_WALL:
            case PF_M4_REFERENCE_STAGE_LINE_RIGHT_WALL:
                pf_native_set_color(renderer, 241u, 156u, 224u, 255u);
                break;
            default:
                pf_native_set_color(renderer, 132u, 146u, 166u, 255u);
                break;
            }
        }
        else
        {
            pf_native_set_color(renderer, 151u, 209u, 230u, 255u);
        }
        start = pf_native_world_point(
            view,
            line.start_x_f32,
            line.start_y_f32);
        end = pf_native_world_point(
            view,
            line.end_x_f32,
            line.end_y_f32);
        pf_native_draw_thick_line(renderer, start, end, 3.0F);
    }
}

static int pf_native_is_crouched(uint8_t action)
{
    return action == (uint8_t)PF_M4_ACTION_CROUCH ||
           action == (uint8_t)PF_M4_ACTION_CROUCH_STEP ||
           action == (uint8_t)PF_M4_ACTION_CROUCH_START ||
           action == (uint8_t)PF_M4_ACTION_CROUCH_END;
}

static void pf_native_draw_circle(
    SDL_Renderer *renderer,
    float center_x,
    float center_y,
    float radius)
{
    const int segments = 32;
    int index;
    float previous_x = center_x + radius;
    float previous_y = center_y;

    for (index = 1; index <= segments; ++index)
    {
        const float angle =
            (float)index * 6.28318530718F / (float)segments;
        const float x = center_x + cosf(angle) * radius;
        const float y = center_y + sinf(angle) * radius;
        (void)SDL_RenderLine(
            renderer,
            previous_x,
            previous_y,
            x,
            y);
        previous_x = x;
        previous_y = y;
    }
}

static void pf_native_draw_hurt_capsule(
    SDL_Renderer *renderer,
    pf_native_view view,
    const hurt_capsule_inspection *capsule)
{
    const SDL_FPoint endpoint_a = pf_native_world_point(
        view,
        capsule->endpoint_a_x_f32,
        capsule->endpoint_a_y_f32);
    const SDL_FPoint endpoint_b = pf_native_world_point(
        view,
        capsule->endpoint_b_x_f32,
        capsule->endpoint_b_y_f32);
    const float radius = capsule->radius_f32 * view.scale;
    const float delta_x = endpoint_b.x - endpoint_a.x;
    const float delta_y = endpoint_b.y - endpoint_a.y;
    const float length = sqrtf(delta_x * delta_x + delta_y * delta_y);

    if (length > 0.001F)
    {
        const float normal_x = -delta_y * radius / length;
        const float normal_y = delta_x * radius / length;
        (void)SDL_RenderLine(
            renderer,
            endpoint_a.x + normal_x,
            endpoint_a.y + normal_y,
            endpoint_b.x + normal_x,
            endpoint_b.y + normal_y);
        (void)SDL_RenderLine(
            renderer,
            endpoint_a.x - normal_x,
            endpoint_a.y - normal_y,
            endpoint_b.x - normal_x,
            endpoint_b.y - normal_y);
    }
    pf_native_draw_circle(
        renderer,
        endpoint_a.x,
        endpoint_a.y,
        radius);
    pf_native_draw_circle(
        renderer,
        endpoint_b.x,
        endpoint_b.y,
        radius);
}

static void pf_native_draw_player(
    SDL_Renderer *renderer,
    pf_native_view view,
    const struct content *content,
    const player_inspection *player,
    int player_index,
    int collision_inspector)
{
    const int crouched = pf_native_is_crouched(player->action_state);
    const float half_width = content->fighter.half_width_f32 * view.scale;
    const float full_half_height =
        content->fighter.half_height_f32 * view.scale;
    const float half_height =
        crouched != 0 ? full_half_height * 0.58F : full_half_height;
    const SDL_FPoint position = pf_native_world_point(
        view,
        player->position_x_f32,
        player->position_y_f32);
    SDL_FRect body;

    if (player->active == UINT8_C(0))
    {
        return;
    }
    body.x = position.x - half_width;
    body.y = position.y - half_height * 2.0F;
    body.w = half_width * 2.0F;
    body.h = half_height * 2.0F;
    switch (player_index)
    {
    case 0:
        pf_native_set_color(
            renderer,
            crouched != 0 ? 37u : 45u,
            crouched != 0 ? 232u : 208u,
            crouched != 0 ? 179u : 242u,
            255u);
        break;
    case 1:
        pf_native_set_color(
            renderer,
            crouched != 0 ? 255u : 249u,
            crouched != 0 ? 164u : 103u,
            crouched != 0 ? 92u : 142u,
            255u);
        break;
    case 2:
        pf_native_set_color(renderer, 255u, 215u, 91u, 255u);
        break;
    default:
        pf_native_set_color(renderer, 185u, 126u, 255u, 255u);
        break;
    }
    (void)SDL_RenderFillRect(renderer, &body);

    if (collision_inspector != 0 &&
        player->hurt_capsule_count != UINT8_C(0))
    {
        uint8_t capsule_index;

        pf_native_set_color(
            renderer,
            player_index == 0 ? 107u : 224u,
            player_index == 0 ? 231u : 174u,
            255u,
            255u);
        for (capsule_index = UINT8_C(0);
             capsule_index < player->hurt_capsule_count;
             ++capsule_index)
        {
            pf_native_draw_hurt_capsule(
                renderer,
                view,
                &player->hurt_capsules[capsule_index]);
        }
    }

    pf_native_set_color(renderer, 8u, 17u, 30u, 255u);
    if (player->facing < INT8_C(0))
    {
        (void)SDL_RenderLine(
            renderer,
            body.x,
            body.y + body.h * 0.35F,
            body.x - 6.0F,
            body.y + body.h * 0.35F);
    }
    else
    {
        (void)SDL_RenderLine(
            renderer,
            body.x + body.w,
            body.y + body.h * 0.35F,
            body.x + body.w + 6.0F,
            body.y + body.h * 0.35F);
    }

    if (player->shield_active != UINT8_C(0))
    {
        const float shield_width =
            (player->shield_right_f32 - player->shield_left_f32) *
            view.scale;
        const float shield_height =
            (player->shield_bottom_f32 - player->shield_top_f32) *
            view.scale;
        const SDL_FPoint shield_center = pf_native_world_point(
            view,
            (player->shield_left_f32 + player->shield_right_f32) * 0.5f,
            (player->shield_top_f32 + player->shield_bottom_f32) * 0.5f);
        const float radius =
            (shield_width < shield_height ? shield_width : shield_height) *
            0.5F;
        pf_native_set_color(
            renderer,
            player->shield_strength == UINT16_MAX ? 123u : 240u,
            181u,
            255u,
            255u);
        pf_native_draw_circle(
            renderer,
            shield_center.x,
            shield_center.y,
            radius);
    }

    if (collision_inspector != 0 && player->hitbox_active != UINT8_C(0))
    {
        const SDL_FPoint left_top = pf_native_world_point(
            view,
            player->hitbox_left_f32,
            player->hitbox_top_f32);
        const SDL_FPoint right_bottom = pf_native_world_point(
            view,
            player->hitbox_right_f32,
            player->hitbox_bottom_f32);
        SDL_FRect hitbox;
        hitbox.x = left_top.x;
        hitbox.y = left_top.y;
        hitbox.w = right_bottom.x - left_top.x;
        hitbox.h = right_bottom.y - left_top.y;
        pf_native_set_color(renderer, 255u, 190u, 91u, 255u);
        (void)SDL_RenderRect(renderer, &hitbox);
    }
}

static void pf_native_draw_blast_inset(
    SDL_Renderer *renderer,
    const stage_inspection *stage,
    int width)
{
    const float inset_width = 128.0F;
    const float inset_height = 68.0F;
    const float left = (float)width - inset_width - 16.0F;
    const float top = 10.0F;
    SDL_FRect rect = {left, top, inset_width, inset_height};
    const float center_x =
        left + inset_width * (-stage->blast_left_f32) /
                   (stage->blast_right_f32 - stage->blast_left_f32);
    const float floor_y =
        top + inset_height *
                  (stage->floor_y_f32 - stage->blast_top_f32) /
                  (stage->blast_bottom_f32 - stage->blast_top_f32);

    pf_native_set_color(renderer, 255u, 103u, 143u, 255u);
    (void)SDL_RenderRect(renderer, &rect);
    pf_native_set_color(renderer, 93u, 227u, 222u, 255u);
    (void)SDL_RenderLine(
        renderer,
        center_x - 32.0F,
        floor_y,
        center_x + 32.0F,
        floor_y);
    pf_native_set_color(renderer, 180u, 190u, 207u, 255u);
    (void)SDL_RenderDebugText(renderer, left, top + inset_height + 3.0F, "BLAST ZONE");
}

static void pf_native_format_result(
    const struct inspection *inspection,
    char *text,
    size_t text_size)
{
    char winners[48];
    size_t used = 0u;
    int winner_count = 0;
    int player;

    if (inspection->truncated != UINT8_C(0))
    {
        (void)snprintf(text, text_size, "TIME LIMIT");
        return;
    }

    winners[0] = '\0';
    for (player = 0; player < (int)inspection->player_count; ++player)
    {
        int written;

        if ((inspection->winner_mask & (uint8_t)(UINT8_C(1) << player)) ==
            UINT8_C(0))
        {
            continue;
        }
        written = snprintf(
            winners + used,
            sizeof(winners) - used,
            "%sP%d",
            winner_count == 0 ? "" : " + ",
            player + 1);
        if (written < 0 || (size_t)written >= sizeof(winners) - used)
        {
            break;
        }
        used += (size_t)written;
        ++winner_count;
    }

    if (winner_count == 0)
    {
        (void)snprintf(text, text_size, "MATCH OVER - DRAW");
    }
    else
    {
        (void)snprintf(
            text,
            text_size,
            "MATCH OVER - %s%s WINS",
            winner_count == 1 ? "" : "TEAM ",
            winners);
    }
}

static int pf_native_result_format_smoke(void)
{
    static const struct pf_native_result_case
    {
        uint8_t player_count;
        uint8_t terminated;
        uint8_t truncated;
        uint8_t winner_mask;
        const char *expected;
    } cases[] = {
        {UINT8_C(2),
         UINT8_C(1),
         UINT8_C(0),
         UINT8_C(2),
         "MATCH OVER - P2 WINS"},
        {UINT8_C(4),
         UINT8_C(1),
         UINT8_C(0),
         UINT8_C(5),
         "MATCH OVER - TEAM P1 + P3 WINS"},
        {UINT8_C(2),
         UINT8_C(1),
         UINT8_C(0),
         UINT8_C(0),
         "MATCH OVER - DRAW"},
        {UINT8_C(2),
         UINT8_C(0),
         UINT8_C(1),
         UINT8_C(0),
         "TIME LIMIT"}};
    struct inspection inspection;
    char result[96];
    size_t index;

    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); ++index)
    {
        (void)memset(&inspection, 0, sizeof(inspection));
        inspection.player_count = cases[index].player_count;
        inspection.terminated = cases[index].terminated;
        inspection.truncated = cases[index].truncated;
        inspection.winner_mask = cases[index].winner_mask;
        pf_native_format_result(&inspection, result, sizeof(result));
        if (strcmp(result, cases[index].expected) != 0)
        {
            return 0;
        }
    }
    return 1;
}

static void pf_native_draw_result_banner(
    SDL_Renderer *renderer,
    const struct inspection *inspection,
    int width,
    int height)
{
    const float banner_width =
        width >= 560 ? 520.0F : (width >= 48 ? (float)width - 32.0F
                                            : (float)width);
    const float banner_height = 104.0F;
    SDL_FRect banner = {
        ((float)width - banner_width) * 0.5F,
        ((float)height - banner_height) * 0.42F,
        banner_width,
        banner_height};
    char result[96];
    const char *rematch = "R - REMATCH";
    float result_x;
    float rematch_x;

    pf_native_format_result(inspection, result, sizeof(result));
    result_x = ((float)width - (float)strlen(result) * 8.0F) * 0.5F;
    rematch_x = ((float)width - (float)strlen(rematch) * 8.0F) * 0.5F;

    pf_native_set_color(renderer, 10u, 24u, 42u, 255u);
    (void)SDL_RenderFillRect(renderer, &banner);
    pf_native_set_color(renderer, 255u, 215u, 91u, 255u);
    (void)SDL_RenderRect(renderer, &banner);
    (void)SDL_RenderDebugText(
        renderer,
        result_x,
        banner.y + 27.0F,
        result);
    pf_native_set_color(renderer, 93u, 227u, 222u, 255u);
    (void)SDL_RenderDebugText(
        renderer,
        rematch_x,
        banner.y + 59.0F,
        rematch);
}

static int pf_native_render(
    SDL_Renderer *renderer,
    pf_native_session *session)
{
    struct inspection inspection;
    pf_native_view view;
    int width;
    int height;
    int player;

    if (inspect(session->sim, &inspection) != PF_STATUS_OK ||
        !SDL_GetCurrentRenderOutputSize(renderer, &width, &height))
    {
        return 0;
    }
    view = pf_native_make_view(width, height);
    pf_native_set_color(renderer, 7u, 16u, 29u, 255u);
    if (!SDL_RenderClear(renderer))
    {
        return 0;
    }

    pf_native_draw_stage(renderer, view, session->collision_inspector);
    for (player = 0; player < (int)inspection.player_count; ++player)
    {
        pf_native_draw_player(
            renderer,
            view,
            &session->content,
            &inspection.players[player],
            player,
            session->collision_inspector);
    }

    pf_native_set_color(renderer, 218u, 229u, 241u, 255u);
    for (player = 0; player < (int)inspection.player_count; ++player)
    {
        (void)SDL_RenderDebugTextFormat(
            renderer,
            12.0F,
            10.0F + 14.0F * (float)player,
            "P%d %u%%  stocks %u  action %u:%u",
            player + 1,
            (unsigned)(
                inspection.players[player].damage_f32 /
                (uint32_t)1.0f),
            (unsigned)inspection.players[player].stocks_remaining,
            (unsigned)inspection.players[player].action_state,
            (unsigned)inspection.players[player].action_ticks);
    }
    (void)SDL_RenderDebugTextFormat(
        renderer,
        12.0F,
        10.0F + 14.0F * (float)inspection.player_count,
        "Battlefield  %uP %s %u-stock  tick %" PRIu64
        "  controllers %d  %s  collision %s",
        (unsigned)session->player_count,
        session->mode == (uint8_t)PF_SIM_MODE_TEAMS ? "teams" : "duel",
        (unsigned)session->stock_count,
        inspection.tick,
        pf_native_active_device_count(session),
        inspection.terminated != UINT8_C(0) ||
                inspection.truncated != UINT8_C(0)
            ? "RESULT"
            : (session->paused != 0 ? "PAUSED" : "60 Hz"),
        session->collision_inspector != 0 ? "ON" : "OFF");
    (void)SDL_RenderDebugText(
        renderer,
        12.0F,
        (float)height - 18.0F,
        "Esc quit | P pause | . step | R reset | F1 collision | F2 duel/teams | F3 stocks");
    pf_native_draw_blast_inset(renderer, &inspection.stage, width);
    if (inspection.terminated != UINT8_C(0) ||
        inspection.truncated != UINT8_C(0))
    {
        pf_native_draw_result_banner(
            renderer,
            &inspection,
            width,
            height);
    }
    return SDL_RenderPresent(renderer);
}

static void pf_native_handle_event(
    pf_native_session *session,
    const SDL_Event *event,
    int *running)
{
    if (event->type == SDL_EVENT_QUIT)
    {
        *running = 0;
        return;
    }
    if (event->type == SDL_EVENT_GAMEPAD_ADDED ||
        event->type == SDL_EVENT_GAMEPAD_REMOVED ||
        event->type == SDL_EVENT_JOYSTICK_ADDED ||
        event->type == SDL_EVENT_JOYSTICK_REMOVED)
    {
        pf_native_refresh_devices(session);
        return;
    }
    if (event->type != SDL_EVENT_KEY_DOWN || event->key.repeat)
    {
        return;
    }
    switch (event->key.key)
    {
    case SDLK_ESCAPE:
        *running = 0;
        break;
    case SDLK_P:
        session->paused = session->paused == 0;
        break;
    case SDLK_PERIOD:
        session->step_requested = 1;
        break;
    case SDLK_R:
        (void)pf_native_reset(session);
        break;
    case SDLK_F1:
        session->collision_inspector = session->collision_inspector == 0;
        break;
    case SDLK_F2:
        if (session->mode == (uint8_t)PF_SIM_MODE_DUEL)
        {
            session->player_count = UINT8_C(4);
            session->mode = (uint8_t)PF_SIM_MODE_TEAMS;
        }
        else
        {
            session->player_count = UINT8_C(2);
            session->mode = (uint8_t)PF_SIM_MODE_DUEL;
        }
        if (!pf_native_reinitialize_sim(session))
        {
            *running = 0;
        }
        break;
    case SDLK_F3:
        session->stock_count =
            session->stock_count >= UINT8_C(8)
                ? UINT8_C(1)
                : (uint8_t)(session->stock_count + UINT8_C(1));
        if (!pf_native_reinitialize_sim(session))
        {
            *running = 0;
        }
        break;
    default:
        break;
    }
}

int native_playtest_run(void)
{
    pf_native_session session;
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_GPUDevice *gpu_device;
    uint64_t next_tick;
    int running = 1;
    int failed = 0;
    int result = 1;

    (void)memset(&session, 0, sizeof(session));
    session.collision_inspector = 1;
    if (!SDL_Init(
            SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD |
            SDL_INIT_JOYSTICK))
    {
        return pf_native_fail("init");
    }
    window = SDL_CreateWindow(
        "Ultra Performance Platform Fighter - Battlefield Playtest",
        1280,
        720,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (window == NULL)
    {
        (void)pf_native_fail("create-window");
        goto cleanup;
    }
    renderer = SDL_CreateGPURenderer(NULL, window);
    if (renderer == NULL)
    {
        renderer = SDL_CreateRenderer(window, NULL);
    }
    if (renderer == NULL)
    {
        (void)pf_native_fail("create-renderer");
        goto cleanup;
    }
    (void)SDL_SetRenderVSync(renderer, 1);
    if (!pf_native_initialize_sim(&session))
    {
        (void)pf_native_fail("initialize-sim");
        goto cleanup;
    }
    pf_native_refresh_devices(&session);
    gpu_device = SDL_GetGPURendererDevice(renderer);
    (void)printf(
        "native-playtest=ready stage=battlefield renderer=%s gpu=%s devices=%d\n",
        SDL_GetRendererName(renderer),
        gpu_device == NULL ? "fallback" : SDL_GetGPUDeviceDriver(gpu_device),
        session.device_count);

    next_tick = SDL_GetTicksNS();
    while (running != 0)
    {
        SDL_Event event;
        int catchup = 0;
        uint64_t now;

        while (SDL_PollEvent(&event))
        {
            pf_native_handle_event(&session, &event, &running);
        }
        now = SDL_GetTicksNS();
        while (((session.paused == 0 && now >= next_tick) ||
                session.step_requested != 0) &&
               catchup < 4)
        {
            if (!pf_native_tick(&session))
            {
                failed = 1;
                running = 0;
                break;
            }
            session.step_requested = 0;
            next_tick += PF_NATIVE_TICK_NS;
            ++catchup;
        }
        if (session.paused != 0)
        {
            next_tick = now + PF_NATIVE_TICK_NS;
        }
        else if (catchup == 4 && now >= next_tick)
        {
            next_tick = now + PF_NATIVE_TICK_NS;
        }
        if (!pf_native_render(renderer, &session))
        {
            failed = 1;
            running = 0;
            break;
        }
        SDL_Delay(1u);
    }
    result = failed;

cleanup:
    pf_native_close_devices(&session);
    if (session.sim != NULL)
    {
        (void)pf_sim_deinit(session.sim);
    }
    SDL_aligned_free(session.scratch_memory);
    SDL_aligned_free(session.state_memory);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}
