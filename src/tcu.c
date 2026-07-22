#include "tcu.h"

void tcu_init(tcu_state_t *state) {
    *state = (tcu_state_t){
        .lever = LEVER_P,
        .mode = MODE_AUTO,
        .gear = 1,
        .engine_error = false,
    };
}
void tcu_cycle_lever(tcu_state_t *state) {
    state->lever = (lever_state_t)((state->lever + 1) % 4);
    if (state->lever == LEVER_D || state->lever == LEVER_N) {
        state->gear = 1;
    }
}

void tcu_toggle_mode(tcu_state_t *state) {
    state->mode = state->mode == MODE_AUTO ? MODE_MANUAL : MODE_AUTO;
}

void tcu_manual_up(tcu_state_t *state, float throttle_percent) {
    static const float thresholds[] = {20.0f, 50.0f, 80.0f, 100.0f};
    if (state->mode != MODE_MANUAL || state->lever == LEVER_P) {
        return;
    }
    if (state->gear < 4 && throttle_percent > thresholds[state->gear - 1]) {
        ++state->gear;
    }
}

void tcu_manual_down(tcu_state_t *state, float throttle_percent) {
    static const float thresholds[] = {0.0f, 20.0f, 50.0f, 80.0f};
    if (state->mode != MODE_MANUAL || state->lever == LEVER_P) {
        return;
    }
    if (state->gear > 1 && throttle_percent < thresholds[state->gear - 1]) {
        --state->gear;
    }
}

void tcu_auto_shift(tcu_state_t *state, float throttle_percent) {
    if (state->mode != MODE_AUTO || state->lever == LEVER_P) {
        return;
    }

    switch (state->gear) {
        case 1:
            if (throttle_percent > 21.0f) state->gear = 2;
            break;
        case 2:
            if (throttle_percent > 51.0f) state->gear = 3;
            else if (throttle_percent < 19.0f) state->gear = 1;
            break;
        case 3:
            if (throttle_percent > 81.0f) state->gear = 4;
            else if (throttle_percent < 49.0f) state->gear = 2;
            break;
        case 4:
            if (throttle_percent < 79.0f) state->gear = 3;
            break;
        default:
            state->gear = 1;
            break;
    }
}
