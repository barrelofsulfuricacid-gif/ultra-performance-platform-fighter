#ifndef PF_REPLAY_H
#define PF_REPLAY_H

#include "pf/sim.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define PF_REPLAY_SCHEMA_VERSION UINT16_C(3)
#define PF_REPLAY_FORMAT_VERSION UINT16_C(1)
#define PF_REPLAY_VERIFICATION_SCHEMA_VERSION UINT16_C(2)
#define PF_REPLAY_OBSERVER_SCHEMA_VERSION UINT16_C(1)
#define PF_REPLAY_FLAG_PER_TICK_HASHES UINT16_C(1)

typedef struct pf_replay_source
{
    uint32_t struct_size;
    uint16_t schema_version;
    uint16_t flags;
    const pf_sim *initial_state;
    const pf_input_frame *input_frames;
    size_t input_frame_count;
    const pf_state_hash *state_hashes;
    size_t state_hash_count;
    uint64_t tick_count;
    pf_tick_result final_result;
} pf_replay_source;

typedef struct pf_replay_verification
{
    uint32_t struct_size;
    uint16_t schema_version;
    uint16_t reserved;
    uint32_t status;
    uint32_t reserved2;
    uint64_t expected_ticks;
    uint64_t verified_ticks;
    uint64_t first_mismatch_tick;
    pf_tick_result actual_result;
    pf_state_hash expected_hash;
    pf_state_hash actual_hash;
} pf_replay_verification;

typedef pf_status (*pf_replay_checkpoint_callback)(
    void *user_data,
    const pf_sim *sim,
    uint64_t replay_tick_count,
    const pf_tick_result *tick_result,
    const pf_state_hash *state_hash);

typedef struct pf_replay_observer
{
    uint32_t struct_size;
    uint16_t schema_version;
    uint16_t reserved;
    pf_replay_checkpoint_callback checkpoint;
    void *user_data;
} pf_replay_observer;

pf_status pf_replay_query_size(
    const pf_replay_source *source,
    size_t *out_replay_bytes);

pf_status pf_replay_encode(
    const pf_replay_source *source,
    pf_mut_bytes *destination);

pf_status pf_replay_verify(
    pf_sim *sim,
    pf_bytes replay,
    pf_replay_verification *out_verification);

pf_status pf_replay_verify_observed(
    pf_sim *sim,
    pf_bytes replay,
    const pf_replay_observer *observer,
    pf_replay_verification *out_verification);

#ifdef __cplusplus
}
#endif

#endif
