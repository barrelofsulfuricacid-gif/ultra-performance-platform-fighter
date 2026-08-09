#ifndef PF_TESTS_SIM_SSBM_STORED_ORACLE_H
#define PF_TESTS_SIM_SSBM_STORED_ORACLE_H

#include <stdint.h>

#define PF_SSBM_STORED_MAX_CAPSULES UINT8_C(32)
#define PF_SSBM_STORED_MAX_TRACE_SAMPLES UINT8_C(64)
#define PF_SSBM_STORED_MAX_TRACE_LANES UINT8_C(2)

typedef enum pf_ssbm_stored_case_mode
{
    PF_SSBM_STORED_RUNTIME = 0,
    PF_SSBM_STORED_GEOMETRY = 1
} pf_ssbm_stored_case_mode;

typedef struct pf_ssbm_stored_pose_track
{
    const char *action_name;
    uint8_t action_state;
    uint16_t first_source_frame;
    uint16_t last_source_frame;
    uint16_t source_frame_step;
} pf_ssbm_stored_pose_track;

typedef struct pf_ssbm_stored_case
{
    const char *id;
    pf_ssbm_stored_case_mode mode;
    uint8_t target_action;
    uint32_t distance_hundredths;
    uint32_t height_hundredths;
    uint16_t action_frame;
    uint16_t source_frame;
    int16_t target_stick_x_or_facing;
    int16_t target_stick_y;
    uint64_t target_buttons;
    uint16_t button_delay_or_jab_frame;
    uint16_t expected_hit_action_tick;
    uint8_t expect_hit;
} pf_ssbm_stored_case;

typedef struct pf_ssbm_stored_hurt_capsule
{
    int32_t endpoint_a_x_q16;
    int32_t endpoint_a_y_q16;
    int32_t endpoint_a_z_q16;
    int32_t endpoint_b_x_q16;
    int32_t endpoint_b_y_q16;
    int32_t endpoint_b_z_q16;
    int32_t radius_q16;
    uint8_t hurtbox_id;
    uint8_t height;
    uint8_t grabbable;
    uint8_t reserved;
} pf_ssbm_stored_hurt_capsule;

typedef uint8_t (*pf_ssbm_stored_pose_reader)(
    void *context,
    uint8_t action_state,
    uint16_t action_frame,
    pf_ssbm_stored_hurt_capsule *out_capsules,
    uint8_t capacity);

typedef int (*pf_ssbm_stored_runtime_case_runner)(
    void *context,
    const pf_ssbm_stored_case *stored_case);

/* Returns one for hit, zero for miss, and negative for invalid geometry. */
typedef int (*pf_ssbm_stored_geometry_case_runner)(
    void *context,
    const pf_ssbm_stored_case *stored_case);

typedef struct pf_ssbm_stored_oracle_domain
{
    const char *name;
    const pf_ssbm_stored_pose_track *pose_tracks;
    uint16_t pose_track_count;
    const pf_ssbm_stored_case *cases;
    uint16_t case_count;
    uint16_t expected_pose_count;
    uint8_t expected_capsules_per_pose;
    const char *expected_production_pose_sha256;
    void *context;
    pf_ssbm_stored_pose_reader read_pose;
    pf_ssbm_stored_runtime_case_runner run_runtime_case;
    pf_ssbm_stored_geometry_case_runner run_geometry_case;
} pf_ssbm_stored_oracle_domain;

typedef struct pf_ssbm_stored_oracle_result
{
    char production_pose_sha256[65];
    const char *failed_operation;
    const char *failed_case;
} pf_ssbm_stored_oracle_result;

typedef struct pf_ssbm_stored_trace_input
{
    int16_t main_stick_x;
    int16_t main_stick_y;
    int16_t secondary_stick_x;
    int16_t secondary_stick_y;
    uint64_t buttons;
    uint16_t left_trigger;
    uint16_t right_trigger;
    uint16_t advance_ticks;
} pf_ssbm_stored_trace_input;

typedef struct pf_ssbm_stored_trace_sample
{
    int32_t position_x_q16;
    int32_t position_y_q16;
    int32_t self_velocity_x_q16;
    int32_t self_velocity_y_q16;
    int32_t knockback_velocity_x_q16;
    int32_t knockback_velocity_y_q16;
    int32_t ground_knockback_velocity_q16;
    uint32_t damage_q16;
    uint16_t action_ticks;
    uint16_t hitlag_ticks;
    uint16_t hitstun_ticks;
    uint8_t action_state;
    uint8_t hitlag_resume_action;
    uint8_t grounded;
    uint8_t tumble;
    uint8_t invulnerable;
    int8_t tech_direction;
    uint8_t prone_orientation;
    int8_t facing;
} pf_ssbm_stored_trace_sample;

typedef enum pf_ssbm_stored_trace_field
{
    PF_SSBM_TRACE_POSITION_X = UINT32_C(1) << 0,
    PF_SSBM_TRACE_POSITION_Y = UINT32_C(1) << 1,
    PF_SSBM_TRACE_SELF_VELOCITY_X = UINT32_C(1) << 2,
    PF_SSBM_TRACE_SELF_VELOCITY_Y = UINT32_C(1) << 3,
    PF_SSBM_TRACE_KNOCKBACK_VELOCITY_X = UINT32_C(1) << 4,
    PF_SSBM_TRACE_KNOCKBACK_VELOCITY_Y = UINT32_C(1) << 5,
    PF_SSBM_TRACE_GROUND_KNOCKBACK_VELOCITY = UINT32_C(1) << 6,
    PF_SSBM_TRACE_DAMAGE = UINT32_C(1) << 7,
    PF_SSBM_TRACE_ACTION_TICKS = UINT32_C(1) << 8,
    PF_SSBM_TRACE_HITLAG_TICKS = UINT32_C(1) << 9,
    PF_SSBM_TRACE_HITSTUN_TICKS = UINT32_C(1) << 10,
    PF_SSBM_TRACE_ACTION_STATE = UINT32_C(1) << 11,
    PF_SSBM_TRACE_HITLAG_RESUME_ACTION = UINT32_C(1) << 12,
    PF_SSBM_TRACE_GROUNDED = UINT32_C(1) << 13,
    PF_SSBM_TRACE_TUMBLE = UINT32_C(1) << 14,
    PF_SSBM_TRACE_INVULNERABLE = UINT32_C(1) << 15,
    PF_SSBM_TRACE_TECH_DIRECTION = UINT32_C(1) << 16,
    PF_SSBM_TRACE_PRONE_ORIENTATION = UINT32_C(1) << 17,
    PF_SSBM_TRACE_FACING = UINT32_C(1) << 18
} pf_ssbm_stored_trace_field;

#define PF_SSBM_STORED_TRACE_FIELDS_V1 \
    (PF_SSBM_TRACE_POSITION_X | PF_SSBM_TRACE_POSITION_Y | \
     PF_SSBM_TRACE_SELF_VELOCITY_X | PF_SSBM_TRACE_SELF_VELOCITY_Y | \
     PF_SSBM_TRACE_KNOCKBACK_VELOCITY_X | \
     PF_SSBM_TRACE_KNOCKBACK_VELOCITY_Y | \
     PF_SSBM_TRACE_GROUND_KNOCKBACK_VELOCITY | PF_SSBM_TRACE_DAMAGE | \
     PF_SSBM_TRACE_ACTION_TICKS | PF_SSBM_TRACE_HITLAG_TICKS | \
     PF_SSBM_TRACE_HITSTUN_TICKS | PF_SSBM_TRACE_ACTION_STATE | \
     PF_SSBM_TRACE_HITLAG_RESUME_ACTION | PF_SSBM_TRACE_GROUNDED | \
     PF_SSBM_TRACE_TUMBLE | PF_SSBM_TRACE_INVULNERABLE | \
     PF_SSBM_TRACE_TECH_DIRECTION | PF_SSBM_TRACE_PRONE_ORIENTATION)
#define PF_SSBM_STORED_TRACE_FIELDS_ALL \
    (PF_SSBM_STORED_TRACE_FIELDS_V1 | PF_SSBM_TRACE_FACING)

typedef struct pf_ssbm_stored_trace_case
{
    const char *id;
    const pf_ssbm_stored_trace_input *inputs;
    /* Domain-defined, allocation-free initial-state selector. Zero preserves
       the historical/default setup for existing generated domains. */
    uint8_t initial_state_variant;
    /* Optional exact source-facing override after domain setup. */
    int8_t initial_facing;
} pf_ssbm_stored_trace_case;

typedef uint8_t (*pf_ssbm_stored_trace_case_runner)(
    void *context,
    const pf_ssbm_stored_trace_case *stored_case,
    pf_ssbm_stored_trace_sample *out_samples,
    uint8_t capacity);

typedef struct pf_ssbm_stored_trace_domain
{
    const char *name;
    const pf_ssbm_stored_trace_case *cases;
    uint16_t case_count;
    uint8_t samples_per_case;
    uint8_t lanes_per_sample;
    uint32_t serialized_fields;
    const char *expected_production_trace_sha256;
    void *context;
    pf_ssbm_stored_trace_case_runner run_case;
} pf_ssbm_stored_trace_domain;

typedef struct pf_ssbm_stored_trace_result
{
    char production_trace_sha256[65];
    const char *failed_operation;
    const char *failed_case;
} pf_ssbm_stored_trace_result;

int pf_ssbm_stored_oracle_run(
    const pf_ssbm_stored_oracle_domain *domain,
    pf_ssbm_stored_oracle_result *out_result);

int pf_ssbm_stored_trace_oracle_run(
    const pf_ssbm_stored_trace_domain *domain,
    pf_ssbm_stored_trace_result *out_result);

#endif
