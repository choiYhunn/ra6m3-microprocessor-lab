#ifndef PORTFOLIO_TCU_H
#define PORTFOLIO_TCU_H

#include <stdbool.h>
#include <stdint.h>

typedef enum { LEVER_P = 0, LEVER_N, LEVER_D, LEVER_R } lever_state_t;
typedef enum { MODE_AUTO = 0, MODE_MANUAL } drive_mode_t;

typedef struct {
    lever_state_t lever;
    drive_mode_t mode;
    uint8_t gear;
    bool engine_error;
} tcu_state_t;

void tcu_init(tcu_state_t *state);
void tcu_cycle_lever(tcu_state_t *state);
void tcu_toggle_mode(tcu_state_t *state);
void tcu_manual_up(tcu_state_t *state, float throttle_percent);
void tcu_manual_down(tcu_state_t *state, float throttle_percent);
void tcu_auto_shift(tcu_state_t *state, float throttle_percent);

#endif
