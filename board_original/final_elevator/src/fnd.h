#ifndef FND_H
#define FND_H

#include "hal_data.h"
#include <stdint.h>

void fnd_init(void);
void fnd_update(void);          // 메인 루프에서 매 사이클 호출
void fnd_set_current(uint8_t floor);  // Digit2 갱신
void fnd_set_goal(uint8_t floor);     // Digit3 갱신
void fnd_motor_running(bool on);      // DC모터 동작 여부 (Digit1/4 애니메이션)

#endif /* FND_H */
