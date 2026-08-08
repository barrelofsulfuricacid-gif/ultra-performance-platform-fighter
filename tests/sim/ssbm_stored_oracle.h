#ifndef PF_TESTS_SIM_SSBM_STORED_ORACLE_H
#define PF_TESTS_SIM_SSBM_STORED_ORACLE_H

#include <stdint.h>

#define PF_SSBM_STORED_MAX_CAPSULES UINT8_C(32)
#define PF_SSBM_STORED_MAX_TRACE_SAMPLES UINT8_C(64)

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
} pf_ssbm_stored_trace_sample;

typedef struct pf_ssbm_stored_trace_case
{
    const char *id;
    const pf_ssbm_stored_trace_input *inputs;
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
