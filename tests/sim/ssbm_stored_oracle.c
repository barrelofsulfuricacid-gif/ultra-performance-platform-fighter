#include "ssbm_stored_oracle.h"

#include "../../src/sim/sim_sha256.h"

#include <stddef.h>
#include <string.h>

static void hash_u8(pf_sha256 *hash, uint8_t value)
{
    pf_sha256_update(hash, &value, sizeof(value));
}

static void hash_u16_le(pf_sha256 *hash, uint16_t value)
{
    const uint8_t bytes[2] = {
        (uint8_t)value,
        (uint8_t)(value >> 8)};

    pf_sha256_update(hash, bytes, sizeof(bytes));
}

static void hash_i32_le(pf_sha256 *hash, int32_t value)
{
    const uint32_t bits = (uint32_t)value;
    const uint8_t bytes[4] = {
        (uint8_t)bits,
        (uint8_t)(bits >> 8),
        (uint8_t)(bits >> 16),
        (uint8_t)(bits >> 24)};

    pf_sha256_update(hash, bytes, sizeof(bytes));
}

static void digest_hex(const uint8_t digest[32], char out_hex[65])
{
    static const char digits[] = "0123456789abcdef";
    size_t index;

    for (index = 0; index < (size_t)32; ++index)
    {
        out_hex[index * (size_t)2] = digits[digest[index] >> 4];
        out_hex[index * (size_t)2 + (size_t)1] =
            digits[digest[index] & UINT8_C(15)];
    }
    out_hex[64] = '\0';
}

static int production_digest(
    const pf_ssbm_stored_oracle_domain *domain,
    pf_ssbm_stored_oracle_result *result)
{
    pf_ssbm_stored_hurt_capsule capsules[PF_SSBM_STORED_MAX_CAPSULES];
    pf_sha256 hash;
    uint8_t digest[32];
    uint16_t pose_count = UINT16_C(0);
    uint16_t track_index;

    pf_sha256_init(&hash);
    for (track_index = UINT16_C(0);
         track_index < domain->pose_track_count;
         ++track_index)
    {
        const pf_ssbm_stored_pose_track *track =
            &domain->pose_tracks[track_index];
        uint32_t source_frame;
        uint16_t action_frame = UINT16_C(1);

        for (source_frame = track->first_source_frame;
             source_frame <= track->last_source_frame;
             source_frame += track->source_frame_step, ++action_frame)
        {
            const uint8_t capsule_count = domain->read_pose(
                domain->context,
                track->action_state,
                action_frame,
                capsules,
                PF_SSBM_STORED_MAX_CAPSULES);
            uint8_t capsule_index;

            if (capsule_count != domain->expected_capsules_per_pose)
            {
                result->failed_operation = "pose-span";
                return 0;
            }
            pf_sha256_update(
                &hash,
                (const uint8_t *)track->action_name,
                strlen(track->action_name) + (size_t)1);
            hash_u16_le(&hash, (uint16_t)source_frame);
            hash_u8(&hash, capsule_count);
            for (capsule_index = UINT8_C(0);
                 capsule_index < capsule_count;
                 ++capsule_index)
            {
                const pf_ssbm_stored_hurt_capsule *capsule =
                    &capsules[capsule_index];

                hash_i32_le(&hash, capsule->endpoint_a_x_q16);
                hash_i32_le(&hash, capsule->endpoint_a_y_q16);
                hash_i32_le(&hash, capsule->endpoint_a_z_q16);
                hash_i32_le(&hash, capsule->endpoint_b_x_q16);
                hash_i32_le(&hash, capsule->endpoint_b_y_q16);
                hash_i32_le(&hash, capsule->endpoint_b_z_q16);
                hash_i32_le(&hash, capsule->radius_q16);
                hash_u8(&hash, capsule->hurtbox_id);
                hash_u8(&hash, capsule->height);
                hash_u8(&hash, capsule->grabbable);
                hash_u8(&hash, capsule->reserved);
            }
            ++pose_count;
        }
    }
    if (pose_count != domain->expected_pose_count)
    {
        result->failed_operation = "pose-count";
        return 0;
    }
    pf_sha256_finish(&hash, digest);
    digest_hex(digest, result->production_pose_sha256);
    if (strcmp(
            result->production_pose_sha256,
            domain->expected_production_pose_sha256) != 0)
    {
        result->failed_operation = "production-pose-digest";
        return 0;
    }
    return 1;
}

static int source_frame_matches(
    const pf_ssbm_stored_oracle_domain *domain,
    const pf_ssbm_stored_case *stored_case)
{
    uint16_t track_index;

    for (track_index = UINT16_C(0);
         track_index < domain->pose_track_count;
         ++track_index)
    {
        const pf_ssbm_stored_pose_track *track =
            &domain->pose_tracks[track_index];

        if (track->action_state == stored_case->target_action)
        {
            const uint32_t source_frame =
                (uint32_t)track->first_source_frame +
                ((uint32_t)stored_case->action_frame - UINT32_C(1)) *
                    (uint32_t)track->source_frame_step;

            return source_frame == stored_case->source_frame &&
                   source_frame <= track->last_source_frame;
        }
    }
    return 0;
}

int pf_ssbm_stored_oracle_run(
    const pf_ssbm_stored_oracle_domain *domain,
    pf_ssbm_stored_oracle_result *out_result)
{
    uint16_t case_index;

    if (domain == NULL || out_result == NULL)
    {
        return 0;
    }
    memset(out_result, 0, sizeof(*out_result));
    if (domain->name == NULL || domain->pose_tracks == NULL ||
        domain->cases == NULL || domain->expected_capsules_per_pose == 0 ||
        domain->expected_capsules_per_pose > PF_SSBM_STORED_MAX_CAPSULES ||
        domain->expected_production_pose_sha256 == NULL ||
        domain->read_pose == NULL || domain->run_runtime_case == NULL ||
        domain->run_geometry_case == NULL)
    {
        out_result->failed_operation = "invalid-domain";
        return 0;
    }
    if (!production_digest(domain, out_result))
    {
        return 0;
    }
    for (case_index = UINT16_C(0);
         case_index < domain->case_count;
         ++case_index)
    {
        const pf_ssbm_stored_case *stored_case =
            &domain->cases[case_index];
        int passed;

        if (stored_case->mode == PF_SSBM_STORED_RUNTIME)
        {
            passed = domain->run_runtime_case(
                domain->context,
                stored_case);
        }
        else if (stored_case->mode == PF_SSBM_STORED_GEOMETRY)
        {
            const int actual_hit = domain->run_geometry_case(
                domain->context,
                stored_case);

            passed = actual_hit >= 0 &&
                     source_frame_matches(domain, stored_case) &&
                     actual_hit == (stored_case->expect_hit != UINT8_C(0));
        }
        else
        {
            passed = 0;
        }
        if (!passed)
        {
            out_result->failed_operation = "case";
            out_result->failed_case = stored_case->id;
            return 0;
        }
    }
    return 1;
}
