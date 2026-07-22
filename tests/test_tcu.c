#include "tcu.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    tcu_state_t state;
    tcu_init(&state);
    assert(state.lever == LEVER_P && state.gear == 1);

    tcu_auto_shift(&state, 100.0f);
    assert(state.gear == 1);

    tcu_cycle_lever(&state);
    tcu_cycle_lever(&state);
    assert(state.lever == LEVER_D);
    tcu_auto_shift(&state, 22.0f);
    assert(state.gear == 2);
    tcu_auto_shift(&state, 20.0f);
    assert(state.gear == 2);
    tcu_auto_shift(&state, 18.0f);
    assert(state.gear == 1);

    tcu_toggle_mode(&state);
    tcu_manual_up(&state, 21.0f);
    assert(state.gear == 2);
    tcu_manual_down(&state, 19.0f);
    assert(state.gear == 1);

    puts("TCU tests passed");
    return 0;
}
