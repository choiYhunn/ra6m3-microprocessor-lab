#ifndef MOTOR_H
#define MOTOR_H
#include "hal_data.h"
#include "tcu.h"

void motor_init(void);
void motor_update(tcu_state_t *s, float tps);
void motor_stop(void);

#endif
