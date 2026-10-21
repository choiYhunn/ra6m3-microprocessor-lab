#ifndef FND_H
#define FND_H
#include "hal_data.h"
#include "tcu.h"

void fnd_reset(void);
void fnd_show(uint8_t digit, uint8_t data);
void fnd_update(tcu_state_t *s);

#endif
