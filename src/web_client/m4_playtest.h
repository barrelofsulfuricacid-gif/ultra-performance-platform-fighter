#ifndef PF_WEB_M4_PLAYTEST_H
#define PF_WEB_M4_PLAYTEST_H

int pf_web_m4_playtest_start(void);

int pf_web_m4_playtest_step(
    int player0_x,
    int player0_y,
    int player0_jump,
    int player0_attack,
    int player0_strong_attack,
    int player0_shield,
    int player1_x,
    int player1_y,
    int player1_jump,
    int player1_attack,
    int player1_strong_attack,
    int player1_shield);

int pf_web_m4_playtest_step_special(
    int player0_x,
    int player0_y,
    int player0_jump,
    int player0_attack,
    int player0_strong_attack,
    int player0_shield,
    int player1_x,
    int player1_y,
    int player1_jump,
    int player1_attack,
    int player1_strong_attack,
    int player1_shield,
    int player0_special,
    int player1_special,
    int player0_taunt,
    int player1_taunt);

int pf_web_m4_playtest_reset(void);

int pf_web_m4_playtest_refresh(void);

int pf_web_m4_playtest_set_team_lab(int enabled);

#endif
