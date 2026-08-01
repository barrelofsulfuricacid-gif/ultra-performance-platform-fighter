#ifndef PF_WEB_REPLAY_CHECKPOINT_H
#define PF_WEB_REPLAY_CHECKPOINT_H

#include <stddef.h>
#include <stdint.h>

int pf_web_run_replay_checkpoint(void);

int pf_web_replay_import(const uint8_t *replay_bytes, size_t replay_size);

#endif
