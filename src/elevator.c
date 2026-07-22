#include "elevator.h"

#include <stddef.h>

enum {
    FLOOR_MIN = 1,
    FLOOR_MAX = 3,
    FLOOR_TRAVEL_MS = 3000,
    ARRIVAL_WAIT_MS = 1000,
    DOOR_OPEN_MS = 2000,
    DOOR_CLOSE_MS = 1000,
};

static bool floor_valid(uint8_t floor) {
    return floor >= FLOOR_MIN && floor <= FLOOR_MAX;
}
static bool has_pending(const elevator_t *elevator) {
    for (uint8_t floor = FLOOR_MIN; floor <= FLOOR_MAX; ++floor) {
        if (elevator->pending[floor]) return true;
    }
    return false;
}

static uint8_t nearest_pending(const elevator_t *elevator) {
    uint8_t best = 0;
    uint8_t best_distance = UINT8_MAX;
    for (uint8_t floor = FLOOR_MIN; floor <= FLOOR_MAX; ++floor) {
        if (!elevator->pending[floor]) continue;
        uint8_t distance = floor > elevator->current_floor
            ? floor - elevator->current_floor
            : elevator->current_floor - floor;
        if (distance < best_distance) {
            best = floor;
            best_distance = distance;
        }
    }
    return best;
}

static uint8_t direction_priority_pending(const elevator_t *elevator) {
    if (elevator->direction == DIRECTION_UP) {
        for (uint8_t floor = elevator->current_floor + 1; floor <= FLOOR_MAX; ++floor) {
            if (elevator->pending[floor]) return floor;
        }
        for (int floor = (int)elevator->current_floor - 1; floor >= FLOOR_MIN; --floor) {
            if (elevator->pending[floor]) return (uint8_t)floor;
        }
    } else if (elevator->direction == DIRECTION_DOWN) {
        for (int floor = (int)elevator->current_floor - 1; floor >= FLOOR_MIN; --floor) {
            if (elevator->pending[floor]) return (uint8_t)floor;
        }
        for (uint8_t floor = elevator->current_floor + 1; floor <= FLOOR_MAX; ++floor) {
            if (elevator->pending[floor]) return floor;
        }
    }
    return nearest_pending(elevator);
}

static void begin_trip(elevator_t *elevator, uint8_t floor) {
    elevator->goal_floor = floor;
    elevator->direction = floor > elevator->current_floor ? DIRECTION_UP : DIRECTION_DOWN;
    elevator->state = ELEVATOR_MOVING;
    elevator->state_elapsed_ms = 0;
    elevator->travel_elapsed_ms = 0;
}

static void begin_arrival(elevator_t *elevator) {
    elevator->goal_floor = elevator->current_floor;
    elevator->state = ELEVATOR_ARRIVED;
    elevator->state_elapsed_ms = 0;
    elevator->travel_elapsed_ms = 0;
}

static void dispatch(elevator_t *elevator, bool keep_direction) {
    if (!has_pending(elevator)) {
        elevator->direction = DIRECTION_NONE;
        return;
    }
    if (elevator->pending[elevator->current_floor]) {
        begin_arrival(elevator);
        return;
    }
    uint8_t next = keep_direction
        ? direction_priority_pending(elevator)
        : nearest_pending(elevator);
    if (next != 0) begin_trip(elevator, next);
}

void elevator_init(elevator_t *elevator, uint8_t initial_floor) {
    uint8_t floor = floor_valid(initial_floor) ? initial_floor : FLOOR_MIN;
    *elevator = (elevator_t){
        .state = ELEVATOR_IDLE,
        .current_floor = floor,
        .goal_floor = floor,
        .direction = DIRECTION_NONE,
    };
}

bool elevator_request(elevator_t *elevator, uint8_t floor) {
    if (!floor_valid(floor) || elevator->pending[floor]) return false;
    elevator->pending[floor] = true;

    if (elevator->state == ELEVATOR_IDLE) {
        dispatch(elevator, false);
    } else if (elevator->state == ELEVATOR_MOVING) {
        bool on_up_route = elevator->direction == DIRECTION_UP
            && floor > elevator->current_floor && floor < elevator->goal_floor;
        bool on_down_route = elevator->direction == DIRECTION_DOWN
            && floor < elevator->current_floor && floor > elevator->goal_floor;
        if (on_up_route || on_down_route) elevator->goal_floor = floor;
    }
    return true;
}

void elevator_request_door_close(elevator_t *elevator) {
    if (elevator->state == ELEVATOR_DOOR_OPEN) {
        elevator->door_close_requested = true;
    }
}

void elevator_tick(elevator_t *elevator, uint32_t elapsed_ms, bool arrival_audio_done) {
    elevator->state_elapsed_ms += elapsed_ms;

    switch (elevator->state) {
        case ELEVATOR_IDLE:
            dispatch(elevator, false);
            break;
        case ELEVATOR_MOVING:
            elevator->travel_elapsed_ms += elapsed_ms;
            while (elevator->travel_elapsed_ms >= FLOOR_TRAVEL_MS
                    && elevator->state == ELEVATOR_MOVING) {
                elevator->travel_elapsed_ms -= FLOOR_TRAVEL_MS;
                elevator->current_floor = (uint8_t)((int)elevator->current_floor + elevator->direction);
                if (elevator->current_floor == elevator->goal_floor) begin_arrival(elevator);
            }
            break;
        case ELEVATOR_ARRIVED:
            if (arrival_audio_done && elevator->state_elapsed_ms >= ARRIVAL_WAIT_MS) {
                elevator->state = ELEVATOR_DOOR_OPEN;
                elevator->state_elapsed_ms = 0;
            }
            break;
        case ELEVATOR_DOOR_OPEN:
            if (elevator->door_close_requested || elevator->state_elapsed_ms >= DOOR_OPEN_MS) {
                elevator->door_close_requested = false;
                elevator->pending[elevator->goal_floor] = false;
                elevator->state = ELEVATOR_DOOR_CLOSE;
                elevator->state_elapsed_ms = 0;
            }
            break;
        case ELEVATOR_DOOR_CLOSE:
            if (elevator->state_elapsed_ms >= DOOR_CLOSE_MS) {
                elevator->state = ELEVATOR_IDLE;
                elevator->state_elapsed_ms = 0;
                dispatch(elevator, true);
            }
            break;
    }
}
