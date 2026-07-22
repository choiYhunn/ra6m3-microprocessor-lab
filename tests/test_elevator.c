#include "elevator.h"

#include <assert.h>
#include <stdio.h>

static void complete_door_cycle(elevator_t *elevator) {
    elevator_tick(elevator, 1000, true);
    assert(elevator->state == ELEVATOR_DOOR_OPEN);
    elevator_tick(elevator, 2000, true);
    assert(elevator->state == ELEVATOR_DOOR_CLOSE);
    elevator_tick(elevator, 1000, true);
}

int main(void) {
    elevator_t elevator;
    elevator_init(&elevator, 1);

    assert(elevator_request(&elevator, 3));
    assert(elevator.state == ELEVATOR_MOVING);
    assert(elevator.goal_floor == 3);

    elevator_tick(&elevator, 1000, false);
    assert(elevator_request(&elevator, 2));
    assert(elevator.goal_floor == 2);

    elevator_tick(&elevator, 2000, false);
    assert(elevator.current_floor == 2);
    assert(elevator.state == ELEVATOR_ARRIVED);
    complete_door_cycle(&elevator);
    assert(elevator.state == ELEVATOR_MOVING);
    assert(elevator.goal_floor == 3);

    elevator_tick(&elevator, 3000, false);
    assert(elevator.current_floor == 3);
    complete_door_cycle(&elevator);
    assert(elevator.state == ELEVATOR_IDLE);
    assert(elevator.direction == DIRECTION_NONE);

    assert(elevator_request(&elevator, 3));
    assert(elevator.state == ELEVATOR_ARRIVED);
    complete_door_cycle(&elevator);
    assert(elevator.state == ELEVATOR_IDLE);

    puts("Elevator tests passed");
    return 0;
}
