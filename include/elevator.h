#ifndef PORTFOLIO_ELEVATOR_H
#define PORTFOLIO_ELEVATOR_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    ELEVATOR_IDLE = 0,
    ELEVATOR_MOVING,
    ELEVATOR_ARRIVED,
    ELEVATOR_DOOR_OPEN,
    ELEVATOR_DOOR_CLOSE
} elevator_state_t;

typedef enum { DIRECTION_DOWN = -1, DIRECTION_NONE = 0, DIRECTION_UP = 1 } direction_t;

typedef struct {
    elevator_state_t state;
    uint8_t current_floor;
    uint8_t goal_floor;
    direction_t direction;
    bool pending[4];
    bool door_close_requested;
    uint32_t state_elapsed_ms;
    uint32_t travel_elapsed_ms;
} elevator_t;

void elevator_init(elevator_t *elevator, uint8_t initial_floor);
bool elevator_request(elevator_t *elevator, uint8_t floor);
void elevator_request_door_close(elevator_t *elevator);
void elevator_tick(elevator_t *elevator, uint32_t elapsed_ms, bool arrival_audio_done);

#endif
