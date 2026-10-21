#ifndef MOTOR_H
#define MOTOR_H

#include "hal_data.h"
#include <stdint.h>
#include "elevator.h"

void motor_init(void);
void dc_motor_start(Direction_t dir);  // CW(UP) or CCW(DOWN), 50% duty
void dc_motor_stop(void);
void servo_set_angle(uint8_t deg);     // 0° (닫힘) or 180° (열림)

#endif /* MOTOR_H */
