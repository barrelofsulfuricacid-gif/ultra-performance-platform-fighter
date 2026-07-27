#include "movement_model.h"

#include <SDL3/SDL.h>

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    WINDOW_WIDTH = 1280,
    WINDOW_HEIGHT = 720,
    TRAIL_CAPACITY = 180,
    GAMEPAD_DEAD_ZONE = 6000
};

static const Uint64 k_tick_nanoseconds = UINT64_C(1000000000) / 60U;

typedef enum Candidate
{
    CANDIDATE_A = 0,
    CANDIDATE_B = 1
} Candidate;

typedef struct Trail
{
    M0MovementView samples[TRAIL_CAPACITY];
    size_t head;
    size_t count;
} Trail;

typedef struct Controls
{
    int16_t move_x;
    uint8_t jump_held;
    uint8_t down_held;
} Controls;

typedef struct Playtest
{
    M0MovementPair pair;
    Trail float_trail;
    Trail fixed_trail;
    SDL_Gamepad *gamepad;
    uint64_t seed;
    double max_delta;
    int float_is_a;
    int focused_candidate;
    int reveal;
    int paused;
    int show_trails;
    int jump_was_held;
    int jump_pending;
} Playtest;

static void fail_sdl(const char *operation)
{
    fprintf(stderr, "%s failed: %s\n", operation, SDL_GetError());
}

static uint64_t mix_seed(uint64_t value)
{
    value ^= value >> 30U;
    value *= UINT64_C(0xbf58476d1ce4e5b9);
    value ^= value >> 27U;
    value *= UINT64_C(0x94d049bb133111eb);
    value ^= value >> 31U;
    return value;
}

static void trail_clear(Trail *trail)
{
    memset(trail, 0, sizeof(*trail));
}

static void trail_push(Trail *trail, M0MovementView sample)
{
    trail->samples[trail->head] = sample;
    trail->head = (trail->head + 1U) % TRAIL_CAPACITY;
    if (trail->count < TRAIL_CAPACITY)
    {
        trail->count++;
    }
}

static void playtest_reset(Playtest *playtest)
{
    m0_pair_reset(&playtest->pair);
    trail_clear(&playtest->float_trail);
    trail_clear(&playtest->fixed_trail);
    trail_push(&playtest->float_trail,
               m0_float_view(&playtest->pair.float32));
    trail_push(&playtest->fixed_trail,
               m0_fixed_view(&playtest->pair.q16_16));
    playtest->max_delta = 0.0;
    playtest->jump_pending = 0;
}

static void playtest_init(Playtest *playtest, uint64_t seed)
{
    memset(playtest, 0, sizeof(*playtest));
    playtest->seed = seed;
    playtest->float_is_a = (int)(mix_seed(seed) & UINT64_C(1));
    playtest->focused_candidate = CANDIDATE_A;
    playtest->show_trails = 1;
    playtest_reset(playtest);
}

static M0MovementView candidate_view(const Playtest *playtest,
                                     Candidate candidate)
{
    int wants_float =
        (candidate == CANDIDATE_A) == (playtest->float_is_a != 0);
    return wants_float ? m0_float_view(&playtest->pair.float32)
                       : m0_fixed_view(&playtest->pair.q16_16);
}

static const Trail *candidate_trail(const Playtest *playtest,
                                    Candidate candidate)
{
    int wants_float =
        (candidate == CANDIDATE_A) == (playtest->float_is_a != 0);
    return wants_float ? &playtest->float_trail : &playtest->fixed_trail;
}

static const char *candidate_model(const Playtest *playtest,
                                   Candidate candidate)
{
    int wants_float =
        (candidate == CANDIDATE_A) == (playtest->float_is_a != 0);
    return wants_float ? "FLOAT32" : "Q16.16";
}

static void playtest_tick(Playtest *playtest, Controls controls)
{
    M0MovementInput input;
    M0MovementView float_view;
    M0MovementView fixed_view;
    double delta_x;
    double delta_y;

    input.move_x = controls.move_x;
    input.jump_pressed = (uint8_t)(playtest->jump_pending != 0);
    input.jump_held = controls.jump_held;
    input.down_held = controls.down_held;
    playtest->jump_pending = 0;

    m0_pair_step(&playtest->pair, input);
    float_view = m0_float_view(&playtest->pair.float32);
    fixed_view = m0_fixed_view(&playtest->pair.q16_16);
    trail_push(&playtest->float_trail, float_view);
    trail_push(&playtest->fixed_trail, fixed_view);

    delta_x = fabs(float_view.x - fixed_view.x);
    delta_y = fabs(float_view.y - fixed_view.y);
    if (delta_x > playtest->max_delta)
    {
        playtest->max_delta = delta_x;
    }
    if (delta_y > playtest->max_delta)
    {
        playtest->max_delta = delta_y;
    }
}

static void close_gamepad(Playtest *playtest)
{
    if (playtest->gamepad != NULL)
    {
        SDL_CloseGamepad(playtest->gamepad);
        playtest->gamepad = NULL;
    }
}

static void open_gamepad_id(Playtest *playtest, SDL_JoystickID id)
{
    if (playtest->gamepad == NULL)
    {
        playtest->gamepad = SDL_OpenGamepad(id);
    }
}

static void open_first_gamepad(Playtest *playtest)
{
    int count = 0;
    SDL_JoystickID *ids;

    if (playtest->gamepad != NULL)
    {
        return;
    }
    ids = SDL_GetGamepads(&count);
    if (ids != NULL)
    {
        if (count > 0)
        {
            open_gamepad_id(playtest, ids[0]);
        }
        SDL_free(ids);
    }
}

static int absolute_int(int value)
{
    return value < 0 ? -value : value;
}

static Controls read_controls(Playtest *playtest)
{
    const bool *keys = SDL_GetKeyboardState(NULL);
    Controls controls = {0};
    int keyboard_axis = 0;
    int gamepad_axis = 0;
    int keyboard_strength = M0_AXIS_MAX;
    int jump_held;
    int down_held;

    if (keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT])
    {
        keyboard_strength = 13500;
    }
    if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT])
    {
        keyboard_axis -= keyboard_strength;
    }
    if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT])
    {
        keyboard_axis += keyboard_strength;
    }

    jump_held =
        keys[SDL_SCANCODE_SPACE] || keys[SDL_SCANCODE_W] ||
        keys[SDL_SCANCODE_UP];
    down_held = keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN];

    if (playtest->gamepad != NULL)
    {
        int raw_axis = SDL_GetGamepadAxis(playtest->gamepad,
                                          SDL_GAMEPAD_AXIS_LEFTX);
        if (absolute_int(raw_axis) >= GAMEPAD_DEAD_ZONE)
        {
            gamepad_axis = m0_axis_clamp(raw_axis);
        }
        jump_held =
            jump_held ||
            SDL_GetGamepadButton(playtest->gamepad,
                                 SDL_GAMEPAD_BUTTON_SOUTH);
        down_held =
            down_held ||
            SDL_GetGamepadButton(playtest->gamepad,
                                 SDL_GAMEPAD_BUTTON_DPAD_DOWN) ||
            SDL_GetGamepadAxis(playtest->gamepad,
                               SDL_GAMEPAD_AXIS_LEFTY) > 16000;
    }

    controls.move_x =
        absolute_int(gamepad_axis) > absolute_int(keyboard_axis)
            ? (int16_t)gamepad_axis
            : (int16_t)keyboard_axis;
    controls.jump_held = (uint8_t)(jump_held != 0);
    controls.down_held = (uint8_t)(down_held != 0);

    if (jump_held && !playtest->jump_was_held)
    {
        playtest->jump_pending = 1;
    }
    playtest->jump_was_held = jump_held;
    return controls;
}

static float world_x_to_screen(const SDL_FRect *panel, double world_x)
{
    const M0StageGeometry *geometry = m0_stage_geometry();
    double width = geometry->blast_right - geometry->blast_left;
    return panel->x +
           (float)(((world_x - geometry->blast_left) / width) * panel->w);
}

static float world_y_to_screen(const SDL_FRect *panel, double world_y)
{
    const double world_top = -2.5;
    const M0StageGeometry *geometry = m0_stage_geometry();
    double height = geometry->blast_bottom - world_top;
    return panel->y +
           (float)(((world_y - world_top) / height) * panel->h);
}

static void set_color(SDL_Renderer *renderer, Uint8 red, Uint8 green,
                      Uint8 blue, Uint8 alpha)
{
    (void)SDL_SetRenderDrawColor(renderer, red, green, blue, alpha);
}

static void render_trail(SDL_Renderer *renderer, const SDL_FRect *panel,
                         const Trail *trail, Uint8 red, Uint8 green,
                         Uint8 blue)
{
    size_t index;
    size_t oldest =
        (trail->head + TRAIL_CAPACITY - trail->count) % TRAIL_CAPACITY;

    (void)SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (index = 0; index < trail->count; ++index)
    {
        size_t sample_index = (oldest + index) % TRAIL_CAPACITY;
        const M0MovementView *sample = &trail->samples[sample_index];
        Uint8 alpha =
            (Uint8)(24U + (index * 112U) /
                              (trail->count == 0U ? 1U : trail->count));
        SDL_FRect point = {
            world_x_to_screen(panel, sample->x) - 1.5f,
            world_y_to_screen(panel, sample->y) - 1.5f,
            3.0f,
            3.0f};
        set_color(renderer, red, green, blue, alpha);
        (void)SDL_RenderFillRect(renderer, &point);
    }
    (void)SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

static void render_stage(SDL_Renderer *renderer, const SDL_FRect *panel)
{
    const M0StageGeometry *geometry = m0_stage_geometry();
    SDL_FRect floor_rect;
    SDL_FRect platform_rect;
    SDL_FRect blast_rect = *panel;

    set_color(renderer, 70, 77, 96, 255);
    (void)SDL_RenderRect(renderer, &blast_rect);

    floor_rect.x = world_x_to_screen(panel, geometry->floor_left);
    floor_rect.y = world_y_to_screen(panel, geometry->floor_y);
    floor_rect.w =
        world_x_to_screen(panel, geometry->floor_right) - floor_rect.x;
    floor_rect.h = 10.0f;
    set_color(renderer, 93, 108, 124, 255);
    (void)SDL_RenderFillRect(renderer, &floor_rect);

    platform_rect.x = world_x_to_screen(panel, geometry->platform_left);
    platform_rect.y = world_y_to_screen(panel, geometry->platform_y);
    platform_rect.w =
        world_x_to_screen(panel, geometry->platform_right) -
        platform_rect.x;
    platform_rect.h = 7.0f;
    set_color(renderer, 111, 126, 145, 255);
    (void)SDL_RenderFillRect(renderer, &platform_rect);
}

static void render_fighter(SDL_Renderer *renderer, const SDL_FRect *panel,
                           M0MovementView view, Uint8 red, Uint8 green,
                           Uint8 blue)
{
    const M0StageGeometry *geometry = m0_stage_geometry();
    float left =
        world_x_to_screen(panel, view.x - geometry->fighter_half_width);
    float right =
        world_x_to_screen(panel, view.x + geometry->fighter_half_width);
    float top =
        world_y_to_screen(panel, view.y - geometry->fighter_half_height);
    float bottom =
        world_y_to_screen(panel, view.y + geometry->fighter_half_height);
    SDL_FRect fighter = {left, top, right - left, bottom - top};
    SDL_FRect eye = {
        view.velocity_x < 0.0 ? left + 3.0f : right - 7.0f,
        top + 7.0f,
        4.0f,
        4.0f};

    set_color(renderer, red, green, blue, 255);
    (void)SDL_RenderFillRect(renderer, &fighter);
    set_color(renderer, 236, 242, 248, 255);
    (void)SDL_RenderFillRect(renderer, &eye);
}

static void render_candidate(SDL_Renderer *renderer,
                             const Playtest *playtest,
                             Candidate candidate,
                             const SDL_FRect *panel,
                             Uint8 red, Uint8 green, Uint8 blue)
{
    M0MovementView view = candidate_view(playtest, candidate);
    const char *letter = candidate == CANDIDATE_A ? "A" : "B";
    int focused = playtest->focused_candidate == (int)candidate;

    render_stage(renderer, panel);
    if (playtest->show_trails)
    {
        render_trail(renderer, panel, candidate_trail(playtest, candidate),
                     red, green, blue);
    }
    render_fighter(renderer, panel, view, red, green, blue);

    if (focused)
    {
        SDL_FRect focus = {
            panel->x - 4.0f,
            panel->y - 4.0f,
            panel->w + 8.0f,
            panel->h + 8.0f};
        set_color(renderer, red, green, blue, 255);
        (void)SDL_RenderRect(renderer, &focus);
    }

    set_color(renderer, 232, 236, 242, 255);
    if (playtest->reveal)
    {
        (void)SDL_RenderDebugTextFormat(
            renderer, panel->x, panel->y - 30.0f,
            "CANDIDATE %s: %s%s", letter,
            candidate_model(playtest, candidate),
            focused ? "  [FOCUS]" : "");
    }
    else
    {
        (void)SDL_RenderDebugTextFormat(
            renderer, panel->x, panel->y - 30.0f,
            "CANDIDATE %s: HIDDEN%s", letter,
            focused ? "  [FOCUS]" : "");
    }
    (void)SDL_RenderDebugTextFormat(
        renderer, panel->x, panel->y + panel->h + 10.0f,
        "POS %+.5f %+.5f  VEL %+.5f %+.5f  %s",
        view.x, view.y, view.velocity_x, view.velocity_y,
        view.grounded ? (view.on_platform ? "PLATFORM" : "GROUND")
                      : "AIR");
}

static void render_scene(SDL_Renderer *renderer, const Playtest *playtest,
                         int width, int height)
{
    const float margin = 42.0f;
    const float gap = 34.0f;
    float panel_width = ((float)width - (2.0f * margin) - gap) / 2.0f;
    float panel_height = (float)height - 214.0f;
    SDL_FRect left_panel = {margin, 116.0f, panel_width, panel_height};
    SDL_FRect right_panel = {
        margin + panel_width + gap, 116.0f, panel_width, panel_height};
    M0MovementView float_view = m0_float_view(&playtest->pair.float32);
    M0MovementView fixed_view = m0_fixed_view(&playtest->pair.q16_16);
    double current_delta_x = fabs(float_view.x - fixed_view.x);
    double current_delta_y = fabs(float_view.y - fixed_view.y);
    const char *device = "KEYBOARD";

    if (playtest->gamepad != NULL &&
        SDL_GetGamepadName(playtest->gamepad) != NULL)
    {
        device = SDL_GetGamepadName(playtest->gamepad);
    }

    set_color(renderer, 15, 18, 27, 255);
    (void)SDL_RenderClear(renderer);

    set_color(renderer, 232, 236, 242, 255);
    (void)SDL_RenderDebugText(
        renderer, margin, 18.0f,
        "M0 BLIND MOVEMENT REPRESENTATION PLAYTEST");
    (void)SDL_RenderDebugText(
        renderer, margin, 36.0f,
        "DASH: A/D   WALK: SHIFT+A/D   JUMP: SPACE/PAD SOUTH   DOWN: S/DOWN");
    (void)SDL_RenderDebugText(
        renderer, margin, 52.0f,
        "1/2 FOCUS   R RESET   P PAUSE   N STEP   T TRAILS   V REVEAL   ESC QUIT");
    (void)SDL_RenderDebugTextFormat(
        renderer, margin, 68.0f, "INPUT: %s   TICK: %" PRIu32 "   %s",
        device, float_view.tick, playtest->paused ? "PAUSED" : "RUNNING");

    render_candidate(renderer, playtest, CANDIDATE_A, &left_panel,
                     48, 205, 190);
    render_candidate(renderer, playtest, CANDIDATE_B, &right_panel,
                     239, 151, 65);

    set_color(renderer, 198, 207, 218, 255);
    (void)SDL_RenderDebugTextFormat(
        renderer, margin, (float)height - 24.0f,
        "CURRENT DELTA X %.7f Y %.7f   SESSION MAX %.7f   SEED %" PRIu64,
        current_delta_x, current_delta_y, playtest->max_delta,
        playtest->seed);
    (void)SDL_RenderPresent(renderer);
}

static void handle_key(Playtest *playtest, SDL_Scancode scancode,
                       int *running, int *single_step)
{
    switch (scancode)
    {
        case SDL_SCANCODE_ESCAPE:
            *running = 0;
            break;
        case SDL_SCANCODE_1:
            playtest->focused_candidate = CANDIDATE_A;
            break;
        case SDL_SCANCODE_2:
            playtest->focused_candidate = CANDIDATE_B;
            break;
        case SDL_SCANCODE_R:
            playtest_reset(playtest);
            break;
        case SDL_SCANCODE_P:
            playtest->paused = !playtest->paused;
            break;
        case SDL_SCANCODE_N:
            *single_step = 1;
            break;
        case SDL_SCANCODE_T:
            playtest->show_trails = !playtest->show_trails;
            break;
        case SDL_SCANCODE_V:
            playtest->reveal = !playtest->reveal;
            break;
        default:
            break;
    }
}

static void handle_events(Playtest *playtest, int *running,
                          int *single_step)
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_EVENT_QUIT)
        {
            *running = 0;
        }
        else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat)
        {
            handle_key(playtest, event.key.scancode, running, single_step);
        }
        else if (event.type == SDL_EVENT_GAMEPAD_ADDED)
        {
            open_gamepad_id(playtest, event.gdevice.which);
        }
        else if (event.type == SDL_EVENT_GAMEPAD_REMOVED &&
                 playtest->gamepad != NULL &&
                 SDL_GetGamepadID(playtest->gamepad) == event.gdevice.which)
        {
            close_gamepad(playtest);
            open_first_gamepad(playtest);
        }
    }
}

static int run_software_smoke(uint64_t seed, const char *screenshot_path)
{
    Playtest playtest;
    SDL_Surface *surface;
    SDL_Renderer *renderer;
    Controls controls = {0};
    uint32_t tick;
    uint64_t pixel_hash = UINT64_C(1469598103934665603);
    size_t byte_count;
    size_t index;
    const unsigned char *pixels;

    playtest_init(&playtest, seed);
    for (tick = 0; tick < 600U; ++tick)
    {
        controls.move_x =
            tick % 240U < 120U ? M0_AXIS_MAX : M0_AXIS_MIN;
        controls.jump_held = (uint8_t)(tick % 90U < 12U);
        controls.down_held = (uint8_t)(tick % 170U > 150U);
        if (tick % 90U == 0U)
        {
            playtest.jump_pending = 1;
        }
        playtest_tick(&playtest, controls);
    }

    surface = SDL_CreateSurface(WINDOW_WIDTH, WINDOW_HEIGHT,
                                SDL_PIXELFORMAT_RGBA32);
    if (surface == NULL)
    {
        fail_sdl("SDL_CreateSurface");
        return EXIT_FAILURE;
    }
    renderer = SDL_CreateSoftwareRenderer(surface);
    if (renderer == NULL)
    {
        fail_sdl("SDL_CreateSoftwareRenderer");
        SDL_DestroySurface(surface);
        return EXIT_FAILURE;
    }
    render_scene(renderer, &playtest, WINDOW_WIDTH, WINDOW_HEIGHT);
    if (screenshot_path != NULL &&
        !SDL_SaveBMP(surface, screenshot_path))
    {
        fail_sdl("SDL_SaveBMP");
        SDL_DestroyRenderer(renderer);
        SDL_DestroySurface(surface);
        return EXIT_FAILURE;
    }

    if (!SDL_LockSurface(surface))
    {
        fail_sdl("SDL_LockSurface");
        SDL_DestroyRenderer(renderer);
        SDL_DestroySurface(surface);
        return EXIT_FAILURE;
    }
    pixels = surface->pixels;
    byte_count = (size_t)surface->pitch * (size_t)surface->h;
    for (index = 0; index < byte_count; index += 97U)
    {
        pixel_hash ^= pixels[index];
        pixel_hash *= UINT64_C(1099511628211);
    }
    SDL_UnlockSurface(surface);
    SDL_DestroyRenderer(renderer);
    SDL_DestroySurface(surface);

    printf("sdl-smoke=pass ticks=%" PRIu32 " pixel_hash=%" PRIu64
           " fixed_hash=%" PRIu64 "\n",
           playtest.pair.q16_16.tick, pixel_hash,
           m0_fixed_hash(&playtest.pair.q16_16));
    return EXIT_SUCCESS;
}

static int parse_arguments(int argc, char **argv, int *smoke,
                           uint64_t *seed, int *seed_supplied,
                           const char **screenshot_path)
{
    int index;
    for (index = 1; index < argc; ++index)
    {
        if (strcmp(argv[index], "--smoke") == 0)
        {
            *smoke = 1;
        }
        else if (strcmp(argv[index], "--seed") == 0)
        {
            char *end = NULL;
            unsigned long long value;
            if (index + 1 >= argc)
            {
                fprintf(stderr, "--seed requires an unsigned integer\n");
                return 0;
            }
            value = strtoull(argv[++index], &end, 10);
            if (end == argv[index] || *end != '\0')
            {
                fprintf(stderr, "invalid --seed value: %s\n", argv[index]);
                return 0;
            }
            *seed = (uint64_t)value;
            *seed_supplied = 1;
        }
        else if (strcmp(argv[index], "--screenshot") == 0)
        {
            if (index + 1 >= argc)
            {
                fprintf(stderr, "--screenshot requires a file path\n");
                return 0;
            }
            *screenshot_path = argv[++index];
        }
        else if (strcmp(argv[index], "--help") == 0)
        {
            printf("usage: %s [--smoke] [--seed unsigned-integer] "
                   "[--screenshot bmp-path]\n",
                   argv[0]);
            return -1;
        }
        else
        {
            fprintf(stderr, "unknown argument: %s\n", argv[index]);
            return 0;
        }
    }
    return 1;
}

int main(int argc, char **argv)
{
    Playtest playtest;
    SDL_Window *window;
    SDL_Renderer *renderer;
    Uint64 previous_time;
    Uint64 accumulator = 0U;
    uint64_t seed = 0U;
    const char *screenshot_path = NULL;
    int seed_supplied = 0;
    int smoke = 0;
    int parse_result =
        parse_arguments(argc, argv, &smoke, &seed, &seed_supplied,
                        &screenshot_path);

    if (parse_result <= 0)
    {
        return parse_result < 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (!SDL_Init(smoke ? SDL_INIT_EVENTS
                        : (SDL_INIT_VIDEO | SDL_INIT_EVENTS |
                           SDL_INIT_GAMEPAD)))
    {
        fail_sdl("SDL_Init");
        return EXIT_FAILURE;
    }

    if (!seed_supplied)
    {
        seed = smoke ? UINT64_C(20260727) : SDL_GetTicksNS();
    }
    if (smoke)
    {
        int result = run_software_smoke(seed, screenshot_path);
        SDL_Quit();
        return result;
    }

    playtest_init(&playtest, seed);
    open_first_gamepad(&playtest);

    window = SDL_CreateWindow(
        "M0 movement representation playtest",
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE);
    if (window == NULL)
    {
        fail_sdl("SDL_CreateWindow");
        close_gamepad(&playtest);
        SDL_Quit();
        return EXIT_FAILURE;
    }
    renderer = SDL_CreateRenderer(window, NULL);
    if (renderer == NULL)
    {
        fail_sdl("SDL_CreateRenderer");
        SDL_DestroyWindow(window);
        close_gamepad(&playtest);
        SDL_Quit();
        return EXIT_FAILURE;
    }
    (void)SDL_SetRenderVSync(renderer, 1);
    previous_time = SDL_GetTicksNS();

    {
        int running = 1;
        while (running)
        {
            Uint64 now;
            Uint64 elapsed;
            Controls controls;
            int single_step = 0;
            int width = WINDOW_WIDTH;
            int height = WINDOW_HEIGHT;

            handle_events(&playtest, &running, &single_step);
            controls = read_controls(&playtest);
            now = SDL_GetTicksNS();
            elapsed = now - previous_time;
            previous_time = now;
            if (elapsed > UINT64_C(250000000))
            {
                elapsed = UINT64_C(250000000);
            }
            accumulator += elapsed;

            if (playtest.paused)
            {
                accumulator = 0U;
                if (single_step)
                {
                    playtest_tick(&playtest, controls);
                }
            }
            else
            {
                int catchup_ticks = 0;
                while (accumulator >= k_tick_nanoseconds &&
                       catchup_ticks < 8)
                {
                    playtest_tick(&playtest, controls);
                    accumulator -= k_tick_nanoseconds;
                    catchup_ticks++;
                }
                if (catchup_ticks == 8 &&
                    accumulator >= k_tick_nanoseconds)
                {
                    accumulator = 0U;
                }
            }

            (void)SDL_GetRenderOutputSize(renderer, &width, &height);
            render_scene(renderer, &playtest, width, height);
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    close_gamepad(&playtest);
    SDL_Quit();
    return EXIT_SUCCESS;
}
