#ifndef PF_WEB_M4_PLAYTEST_H
#define PF_WEB_M4_PLAYTEST_H

int pf_web_m4_playtest_start(void);

int pf_web_m4_playtest_step(
    int player0_x,
    int player0_y,
    int player0_jump,
    int player1_x,
    int player1_y,
    int player1_jump);

int pf_web_m4_playtest_reset(void);

#endif
